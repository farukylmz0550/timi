/* core/plan.c - 45:00 hedef + 45 sn üst sınır (C portu) */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "msp/core.h"

#ifndef MSP_TARGET_SEC
#define MSP_TARGET_SEC (45 * 60)
#endif
#ifndef MSP_SLACK_SEC
#define MSP_SLACK_SEC 45
#endif
#ifndef MSP_CAP_SEC
#define MSP_CAP_SEC (MSP_TARGET_SEC + MSP_SLACK_SEC)
#endif
#ifndef MSP_PLAN_FLOOR
#define MSP_PLAN_FLOOR (MSP_TARGET_SEC - MSP_SLACK_SEC)
#endif

static void shuffle_array(msp_track_t *arr, size_t n) {
    if (n <= 1) return;
    unsigned long seed = (unsigned long)time(NULL) ^ (unsigned long)clock() ^ 1469598103934665603UL;
    for (long i = (long)n - 1; i >= 1; --i) {
        seed = seed * 1099511628211UL ^ (unsigned long)(seed >> 33);
        size_t j = (size_t)(seed >> 11) % (size_t)(i + 1);
        msp_track_t tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

static int select_block(const msp_track_t *tracks, size_t n, int floor_sec, int ideal_sec, int cap_sec,
                         int **out_idx, size_t *out_cnt) {
    if (n == 0) return -1;
    size_t cap = (size_t)cap_sec;
    size_t floor_s = (size_t)(floor_sec > 0 ? floor_sec : 0);
    char *reachable = (char *)calloc(cap + 1, 1);
    int *parent_sum = (int *)malloc((cap + 1) * sizeof(int));
    int *parent_idx_arr = (int *)malloc((cap + 1) * sizeof(int));
    if (!reachable || !parent_sum || !parent_idx_arr) {
        free(reachable); free(parent_sum); free(parent_idx_arr);
        return -1;
    }
    for (size_t i = 0; i <= cap; ++i) { parent_sum[i] = -1; parent_idx_arr[i] = -1; }
    reachable[0] = 1;
    for (size_t idx = 0; idx < n; ++idx) {
        double td = tracks[idx].duration_sec;
        if (td <= 0.0) td = 1.0;
        size_t dur = (size_t)ceil(td);
        if (dur < 1) dur = 1;
        if (dur > cap) continue;
        for (long s = (long)cap - (long)dur; s >= 0; --s) {
            if (reachable[s] && !reachable[s + dur]) {
                reachable[s + dur] = 1;
                parent_sum[s + dur] = (int)s;
                parent_idx_arr[s + dur] = (int)idx;
            }
        }
    }
    long goal = -1;
    long best_diff = 1000000000L;
    for (long s = (long)floor_s; s <= (long)cap; ++s) {
        if (reachable[s]) {
            long diff = llabs((long)s - (long)ideal_sec);
            if (goal < 0 || diff < best_diff || (diff == best_diff && s > goal)) {
                best_diff = diff;
                goal = s;
            }
        }
    }
    if (goal < 0) {
        free(reachable); free(parent_sum); free(parent_idx_arr);
        return -1;
    }
    size_t tmp_cnt = 0;
    int *tmp = (int *)malloc(n * sizeof(int));
    long ss = goal;
    while (parent_sum[ss] >= 0 && tmp_cnt < n) {
        tmp[tmp_cnt++] = parent_idx_arr[ss];
        ss = parent_sum[ss];
    }
    for (size_t i = 0; i < tmp_cnt / 2; ++i) {
        int x = tmp[i]; tmp[i] = tmp[tmp_cnt - 1 - i]; tmp[tmp_cnt - 1 - i] = x;
    }
    /* sort */
    for (size_t i = 0; i < tmp_cnt; ++i)
        for (size_t j = i + 1; j < tmp_cnt; ++j)
            if (tmp[i] > tmp[j]) { int x = tmp[i]; tmp[i] = tmp[j]; tmp[j] = x; }
    *out_idx = tmp;
    *out_cnt = tmp_cnt;
    free(reachable); free(parent_sum); free(parent_idx_arr);
    return 0;
}

int msp_plan_build(msp_source_t *s, const msp_opts_t *opts, msp_plan_t **out) {
    if (!s || !out) return -1;
    msp_opts_t def = {0};
    if (!opts) opts = &def;
    size_t n = 0;
    for (size_t i = 0; i < s->track_count; ++i) if (s->tracks[i].path) n++;
    if (n == 0) return -2;
    msp_track_t *tracks = (msp_track_t *)calloc(n, sizeof(msp_track_t));
    for (size_t i = 0, j = 0; i < s->track_count && j < n; ++i)
        if (s->tracks[i].path) tracks[j++] = s->tracks[i];
    if (opts->shuffle || opts->fewer_shuffle) shuffle_array(tracks, n);
    int *idx = NULL;
    size_t icnt = 0;
    if (select_block(tracks, n, MSP_PLAN_FLOOR, MSP_TARGET_SEC, MSP_CAP_SEC, &idx, &icnt) != 0)
        if (select_block(tracks, n, MSP_TARGET_SEC - 120, MSP_TARGET_SEC, MSP_CAP_SEC, &idx, &icnt) != 0) { idx = NULL; icnt = 0; }
    msp_plan_t *p = (msp_plan_t *)calloc(1, sizeof(msp_plan_t));
    p->source_name = s->name ? strdup(s->name) : NULL;
    if (idx && icnt > 0) {
        p->items = (msp_track_t *)calloc(icnt, sizeof(msp_track_t));
        p->item_count = icnt;
        p->mode = MSP_MODE_BLOCK;
        p->stop_at = 0.0;
        p->fade_sec = 0.0;
        for (size_t i = 0; i < icnt; ++i) p->items[i] = tracks[idx[i]];
        free(idx);
    } else {
        double total = 0.0;
        for (size_t i = 0; i < n; ++i) total += tracks[i].duration_sec > 0 ? tracks[i].duration_sec : 1.0;
        if (total < 1.0) total = 1.0;
        size_t repeats = (size_t)ceil((MSP_CAP_SEC + 1.0) / total) + 2;
        if (repeats < 2) repeats = 2;
        p->items = (msp_track_t *)calloc(n * repeats, sizeof(msp_track_t));
        p->item_count = n * repeats;
        p->mode = MSP_MODE_LOOP;
        p->stop_at = (double)MSP_CAP_SEC;
        p->fade_sec = 6.0;
        for (size_t r = 0; r < repeats; ++r)
            for (size_t i = 0; i < n; ++i)
                p->items[r * n + i] = tracks[i];
    }
    /* total */
    if (p->mode == MSP_MODE_LOOP && p->stop_at > 0.0) p->total_sec = p->stop_at;
    else {
        double t = 0.0;
        for (size_t i = 0; i < p->item_count; ++i) t += p->items[i].duration_sec > 0 ? p->items[i].duration_sec : 0.0;
        p->total_sec = t;
    }
    free(tracks);
    *out = p;
    return 0;
}

double msp_plan_total(const msp_plan_t *p) {
    if (!p) return 0.0;
    return p->total_sec;
}

void msp_plan_free(msp_plan_t *p) {
    if (!p) return;
    free(p->items);
    free(p->source_name);
    free(p);
}

const char *msp_format_time(double sec, char *buf, size_t len) {
    if (!buf || len == 0) return "";
    if (sec < 0.0) sec = 0.0;
    long s = (long)llround(sec);
    long m = s / 60;
    long ss = s % 60;
    snprintf(buf, len, "%ld:%02ld", m, ss);
    return buf;
}


