/* Xiaozhi device discovery and WebSocket protocol v1 over verified SDK TLS. */
#include "pet_voice_cloud.h"
#include "pet_version.h"
#include "rtthread.h"
#include "bf0_ble_common.h"
#include "cJSON.h"
#include "tls_client.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <errno.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DISCOVERY_HOST "api.tenclass.net"
#define HOST_MAX 96
#define PATH_MAX_LEN 224

static MbedTLSSession *tls;
static bool (*is_active)(void);
static uint8_t input[PET_VOICE_FRAME_MAX + 4], message[PET_VOICE_FRAME_MAX];
static uint8_t output[PET_VOICE_FRAME_MAX + 8];
static size_t input_size, message_size;
static uint8_t fragmented;

static bool retryable(int rc)
{
    return rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE ||
           rc == MBEDTLS_ERR_SSL_TIMEOUT;
}

static bool before(rt_tick_t deadline)
{
    return is_active && is_active() && (int32_t)(rt_tick_get() - deadline) < 0;
}

static bool send_bytes(const void *data, size_t size)
{
    const uint8_t *p = data;
    rt_tick_t deadline = rt_tick_get() + rt_tick_from_millisecond(2000);
    while (size && before(deadline))
    {
        int rc = mbedtls_ssl_write(&tls->ssl, p, size);
        if (rc > 0) { p += rc; size -= (size_t)rc; }
        else if (!retryable(rc)) return false;
        else rt_thread_mdelay(5);
    }
    return size == 0;
}

static bool identity(char mac[18], char id[37])
{
    bd_addr_t addr;
    unsigned char hash[32];
    if (ble_get_public_address(&addr) || mbedtls_sha256_ret(addr.addr, 6, hash, 0))
        return false;
    snprintf(mac, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             addr.addr[5], addr.addr[4], addr.addr[3], addr.addr[2], addr.addr[1], addr.addr[0]);
    hash[6] = (hash[6] & 15) | 0x40;
    hash[8] = (hash[8] & 63) | 0x80;
    char *p = id;
    for (unsigned i = 0; i < 16; i++)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10) *p++ = '-';
        snprintf(p, 3, "%02x", hash[i]);
        p += 2;
    }
    return true;
}

static bool printable(const char *text)
{
    for (; *text; text++)
        if ((unsigned char)*text <= 32 || (unsigned char)*text >= 127) return false;
    return true;
}

static bool trusted_host(const char *host)
{
    size_t n = strlen(host);
    const char *suffixes[] = {".tenclass.net", ".xiaozhi.me"};
    for (unsigned i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++)
    {
        size_t len = strlen(suffixes[i]);
        if (n > len && !strcmp(host + n - len, suffixes[i])) return true;
    }
    return false;
}

static bool endpoint(const char *url, char host[HOST_MAX], char path[PATH_MAX_LEN])
{
    if (strncmp(url, "wss://", 6) || !printable(url)) return false;
    const char *start = url + 6;
    const char *end = strchr(start, '/');
    if (!end || end == start || strlen(end) >= PATH_MAX_LEN) return false;
    size_t n = (size_t)(end - start);
    if (n >= 4 && !strncmp(end - 4, ":443", 4)) n -= 4;
    if (!n || n >= HOST_MAX) return false;
    for (size_t i = 0; i < n; i++)
        if (!(isalnum((unsigned char)start[i]) || start[i] == '.' || start[i] == '-')) return false;
    memcpy(host, start, n);
    host[n] = 0;
    if (!trusted_host(host)) return false;
    strcpy(path, end);
    return true;
}

static bool start_tls(const char *host, const char *phase);

static bool cloud_failure(const char *phase, const char *step, int code)
{
    rt_kprintf("[pet:VOICE] cloud phase=%s step=%s code=%d\n", phase, step, code);
    return false;
}

static int http_status(const char *headers)
{
    int status = 0;
    if ((!strncmp(headers, "HTTP/1.0 ", 9) || !strncmp(headers, "HTTP/1.1 ", 9)) &&
        headers[9] >= '0' && headers[9] <= '9' &&
        headers[10] >= '0' && headers[10] <= '9' &&
        headers[11] >= '0' && headers[11] <= '9' && headers[12] == ' ')
        status = (headers[9] - '0') * 100 + (headers[10] - '0') * 10 + headers[11] - '0';
    return status;
}

static bool read_headers(char *response, size_t capacity)
{
    size_t used = 0;
    rt_tick_t deadline = rt_tick_get() + rt_tick_from_millisecond(7000);
    while (used < capacity - 1 && before(deadline))
    {
        int rc = mbedtls_ssl_read(&tls->ssl, (unsigned char *)response + used, 1);
        if (retryable(rc)) { rt_thread_mdelay(5); continue; }
        if (rc != 1) return false;
        response[++used] = 0;
        if (used >= 4 && !memcmp(response + used - 4, "\r\n\r\n", 4)) return true;
    }
    return false;
}

static const char *header_value(const char *response, const char *name, size_t *len)
{
    const char *line = strstr(response, "\r\n");
    size_t key = strlen(name);
    while (line && line[2] != '\r')
    {
        line += 2;
        const char *end = strstr(line, "\r\n");
        if (!end) break;
        if ((size_t)(end - line) >= key && !strncasecmp(line, name, key))
        {
            line += key;
            while (line < end && (*line == ' ' || *line == '\t')) line++;
            while (end > line && (end[-1] == ' ' || end[-1] == '\t')) end--;
            *len = (size_t)(end - line);
            return line;
        }
        line = end;
    }
    return NULL;
}

static bool post(const char *path, const char *mac, const char *id, const char *body,
                 char *headers, size_t headers_size)
{
    char request[420];
    int n = snprintf(request, sizeof(request),
        "POST %s HTTP/1.0\r\nHost: " DISCOVERY_HOST "\r\n"
        "Device-Id: %s\r\nClient-Id: %s\r\nActivation-Version: 1\r\n"
        "User-Agent: ShaheCat/" PET_FIRMWARE_VERSION "\r\nAccept-Language: zh-CN\r\n"
        "Content-Type: application/json\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",
        path, mac, id, (unsigned)strlen(body));
    return n > 0 && (size_t)n < sizeof(request) && send_bytes(request, (size_t)n) &&
           send_bytes(body, strlen(body)) && read_headers(headers, headers_size);
}

static vc_open_result_t discover(const char *mac, const char *id, char activation[16],
                                  char host[HOST_MAX], char path[PATH_MAX_LEN], char token[512])
{
    char body[256];
    snprintf(body, sizeof(body),
        "{\"version\":2,\"mac_address\":\"%s\",\"uuid\":\"%s\","
        "\"application\":{\"name\":\"shahe-campus-cat\",\"version\":\"%s\"},"
        "\"board\":{\"type\":\"hdk563\",\"mac\":\"%s\"}}", mac, id, PET_FIRMWARE_VERSION, mac);
    char headers[1536];
    char *response = rt_malloc(PET_VOICE_FRAME_MAX + 1);
    cJSON *json = NULL;
    vc_open_result_t result = VC_ERROR;
    if (!response)
    {
        cloud_failure("ota", "allocate-response", 0);
        goto finish;
    }
    if (!start_tls(DISCOVERY_HOST, "ota")) goto finish;
    if (!post("/xiaozhi/ota/", mac, id, body, headers, sizeof(headers)))
    {
        cloud_failure("ota", "http-exchange", 0);
        goto finish;
    }
    int status = http_status(headers);
    if (status != 200)
    {
        cloud_failure("ota", "http-status", status);
        goto finish;
    }
    const char *response_error = "content-length";
    size_t field_length = 0, content_length = 0;
    const char *value = header_value(headers, "Content-Length:", &field_length);
    if (value)
    {
        if (!field_length || field_length > 4) goto invalid_response;
        for (size_t i = 0; i < field_length; i++)
        {
            if (value[i] < '0' || value[i] > '9') goto invalid_response;
            content_length = content_length * 10 + (unsigned)(value[i] - '0');
        }
        if (!content_length || content_length > PET_VOICE_FRAME_MAX) goto invalid_response;
    }
    response_error = "transfer-encoding";
    if (header_value(headers, "Transfer-Encoding:", &field_length)) goto invalid_response;
    response_error = "response-body";
    size_t used = 0;
    size_t limit = content_length ? content_length : PET_VOICE_FRAME_MAX;
    rt_tick_t deadline = rt_tick_get() + rt_tick_from_millisecond(7000);
    while (used < limit)
    {
        if (!before(deadline)) goto invalid_response;
        int rc = mbedtls_ssl_read(&tls->ssl, (unsigned char *)response + used, limit - used);
        if (retryable(rc)) { rt_thread_mdelay(5); continue; }
        if (!rc || rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) break;
        if (rc < 0)
        {
            cloud_failure("ota", response_error, rc);
            goto finish;
        }
        used += (size_t)rc;
    }
    if (!used || (content_length && used != content_length) || (!content_length && used == limit))
        goto invalid_response;
    response[used] = 0;
    json = cJSON_ParseWithLength(response, used + 1);
    response_error = "response-json";
    if (!json) goto invalid_response;
    const cJSON *binding = cJSON_GetObjectItemCaseSensitive(json, "activation");
    const char *code = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(binding, "code"));
    if (code && strlen(code) > 0 && strlen(code) < 16 && strspn(code, "0123456789") == strlen(code))
    {
        strcpy(activation, code);
        result = VC_ACTIVATION;
        goto finish;
    }
    const cJSON *ws = cJSON_GetObjectItemCaseSensitive(json, "websocket");
    const char *url = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(ws, "url"));
    const char *credential = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(ws, "token"));
    const char *value_start = credential && !strncmp(credential, "Bearer ", 7) ? credential + 7 : credential;
    response_error = "websocket-config";
    if (!url || !endpoint(url, host, path) ||
        (credential && (strlen(credential) >= 512 || !printable(value_start))))
        goto invalid_response;
    strcpy(token, credential ? credential : "");
    result = VC_CONNECTED;
    goto finish;
invalid_response:
    cloud_failure("ota", response_error, 0);
finish:
    vc_close();
    cJSON_Delete(json);
    rt_free(response);
    return result;
}

static int socket_read(void *context, unsigned char *data, size_t size)
{
    int fd = ((mbedtls_net_context *)context)->fd;
    int rc = lwip_recv(fd, data, size, 0);
    if (rc >= 0) return rc;
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return MBEDTLS_ERR_SSL_WANT_READ;
    return MBEDTLS_ERR_NET_RECV_FAILED;
}

static int socket_write(void *context, const unsigned char *data, size_t size)
{
    int fd = ((mbedtls_net_context *)context)->fd;
    int rc = lwip_send(fd, data, size, 0);
    if (rc >= 0) return rc;
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return MBEDTLS_ERR_SSL_WANT_WRITE;
    return MBEDTLS_ERR_NET_SEND_FAILED;
}

static bool socket_connect(const char *host, const char *phase)
{
    struct addrinfo hints = {0}, *addresses = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (!is_active()) return false;
    int dns_result = lwip_getaddrinfo(host, "443", &hints, &addresses);
    if (dns_result) return cloud_failure(phase, "dns", dns_result);
    rt_tick_t deadline = rt_tick_get() + rt_tick_from_millisecond(6000);
    int connect_error = 0;
    for (struct addrinfo *address = addresses; address && before(deadline); address = address->ai_next)
    {
        int fd = lwip_socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (fd < 0) { connect_error = errno; continue; }
        unsigned long nonblock = 1;
        if (lwip_ioctl(fd, FIONBIO, &nonblock) == 0)
        {
            int rc = lwip_connect(fd, address->ai_addr, address->ai_addrlen);
            connect_error = rc ? errno : 0;
            if (rc && (connect_error == EINPROGRESS || connect_error == EWOULDBLOCK))
            {
                while (before(deadline))
                {
                    struct timeval wait = {0, 100000};
                    fd_set writable;
                    FD_ZERO(&writable);
                    FD_SET(fd, &writable);
                    rc = lwip_select(fd + 1, NULL, &writable, NULL, &wait);
                    if (!rc) continue;
                    int error = 0;
                    socklen_t length = sizeof(error);
                    if (rc < 0 || lwip_getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length))
                    {
                        connect_error = errno;
                        rc = -1;
                    }
                    else
                    {
                        connect_error = error;
                        rc = error ? -1 : 0;
                    }
                    break;
                }
                if (!before(deadline))
                {
                    connect_error = ETIMEDOUT;
                    rc = -1;
                }
            }
            if (!rc)
            {
                tls->server_fd.fd = fd;
                break;
            }
        }
        else connect_error = errno;
        lwip_close(fd);
    }
    lwip_freeaddrinfo(addresses);
    if (tls->server_fd.fd >= 0) return true;
    return !is_active() ? false : cloud_failure(phase, "tcp", connect_error);
}

static bool start_tls(const char *host, const char *phase)
{
    tls = rt_calloc(1, sizeof(*tls));
    if (!tls) return cloud_failure(phase, "tls-allocate", 0);
    const char seed[] = "shahe-cat-voice";
    int rc = mbedtls_client_init(tls, (void *)seed, sizeof(seed));
    if (rc) return cloud_failure(phase, "tls-init", rc);
    tls->host = rt_strdup(host);
    if (!tls->host) return cloud_failure(phase, "tls-allocate-host", 0);
    rc = mbedtls_client_context(tls);
    if (rc) return cloud_failure(phase, "tls-config", rc);
    if (!socket_connect(host, phase)) return false;
    mbedtls_ssl_set_bio(&tls->ssl, &tls->server_fd, socket_write, socket_read, NULL);
    rt_tick_t deadline = rt_tick_get() + rt_tick_from_millisecond(12000);
    while (before(deadline))
    {
        rc = mbedtls_ssl_handshake(&tls->ssl);
        if (!rc)
        {
            uint32_t flags = mbedtls_ssl_get_verify_result(&tls->ssl);
            return flags == 0 || cloud_failure(phase, "tls-verify", (int)flags);
        }
        if (!retryable(rc))
        {
            uint32_t flags = mbedtls_ssl_get_verify_result(&tls->ssl);
            if (flags && flags != UINT32_MAX)
                cloud_failure(phase, "tls-verify", (int)flags);
            return cloud_failure(phase, "tls-handshake", rc);
        }
        rt_thread_mdelay(10);
    }
    return !is_active() ? false : cloud_failure(phase, "tls-timeout", rc);
}

static bool upgrade(const char *host, const char *path, const char *token, const char *mac, const char *id)
{
    unsigned char key_bytes[16], key[32] = {0}, digest[20], expected[32] = {0};
    size_t len;
    char check[72], request[1232], response[1536];
    if (mbedtls_ctr_drbg_random(&tls->ctr_drbg, key_bytes, sizeof(key_bytes)) ||
        mbedtls_base64_encode(key, sizeof(key), &len, key_bytes, sizeof(key_bytes)))
        return cloud_failure("websocket", "handshake-key", 0);
    snprintf(check, sizeof(check), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
    if (mbedtls_sha1_ret((const unsigned char *)check, strlen(check), digest) ||
        mbedtls_base64_encode(expected, sizeof(expected), &len, digest, sizeof(digest)))
        return cloud_failure("websocket", "handshake-key", 0);
    const char *prefix = *token ? "Authorization: " : "";
    const char *scheme = *token && strncmp(token, "Bearer ", 7) ? "Bearer " : "";
    int n = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: %s\r\n%s%s%s%s"
        "Protocol-Version: 1\r\nDevice-Id: %s\r\nClient-Id: %s\r\n\r\n",
        path, host, key, prefix, scheme, token, *token ? "\r\n" : "", mac, id);
    if (n <= 0 || (size_t)n >= sizeof(request) || !send_bytes(request, (size_t)n))
        return cloud_failure("websocket", "http-send", 0);
    if (!read_headers(response, sizeof(response)))
        return cloud_failure("websocket", "http-headers", 0);
    if (strncmp(response, "HTTP/1.1 101 ", 13))
        return cloud_failure("websocket", "http-status", http_status(response));
    size_t accepted_length = 0;
    const char *accepted = header_value(response, "Sec-WebSocket-Accept:", &accepted_length);
    if (accepted && accepted_length == len && !memcmp(accepted, expected, len)) return true;
    return cloud_failure("websocket", "accept-key", 0);
}

vc_open_result_t vc_open(char activation[16], bool check_activation, bool (*active)(void))
{
    char mac[18], id[37], host[HOST_MAX], path[PATH_MAX_LEN], token[512];
    vc_close();
    is_active = active;
    if (!identity(mac, id))
    {
        cloud_failure("ota", "identity", 0);
        return VC_ERROR;
    }
    if (check_activation)
    {
        char headers[1536];
        if (start_tls(DISCOVERY_HOST, "activation"))
        {
            if (!post("/xiaozhi/ota/activate", mac, id, "{}", headers, sizeof(headers)))
                cloud_failure("activation", "http-exchange", 0);
            else
                rt_kprintf("[pet:VOICE] cloud phase=activation http=%d\n", http_status(headers));
        }
        vc_close();
    }
    if (!active()) return VC_ERROR;
    vc_open_result_t result = discover(mac, id, activation, host, path, token);
    if (result != VC_CONNECTED) return result;
    if (start_tls(host, "websocket") && upgrade(host, path, token, mac, id)) return VC_CONNECTED;
    vc_close();
    return VC_ERROR;
}

bool vc_send(uint8_t opcode, const void *data, size_t size)
{
    uint8_t *frame = output;
    uint8_t mask[4];
    if (!tls || size > PET_VOICE_FRAME_MAX ||
        mbedtls_ctr_drbg_random(&tls->ctr_drbg, mask, sizeof(mask)))
        return false;
    size_t pos = 0;
    frame[pos++] = 0x80 | opcode;
    frame[pos++] = 0x80 | (uint8_t)(size < 126 ? size : 126);
    if (size >= 126)
    {
        frame[pos++] = (uint8_t)(size >> 8);
        frame[pos++] = (uint8_t)size;
    }
    memcpy(frame + pos, mask, sizeof(mask));
    pos += sizeof(mask);
    for (size_t i = 0; i < size; i++) frame[pos + i] = ((const uint8_t *)data)[i] ^ mask[i % 4];
    return send_bytes(frame, pos + size);
}

/* Returns 1 for one complete message, 0 when incomplete, -1 on close/error. */
int vc_read(uint8_t *opcode, const uint8_t **data, size_t *size)
{
    if (!tls) return -1;
    if (input_size < sizeof(input))
    {
        int rc = mbedtls_ssl_read(&tls->ssl, input + input_size, sizeof(input) - input_size);
        if (rc > 0) input_size += (size_t)rc;
        else if (!retryable(rc) && !input_size) return -1;
    }
    if (input_size < 2) return 0;
    uint8_t op = input[0] & 15;
    bool final = (input[0] & 0x80) != 0;
    size_t header = 2, len = input[1] & 127;
    if ((input[0] & 0x70) || (input[1] & 0x80) || len == 127) return -1;
    if (len == 126)
    {
        if (input_size < 4) return 0;
        header = 4;
        len = (size_t)input[2] * 256 + input[3];
    }
    if (len > PET_VOICE_FRAME_MAX) return -1;
    if (input_size < header + len) return 0;
    int result = 0;
    if (op >= 8)
    {
        if (!final || len > 125 || op == 8) return -1;
        if (op == 9 && !vc_send(10, input + header, len)) return -1;
        if (op != 9 && op != 10) return -1;
    }
    else
    {
        if (op)
        {
            if (fragmented || (op != 1 && op != 2)) return -1;
            fragmented = op;
            message_size = 0;
        }
        if (!fragmented || len > sizeof(message) - message_size) return -1;
        memcpy(message + message_size, input + header, len);
        message_size += len;
        if (final)
        {
            *opcode = fragmented;
            *data = message;
            *size = message_size;
            fragmented = 0;
            result = 1;
        }
    }
    input_size -= header + len;
    memmove(input, input + header + len, input_size);
    return result;
}

void vc_close(void)
{
    if (tls) mbedtls_client_close(tls);
    tls = NULL;
    input_size = message_size = fragmented = 0;
}
