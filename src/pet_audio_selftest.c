#include "pet_audio.h"
#include "pet_audio_pcm.h"
#include <string.h>
#define CHECK(x) do { if (!(x)) return false; } while (0)
static bool mix_modes(void)
{
    pa_mixer_t m = {0}, quiet = {0};
    int16_t a[160], b[160];
    pa_mix(&m, 2, false, a, 160);
    pa_mix(&quiet, 2, true, b, 160);
    for (unsigned i = 0; i < 160; i++) CHECK(b[i] == a[i] / 4);
    m.music_at = pa_music.count - 80;
    pa_mix(&m, 2, false, a, 160);
    CHECK(m.music_at == 80);
    CHECK(pa_music.data[0] == 0 && pa_music.data[pa_music.count-1] == 0);
    pa_trigger(&m, PA_TOUCH);
    pa_mix(&m, 0, false, a, 160);
    for (unsigned i = 0; i < 160; i++) CHECK(!a[i]);
    CHECK(m.cue == PA_NONE && m.music_at == 80);
    pa_trigger(&m, PA_NOTE0);
    pa_mix(&m, 1, false, a, 160);
    CHECK(m.music_at == 80 && m.cue_at == 160 && a[100] != 0);
    for (unsigned c = 1; c < PA_CUE_COUNT; c++) {
        CHECK(pa_cues[c].count > 0 && pa_cues[c].count < PA_SAMPLE_RATE);
        pa_trigger(&m, (pa_cue_t)c);
        for (unsigned i = 0; i < 60; i++) pa_mix(&m, 2, false, a, 160);
        CHECK(m.cue == PA_NONE);
    }
    CHECK(memcmp(pa_cues[PA_NOTE0].data, pa_cues[PA_NOTE1].data, 320) != 0);
    CHECK(memcmp(pa_cues[PA_NOTE1].data, pa_cues[PA_NOTE2].data, 320) != 0);
    CHECK(memcmp(pa_cues[PA_NOTE2].data, pa_cues[PA_NOTE3].data, 320) != 0);
    int music_peak = 0, cue_peak = 0;
    for (uint32_t i = 0; i < pa_music.count; i++) {
        int v = pa_music.data[i]; if (v < 0) v = -v;
        if (v > music_peak) music_peak = v;
    }
    for (unsigned c = 1; c < PA_CUE_COUNT; c++)
        for (uint32_t i = 0; i < pa_cues[c].count; i++) {
            int v = pa_cues[c].data[i]; if (v < 0) v = -v;
            if (v > cue_peak) cue_peak = v;
        }
    CHECK(music_peak > 0 && cue_peak > music_peak && music_peak + cue_peak < 32767);
    return true;
}
static bool event_once(void)
{
    pet_world_t w;
    pw_init(&w);
    pa_observer_t s;
    pa_observer_init(&s, &w);
    CHECK(pa_observe(&s, &w) == PA_NONE);
    CHECK(pw_request(&w, PW_TOUCH) == PW_OK);
    CHECK(pa_observe(&s, &w) == PA_TOUCH && pa_observe(&s, &w) == PA_NONE);
    CHECK(pw_request(&w, PW_TOUCH) == PW_BUSY && pa_observe(&s, &w) == PA_NONE);
    pw_advance(&w, 2000);
    w.journal_progress = 7;
    CHECK(pw_request(&w, PW_JOURNAL_CLAIM) == PW_OK && pa_observe(&s, &w) == PA_STAMP);
    pa_observer_init(&s, &w);
    CHECK(pa_observe(&s, &w) == PA_NONE);
    w.phase = PW_MINIGAME;
    pm_start(&w.memory, 3);
    CHECK(pa_observe(&s, &w) == (pa_cue_t)(PA_NOTE0 + w.memory.sequence[0]));
    CHECK(pa_observe(&s, &w) == PA_NONE);
    w.memory.elapsed = 750;
    CHECK(pa_observe(&s, &w) == (pa_cue_t)(PA_NOTE0 + w.memory.sequence[1]));
    w.memory.elapsed = 2000;
    CHECK(pa_observe(&s, &w) == PA_NONE);
    w.memory.phase = PM_LOST;
    CHECK(pa_observe(&s, &w) == PA_RETRY && pa_observe(&s, &w) == PA_NONE);
    return true;
}
bool pa_selftest(void (*report)(const char *, bool))
{
    bool mix = mix_modes(), events = event_once();
    if (report) {
        report("pcm-loop-modes-duck-and-four-notes", mix);
        report("world-events-once-no-replay", events);
    }
    return mix && events;
}
