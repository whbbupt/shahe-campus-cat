#include "pet_voice.h"
#include "pet_voice_pan.h"
#include "pet_voice_cloud.h"
#include "pet_audio_rt.h"
#include "rtthread.h"
#include "audio_server.h"
#include "opus.h"
#include "cJSON.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#define MIC_RATE 16000
#define MIC_SAMPLES 960
#define RECORD_SLOTS 8
#define OPUS_BYTES 1275
#define PCM_SAMPLES 5760

static struct rt_mutex view_lock;
static bool started;
static atomic_bool page, pressed, cancelled, retry, accept;
static atomic_uint page_epoch, level, record_read, record_write;
static unsigned session_epoch;
static atomic_bool record_overflow;
static pet_voice_view_t view;
static int16_t recorded[RECORD_SLOTS][MIC_SAMPLES];
static unsigned fill; /* Microphone callback only while the handle is open. */
static int16_t playback[PCM_SAMPLES];
static audio_client_t mic, speaker;
static OpusEncoder *encoder;
static OpusDecoder *decoder;
static unsigned playback_rate, sent_frames;
static bool connected, consumed, last_online, drain;
static rt_tick_t deadline;
static char session_id[64];
static pet_voice_state_t state;

static void publish(pet_voice_state_t value, const char *code)
{
    state = value;
    rt_mutex_take(&view_lock, RT_WAITING_FOREVER);
    view.state = value;
    snprintf(view.code, sizeof(view.code), "%s", code ? code : "");
    rt_mutex_release(&view_lock);
}

static rt_tick_t after(unsigned ms)
{
    return rt_tick_get() + rt_tick_from_millisecond(ms);
}

static bool due(rt_tick_t when)
{
    return (int32_t)(rt_tick_get() - when) >= 0;
}

static void close_audio(void)
{
    if (mic) { audio_close(mic); mic = NULL; }
    if (speaker) { audio_close(speaker); speaker = NULL; }
    if (encoder) { opus_encoder_destroy(encoder); encoder = NULL; }
    if (decoder) { opus_decoder_destroy(decoder); decoder = NULL; }
    drain = false;
    atomic_store(&level, 0);
}

static void disconnect(void)
{
    close_audio();
    if (connected) vc_close();
    connected = false;
    session_id[0] = 0;
}

static void failed(const char *reason)
{
    rt_kprintf("[pet:VOICE] %s\n", reason);
    disconnect();
    publish(PV_ERROR, NULL);
}

static int record_audio(audio_server_callback_cmt_t cmd, void *user, uint32_t arg)
{
    (void)user;
    if (cmd != as_callback_cmd_data_coming || !arg) return 0;
    const audio_server_coming_data_t *block = (const void *)(uintptr_t)arg;
    if (!block->data || !atomic_load(&pressed) || !atomic_load(&page)) return 0;
    unsigned maximum = 0;
    for (size_t i = 0; i + 1 < block->data_len; i += 2)
    {
        unsigned write = atomic_load_explicit(&record_write, memory_order_relaxed);
        unsigned read = atomic_load_explicit(&record_read, memory_order_acquire);
        if (write - read >= RECORD_SLOTS)
        {
            atomic_store(&record_overflow, true);
            return 0;
        }
        int sample = (int16_t)((unsigned)block->data[i] | (unsigned)block->data[i + 1] << 8);
        recorded[write % RECORD_SLOTS][fill++] = (int16_t)sample;
        unsigned amplitude = (unsigned)(sample < 0 ? -sample : sample);
        if (amplitude > maximum) maximum = amplitude;
        if (fill == MIC_SAMPLES)
        {
            fill = 0;
            atomic_store_explicit(&record_write, write + 1, memory_order_release);
        }
    }
    atomic_store(&level, maximum);
    return 0;
}

static bool flush_record(void)
{
    uint8_t packet[OPUS_BYTES];
    unsigned read = atomic_load_explicit(&record_read, memory_order_relaxed);
    unsigned write = atomic_load_explicit(&record_write, memory_order_acquire);
    if (atomic_load(&record_overflow)) return false;
    while (read != write)
    {
        int n = opus_encode(encoder, recorded[read % RECORD_SLOTS], MIC_SAMPLES, packet, sizeof(packet));
        if (n <= 0 || !vc_send(2, packet, (size_t)n)) return false;
        sent_frames++;
        atomic_store_explicit(&record_read, ++read, memory_order_release);
    }
    return true;
}

static bool send_control(const char *type, const char *fields)
{
    char text[160];
    int n = snprintf(text, sizeof(text), "{\"session_id\":\"%s\",\"type\":\"%s\"%s}",
                     session_id, type, fields);
    return n > 0 && (size_t)n < sizeof(text) && vc_send(1, text, (size_t)n);
}

static bool start_record(void)
{
    int error;
    encoder = opus_encoder_create(MIC_RATE, 1, OPUS_APPLICATION_VOIP, &error);
    if (!encoder) return false;
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(16000));
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(0));
    fill = sent_frames = 0;
    atomic_store(&record_read, 0);
    atomic_store(&record_write, 0);
    atomic_store(&record_overflow, false);
    if (!send_control("listen", ",\"state\":\"start\",\"mode\":\"manual\"")) return false;
    audio_parameter_t config = {0};
    config.read_samplerate = MIC_RATE;
    config.read_channnel_num = 1;
    config.read_bits_per_sample = 16;
    config.read_cache_size = 320;
    mic = audio_open2(AUDIO_TYPE_LOCAL_RECORD, AUDIO_RX, &config, record_audio, NULL, AUDIO_DEVICE_SPEAKER);
    if (!mic) return false;
    deadline = after(10000);
    publish(PV_RECORDING, NULL);
    return true;
}

static bool finish_record(void)
{
    audio_client_t capture = mic;
    mic = NULL;
    if (audio_close(capture)) return false;
    if (fill >= MIC_RATE / 100)
    {
        unsigned write = atomic_load(&record_write);
        if (write - atomic_load(&record_read) >= RECORD_SLOTS && !flush_record()) return false;
        memset(recorded[write % RECORD_SLOTS] + fill, 0, (MIC_SAMPLES - fill) * sizeof(int16_t));
        atomic_store(&record_write, write + 1);
    }
    if (!flush_record()) return false;
    opus_encoder_destroy(encoder);
    encoder = NULL;
    atomic_store(&level, 0);
    if (!sent_frames)
    {
        publish(PV_READY, NULL);
        return send_control("abort", "");
    }
    deadline = after(20000);
    publish(PV_WAITING, NULL);
    return send_control("listen", ",\"state\":\"stop\"");
}

static bool start_play(void)
{
    int error;
    decoder = opus_decoder_create((opus_int32)playback_rate, 1, &error);
    if (!decoder) return false;
    audio_parameter_t config = {0};
    config.write_samplerate = playback_rate;
    config.write_channnel_num = 1;
    config.write_bits_per_sample = 16;
    config.write_cache_size = playback_rate / 5;
    speaker = audio_open2(AUDIO_TYPE_LOCAL_MUSIC, AUDIO_TX, &config, NULL, NULL, AUDIO_DEVICE_SPEAKER);
    if (!speaker) return false;
    deadline = after(20000);
    publish(PV_PLAYING, NULL);
    return true;
}

static bool play(const uint8_t *data, size_t size)
{
    if (!speaker || !size || size > OPUS_BYTES) return false;
    int samples = opus_decode(decoder, data, (opus_int32)size, playback, PCM_SAMPLES, 0);
    if (samples <= 0) return false;
    unsigned maximum = 0;
    for (int i = 0; i < samples; i++)
    {
        int value = playback[i];
        unsigned amplitude = (unsigned)(value < 0 ? -value : value);
        if (amplitude > maximum) maximum = amplitude;
    }
    atomic_store(&level, maximum);
    size_t written = 0, bytes = (size_t)samples * sizeof(int16_t);
    rt_tick_t limit = after(450);
    while (written < bytes)
    {
        if (!atomic_load(&page) || atomic_load(&cancelled)) return true;
        int n = audio_write(speaker, (uint8_t *)playback + written, (uint32_t)(bytes - written));
        if (n < 0 || (size_t)n > bytes - written || due(limit)) return false;
        if (n) written += (size_t)n;
        else rt_thread_mdelay(5);
    }
    deadline = after(20000);
    return true;
}

static bool message(uint8_t opcode, const uint8_t *data, size_t len)
{
    if (opcode == 2)
        return state != PV_PLAYING || play(data, len);
    cJSON *json = cJSON_ParseWithLength((const char *)data, len);
    const char *type = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "type"));
    bool ok = json && type;
    if (ok && !strcmp(type, "hello") && state == PV_CONNECTING)
    {
        const char *id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "session_id"));
        const cJSON *params = cJSON_GetObjectItemCaseSensitive(json, "audio_params");
        const cJSON *rate = cJSON_GetObjectItemCaseSensitive(params, "sample_rate");
        const char *value = cJSON_GetStringValue(rate);
        unsigned hz = cJSON_IsNumber(rate) ? (unsigned)rate->valueint : 0;
        if (value)
        {
            if (!strcmp(value, "16000")) hz = 16000;
            else if (!strcmp(value, "24000")) hz = 24000;
            else if (!strcmp(value, "48000")) hz = 48000;
        }
        ok = id && *id && strlen(id) < sizeof(session_id) &&
            strspn(id, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == strlen(id) &&
            (hz == 16000 || hz == 24000 || hz == 48000);
        if (ok)
        {
            strcpy(session_id, id);
            playback_rate = hz;
            publish(PV_READY, NULL);
            rt_kprintf("[pet:VOICE] session ready, downlink %u Hz\n", hz);
        }
    }
    if (ok && !strcmp(type, "tts"))
    {
        const char *value = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "state"));
        if (value && !strcmp(value, "start") && state == PV_WAITING) ok = start_play();
        if (value && !strcmp(value, "stop") && state == PV_PLAYING)
        {
            drain = true;
            deadline = after(1200);
        }
    }
    if (ok && !strcmp(type, "goodbye")) ok = false;
    cJSON_Delete(json);
    return ok;
}

static bool session_active(void)
{
    return atomic_load(&page) && atomic_load(&page_epoch) == session_epoch;
}

static void worker(void *unused)
{
    (void)unused;
    unsigned epoch = 0;
    for (;;)
    {
        unsigned current_epoch = atomic_load(&page_epoch);
        bool open = atomic_load(&page);
        if (epoch != current_epoch)
        {
            epoch = current_epoch;
            disconnect();
            last_online = consumed = false;
            pet_audio_voice_pause(open);
            publish(open ? PV_NETWORK : PV_UNAVAILABLE, NULL);
        }
        pet_voice_pan_poll(open, atomic_exchange(&accept, false));
        bool again = atomic_exchange(&retry, false);
        bool held = atomic_load(&pressed);
        bool abort = atomic_exchange(&cancelled, false);
        if (!held) consumed = false;
        if (open)
        {
            uint32_t pairing_code;
            bool online = pet_voice_pan_online();
            if (pet_voice_pan_pairing(&pairing_code))
            {
                char code[16];
                snprintf(code, sizeof(code), "%06lu", (unsigned long)pairing_code);
                publish(PV_PAIR, code);
            }
            else if (!online)
            {
                if (connected) disconnect();
                publish(PV_NETWORK, NULL);
            }
            else if ((!last_online || again || state == PV_NETWORK) && !connected)
            {
                char code[16] = "";
                bool check_activation = state == PV_ACTIVATE && again;
                publish(PV_CONNECTING, NULL);
                session_epoch = epoch;
                vc_open_result_t opened = vc_open(code, check_activation, session_active);
                connected = opened == VC_CONNECTED;
                if (epoch != atomic_load(&page_epoch)) { disconnect(); continue; }
                if (opened == VC_ACTIVATION) publish(PV_ACTIVATE, code);
                else if (connected)
                {
                    static const char hello[] = "{\"type\":\"hello\",\"version\":1,\"transport\":\"websocket\","
                        "\"audio_params\":{\"format\":\"opus\",\"sample_rate\":16000,\"channels\":1,\"frame_duration\":60}}";
                    deadline = after(12000);
                    if (!vc_send(1, hello, sizeof(hello) - 1)) failed("hello send failed");
                }
                else failed("HTTPS discovery or verified WebSocket failed");
            }
            last_online = online;
            if (connected && abort && (state == PV_WAITING || state == PV_PLAYING || state == PV_RECORDING))
            {
                if (!send_control("abort", "")) failed("cancel send failed");
                else
                {
                    disconnect();
                    last_online = false;
                    publish(PV_NETWORK, NULL);
                }
            }
            if (connected && state == PV_READY && held && !consumed && pet_audio_voice_parked())
            {
                consumed = true;
                if (!start_record()) failed("record start failed");
            }
            if (connected && state == PV_RECORDING)
            {
                if (!flush_record()) failed("record buffer or send failed");
                else if ((!held || due(deadline)) && !finish_record()) failed("record finish failed");
            }
            for (unsigned i = 0; connected && i < 4; i++)
            {
                uint8_t opcode;
                const uint8_t *data;
                size_t size;
                int rc = vc_read(&opcode, &data, &size);
                if (rc < 0 || (rc == 1 && !message(opcode, data, size))) failed("cloud session closed");
                if (rc <= 0) break;
            }
            if (connected && drain)
            {
                uint32_t bytes = 0;
                if (due(deadline) || (audio_ioctl(speaker, AUDIO_IOCTL_BYTES_IN_CACHE, &bytes) == 0 && !bytes))
                {
                    close_audio();
                    publish(PV_READY, NULL);
                }
            }
            if (connected && !drain && state != PV_READY && state != PV_RECORDING && due(deadline))
                failed("voice response timeout");
        }
        rt_thread_mdelay(10);
    }
}

void pet_voice_start(void)
{
    if (rt_mutex_init(&view_lock, "vc_view", RT_IPC_FLAG_PRIO) != RT_EOK || !pet_voice_pan_init())
        return;
    rt_thread_t thread = rt_thread_create("pet_voice", worker, NULL, 10240, 20, 10);
    started = thread && rt_thread_startup(thread) == RT_EOK;
    if (!started && thread) rt_thread_delete(thread);
    rt_kprintf("[pet:VOICE] worker %s\n", started ? "ready" : "unavailable");
}

void pet_voice_page(bool open)
{
    atomic_store(&pressed, false);
    atomic_store(&cancelled, false);
    atomic_store(&retry, false);
    atomic_store(&accept, false);
    atomic_store(&page, open);
    atomic_fetch_add(&page_epoch, 1);
}

void pet_voice_hold(bool down) { atomic_store(&pressed, down); }
void pet_voice_cancel(void) { atomic_store(&cancelled, true); }
void pet_voice_retry(void) { atomic_store(&retry, true); }
void pet_voice_pair(void) { atomic_store(&accept, true); }

pet_voice_view_t pet_voice_view(void)
{
    if (!started) return (pet_voice_view_t){.state = PV_UNAVAILABLE};
    rt_mutex_take(&view_lock, RT_WAITING_FOREVER);
    pet_voice_view_t snapshot = view;
    rt_mutex_release(&view_lock);
    snapshot.level = atomic_load(&level);
    return snapshot;
}
