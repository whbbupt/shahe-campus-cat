/* Only this worker calls audio_server; LVGL never waits on audio IO.
 * Playback/backpressure follows SDK audio_server.h and the existing lab player. */
#include "pet_audio_rt.h"
#include "pet_audio.h"
#include "pet_audio_pcm.h"
#include "rtthread.h"
#include "audio_server.h"
#include <string.h>
#define BLOCK_SAMPLES 160U
static struct rt_mutex lock;
static bool initialized, available, duck, worker_ready;
static unsigned mode, volume, generation;
static pa_cue_t pending;
static pa_observer_t observer;
bool pet_audio_available(void)
{
    if (!initialized) return false;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    bool value = available;
    rt_mutex_release(&lock);
    return value;
}
void pet_audio_sync(const pet_world_t *w)
{
    pa_cue_t cue = pa_observe(&observer, w);
    if (!initialized) return;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    if (mode != w->sound_mode)
    {
        mode = w->sound_mode; generation++; pending = PA_NONE;
        available = worker_ready;
        cue = PA_NONE;
    }
    if (volume != w->volume)
    {
        volume = w->volume;
        available = worker_ready;
        if (!volume) { pending = PA_NONE; generation++; cue = PA_NONE; }
    }
    if (observer.phase != PW_MINIGAME && duck) { generation++; pending = PA_NONE; }
    duck = w->phase == PW_MINIGAME;
    if (mode && volume && available && cue != PA_NONE) pending = cue;
    rt_mutex_release(&lock);
}
typedef struct { unsigned mode, token; bool duck, available; pa_cue_t cue; unsigned volume; } settings_t;
static settings_t settings(bool consume)
{
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    settings_t value = {mode, generation, duck, available, pending, volume};
    if (consume) pending = PA_NONE;
    rt_mutex_release(&lock);
    return value;
}
static void failed(void)
{
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    available = false; pending = PA_NONE;
    rt_mutex_release(&lock);
    rt_kprintf("[pet:AUDIO] unavailable; gameplay continues, change sound mode to retry\n");
}
static void worker(void *arg)
{
    (void)arg;
    pa_mixer_t mixer = {0};
    audio_client_t client = NULL;
    uint8_t old_volume = 0;
    unsigned client_volume = 0;
    bool volume_set = false;
    unsigned token = 0, tail = 4000;
    int16_t block[BLOCK_SAMPLES], writable[BLOCK_SAMPLES];
    for (;;)
    {
        settings_t s = settings(true);
        bool error = false;
        if (token != s.token)
        {
            if (client) { if (audio_close(client) != 0) error = true; client = NULL; }
            if (volume_set) {
                if (audio_server_set_private_volume(AUDIO_TYPE_LOCAL_MUSIC, old_volume) != 0) error = true;
                volume_set = false;
            }
            mixer.cue = PA_NONE; tail = 4000; token = s.token;
            if (error) { failed(); rt_thread_mdelay(10); continue; }
        }
        if (s.cue) pa_trigger(&mixer, s.cue);
        bool idle = !s.mode || !s.volume || !s.available || (s.mode == 1 && !mixer.cue && tail >= 4000);
        if (idle)
        {
            if (client) { if (audio_close(client) != 0) error = true; client = NULL; }
            if (volume_set) {
                if (audio_server_set_private_volume(AUDIO_TYPE_LOCAL_MUSIC, old_volume) != 0) error = true;
                volume_set = false;
            }
            if (error) failed();
            rt_thread_mdelay(10);
            continue;
        }
        if (!client)
        {
            old_volume = audio_server_get_private_volume(AUDIO_TYPE_LOCAL_MUSIC);
            volume_set = true;
            error = audio_server_set_private_volume(AUDIO_TYPE_LOCAL_MUSIC, (uint8_t)s.volume) != 0;
            client_volume = s.volume;
            audio_parameter_t p = {0};
            p.write_samplerate = PA_SAMPLE_RATE; p.write_channnel_num = 1;
            p.write_bits_per_sample = 16; p.write_cache_size = 1024;
            if (!error) client = audio_open2(AUDIO_TYPE_LOCAL_MUSIC, AUDIO_TX, &p, NULL, NULL, AUDIO_DEVICE_SPEAKER);
            if (!client) error = true;
        }
        else if (client_volume != s.volume)
        {
            error = audio_server_set_private_volume(AUDIO_TYPE_LOCAL_MUSIC, (uint8_t)s.volume) != 0;
            client_volume = s.volume;
        }
        bool had_cue = mixer.cue != PA_NONE;
        pa_mix(&mixer, s.mode, s.duck, block, BLOCK_SAMPLES);
        tail = had_cue || mixer.cue ? 0 : tail + BLOCK_SAMPLES;
        unsigned offset = 0;
        rt_tick_t progress = rt_tick_get();
        while (!error && offset < BLOCK_SAMPLES)
        {
            settings_t current = settings(false);
            if (current.token != token || !current.mode || !current.volume || !current.available) break;
            unsigned count = BLOCK_SAMPLES - offset;
            memcpy(writable, block + offset, count * sizeof(int16_t));
            int n = audio_write(client, (uint8_t *)writable, count * sizeof(int16_t));
            if (n < 0 || n > (int)(count * 2) || (n & 1)) { error = true; break; }
            if (n) { offset += (unsigned)n / 2; progress = rt_tick_get(); }
            else if ((rt_tick_t)(rt_tick_get() - progress) >= rt_tick_from_millisecond(300))
                error = true;
            rt_thread_mdelay(n ? 1 : 5);
        }
        if (error)
        {
            if (client) audio_close(client);
            client = NULL;
            if (volume_set) audio_server_set_private_volume(AUDIO_TYPE_LOCAL_MUSIC, old_volume);
            volume_set = false;
            mixer.cue = PA_NONE; tail = 4000;
            failed();
        }
    }
}
void pet_audio_start(const pet_world_t *w)
{
    pa_observer_init(&observer, w);
    if (initialized) { pet_audio_sync(w); return; }
    if (rt_mutex_init(&lock, "pet_sfx", RT_IPC_FLAG_PRIO) != RT_EOK)
    {
        rt_kprintf("[pet:AUDIO] lock unavailable\n");
        return;
    }
    initialized = available = true; mode = w->sound_mode; volume = w->volume; pending = PA_NONE;
    duck = false; generation = 0;
    rt_thread_t thread = rt_thread_create("pet_sfx", worker, NULL, 4096, 18, 10);
    worker_ready = thread != NULL;
    if (thread && rt_thread_startup(thread) == RT_EOK) {
        rt_kprintf("[pet:AUDIO] worker ready, mode=%u, 16000Hz mono PCM\n", mode);
        return;
    }
    if (thread) rt_thread_delete(thread);
    worker_ready = available = false;
    rt_kprintf("[pet:AUDIO] worker unavailable; gameplay continues\n");
}
