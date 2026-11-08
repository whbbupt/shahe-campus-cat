#include "pet_audio.h"
#include "pet_audio_pcm.h"
#include <string.h>
static int show_step(const pet_world_t *w)
{
    return w->phase == PW_MINIGAME && w->memory.phase == PM_SHOW
        ? (int)(w->memory.round * 8 + w->memory.elapsed / 750) : -1;
}
void pa_observer_init(pa_observer_t *s, const pet_world_t *w)
{
    memcpy(s->settlements, w->settlements, sizeof(s->settlements));
    s->stamps = w->journal_stamps;
    s->wins = w->memory_wins;
    s->phase = w->phase;
    s->order = w->order;
    s->game_phase = w->memory.phase;
    s->challenge_phase = w->challenge.phase;
    s->challenge_score = w->challenge.score;
    s->challenge_round = w->challenge.round;
    s->snacks = w->snacks;
    s->furniture_owned = w->wallet.owned;
    s->furniture_equipped = w->wallet.equipped;
    s->show_step = show_step(w);
}
pa_cue_t pa_observe(pa_observer_t *s, const pet_world_t *w)
{
    pa_cue_t cue = PA_NONE;
    if (w->wallet.owned != s->furniture_owned || w->wallet.equipped != s->furniture_equipped) cue = PA_DONE;
    if (w->snacks != s->snacks) cue = PA_MEAL;
    if (w->settlements[PW_TOUCH] != s->settlements[PW_TOUCH]) cue = PA_TOUCH;
    if (w->settlements[PW_EAT] != s->settlements[PW_EAT] ||
        w->settlements[PW_TAKEOUT] != s->settlements[PW_TAKEOUT]) cue = PA_MEAL;
    if (w->settlements[PW_STUDY] != s->settlements[PW_STUDY] ||
        w->settlements[PW_CODE] != s->settlements[PW_CODE] ||
        w->settlements[PW_WORKOUT] != s->settlements[PW_WORKOUT] ||
        (w->order == PW_ORDER_READY && s->order != PW_ORDER_READY)) cue = PA_DONE;
    if (w->phase == PW_SLEEP && s->phase != PW_SLEEP) cue = PA_REST;
    if (w->journal_stamps != s->stamps) cue = PA_STAMP;
    int step = show_step(w);
    if (step >= 0 && step != s->show_step && w->memory.elapsed % 750 < 500)
        cue = (pa_cue_t)(PA_NOTE0 + w->memory.sequence[w->memory.elapsed / 750]);
    if (w->phase == PW_MINIGAME && w->memory.phase == PM_LOST &&
        s->game_phase != PM_LOST) cue = PA_RETRY;
    if (w->phase == PW_MINIGAME && w->challenge.kind != PG_NONE)
    {
        if (w->challenge.kind == PG_RHYTHM && w->challenge.phase == PG_INPUT &&
            (s->phase != PW_MINIGAME || s->challenge_round != w->challenge.round)) cue = PA_NOTE1;
        if (w->challenge.score != s->challenge_score) cue = PA_NOTE0;
        if (w->challenge.phase == PG_LOST && s->challenge_phase != PG_LOST) cue = PA_RETRY;
    }
    if (w->settlements[PW_MEMORY] != s->settlements[PW_MEMORY])
        cue = w->challenge.kind == PG_LIFE ? PA_DONE : PA_WIN;
    pa_observer_init(s, w);
    return cue;
}
void pa_trigger(pa_mixer_t *m, pa_cue_t cue)
{
    if (cue > PA_NONE && cue < PA_CUE_COUNT) { m->cue = cue; m->cue_at = 0; }
}
void pa_mix(pa_mixer_t *m, unsigned mode, bool duck, int16_t *out, size_t n)
{
    if (!mode) { m->cue = PA_NONE; m->cue_at = 0; }
    for (size_t i = 0; i < n; i++)
    {
        int sample = 0;
        if (mode == 2)
        {
            sample = pa_music.data[m->music_at];
            if (duck) sample /= 4;
            if (++m->music_at == pa_music.count) m->music_at = 0;
        }
        if (mode && m->cue != PA_NONE)
        {
            const pa_pcm_t *pcm = &pa_cues[m->cue];
            sample += pcm->data[m->cue_at++];
            if (m->cue_at == pcm->count) { m->cue = PA_NONE; m->cue_at = 0; }
        }
        out[i] = (int16_t)(sample > 32767 ? 32767 : sample < -32768 ? -32768 : sample);
    }
}
