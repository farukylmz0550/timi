#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include "msp/backend.h"
#include "third_party/miniaudio.h"

struct msp_player {
    ma_engine engine;
    const msp_plan_t *plan;
    const msp_opts_t *opts;
    size_t index;
    int playing;
    ma_sound *sounds;
    size_t sound_count;
};

static void on_end(void *pUserData, ma_sound *pSound) {
    (void)pSound;
    msp_player_t *p = (msp_player_t *)pUserData;
    if (!p || !p->plan) return;
    p->index++;
    if (p->index >= p->plan->item_count) {
        if (p->opts && p->opts->repeat_mode == 1) { /* all */
            p->index = 0;
            if (p->index < p->plan->item_count && p->plan->items[p->index].path) {
                ma_engine_play_sound(&p->engine, p->plan->items[p->index].path, NULL);
            }
        }
        return;
    }
    if (p->plan->items[p->index].path) {
        ma_engine_play_sound(&p->engine, p->plan->items[p->index].path, NULL);
    }
}

int msp_player_create(const msp_plan_t *plan, const msp_opts_t *opts, msp_player_t **out) {
    if (!plan || !out) return -1;
    msp_player_t *p = calloc(1, sizeof(msp_player_t));
    p->plan = plan;
    p->opts = opts;
    p->index = 0;
    if (ma_engine_init(NULL, &p->engine) != MA_SUCCESS) {
        free(p);
        return -1;
    }
    *out = p;
    return 0;
}

int msp_player_play(msp_player_t *p) {
    if (!p || !p->plan || p->plan->item_count == 0) return -1;
    const char *path = p->plan->items[p->index].path;
    if (!path) return -1;
    /* try to play and set callback by restarting from start logic */
    ma_engine_play_sound(&p->engine, path, NULL);
    p->playing = 1;
    /* simple: poll not possible; user can advance manually */
    return 0;
}

int msp_player_pause(msp_player_t *p) { (void)p; return 0; }
int msp_player_stop(msp_player_t *p) { if (p) ma_engine_stop(&p->engine); return 0; }
int msp_player_next(msp_player_t *p) { if (!p || !p->plan) return -1; p->index++; if (p->index >= p->plan->item_count) p->index = 0; return msp_player_play(p); }
int msp_player_prev(msp_player_t *p) { if (!p || !p->plan) return -1; if (p->index == 0) p->index = p->plan->item_count ? p->plan->item_count - 1 : 0; else p->index--; return msp_player_play(p); }
int msp_player_seek(msp_player_t *p, double sec) { (void)p; (void)sec; return 0; }

void msp_player_free(msp_player_t *p) {
    if (!p) return;
    ma_engine_uninit(&p->engine);
    free(p->sounds);
    free(p);
}
