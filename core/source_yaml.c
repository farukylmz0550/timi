#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msp/core.h"

void msp_str_trim(char *s);
double msp_probe_duration(const char *path);
double msp_cache_get(const char *key);
void msp_cache_set(const char *key, double duration);

static void unquote(char *s) {
    if (!s || !*s) return;
    size_t len = strlen(s);
    if (len >= 2 && ((s[0]=='"' && s[len-1]=='"') || (s[0]=='\'' && s[len-1]=='\''))) {
        memmove(s, s+1, len-2);
        s[len-2] = 0;
    }
}

static void strip_nl(char *s) {
    if (!s) return;
    char *nl = strchr(s, '\n'); if (nl) *nl = 0;
    nl = strchr(s, '\r'); if (nl) *nl = 0;
}

int msp_source_load_yaml(const char *yaml_path, msp_source_t *s) {
    if (!yaml_path || !s) return -1;
    FILE *f = fopen(yaml_path, "r");
    if (!f) return -1;
    char line[1024];
    int in_playlist = 0;
    char cur_file[512] = {0};
    char cur_title[256] = {0};
    char cur_artist[256] = {0};
    char cur_album[256] = {0};
    char cur_duration[64] = {0};
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\r' || *p == 0) continue;
        strip_nl(p);
        if (strncmp(p, "playlist:", 10) == 0) { in_playlist = 1; continue; }
        if (in_playlist) {
            if (*p != '-' && (*p == '-' || strchr(p, ':'))) {
                /* check if it's a new key at same level */
                if (*p != '-') {
                    /* maybe end of playlist */
                }
            }
            if (*p == '-') {
                /* flush previous */
                if (cur_file[0]) {
                    /* find existing track or add */
                    char full[4096];
                    const char *dir = strrchr(yaml_path, '/');
                    if (dir) {
                        size_t dlen = (size_t)(dir - yaml_path);
                        memcpy(full, yaml_path, dlen);
                        full[dlen] = '/';
                        strncpy(full + dlen + 1, cur_file, sizeof(full) - dlen - 2);
                        full[sizeof(full)-1] = 0;
                    } else {
                        strncpy(full, cur_file, sizeof(full)-1);
                    }
                    /* look for track */
                    for (size_t i = 0; i < s->track_count; ++i) {
                        if (s->tracks[i].path && strcmp(s->tracks[i].path, full) == 0) {
                            if (cur_title[0]) { free(s->tracks[i].title); s->tracks[i].title = strdup(cur_title); }
                            if (cur_artist[0]) { free(s->tracks[i].artist); s->tracks[i].artist = strdup(cur_artist); }
                            if (cur_album[0]) { free(s->tracks[i].album); s->tracks[i].album = strdup(cur_album); }
                            if (cur_duration[0]) {
                                double dd = atof(cur_duration);
                                if (dd > 0) s->tracks[i].duration_sec = dd;
                            }
                            break;
                        }
                    }
                    cur_file[0]=cur_title[0]=cur_artist[0]=cur_album[0]=cur_duration[0]=0;
                }
                char *q = strchr(p, ':');
                if (q) {
                    char *val = q+1; while (*val==' '||*val=='\t') val++;
                    unquote(val);
                    if (*val) strncpy(cur_file, val, sizeof(cur_file)-1);
                }
                continue;
            }
            /* key: value in playlist item */
            char *colon = strchr(p, ':');
            if (!colon) continue;
            *colon = 0;
            char *key = p;
            msp_str_trim(key);
            char *val = colon + 1;
            while (*val==' '||*val=='\t') val++;
            unquote(val);
            if (strcmp(key, "file") == 0) strncpy(cur_file, val, sizeof(cur_file)-1);
            else if (strcmp(key, "title") == 0) strncpy(cur_title, val, sizeof(cur_title)-1);
            else if (strcmp(key, "artist") == 0) strncpy(cur_artist, val, sizeof(cur_artist)-1);
            else if (strcmp(key, "album") == 0) strncpy(cur_album, val, sizeof(cur_album)-1);
            else if (strcmp(key, "duration") == 0 || strcmp(key, "duration_sec") == 0) strncpy(cur_duration, val, sizeof(cur_duration)-1);
            continue;
        }
        /* top level */
        char *colon = strchr(p, ':');
        if (!colon) continue;
        *colon = 0;
        char *key = p;
        msp_str_trim(key);
        char *val = colon + 1;
        while (*val==' '||*val=='\t') val++;
        unquote(val);
        if (strcmp(key, "title") == 0 && !s->title) s->title = strdup(val);
        else if (strcmp(key, "artist") == 0 && !s->artist) s->artist = strdup(val);
        else if (strcmp(key, "genre") == 0 && !s->genre) s->genre = strdup(val);
        else if (strcmp(key, "collection_type") == 0 && !s->collection_type) s->collection_type = strdup(val);
        else if (strcmp(key, "cover") == 0 && !s->cover) s->cover = strdup(val);
        else if (strcmp(key, "description") == 0 && !s->description) s->description = strdup(val);
    }
    /* flush last */
    if (in_playlist && cur_file[0]) {
        char full[4096];
        const char *dir = strrchr(yaml_path, '/');
        if (dir) {
            size_t dlen = (size_t)(dir - yaml_path);
            memcpy(full, yaml_path, dlen);
            full[dlen] = '/';
            strncpy(full + dlen + 1, cur_file, sizeof(full) - dlen - 2);
            full[sizeof(full)-1] = 0;
        } else {
            strncpy(full, cur_file, sizeof(full)-1);
        }
        for (size_t i = 0; i < s->track_count; ++i) {
            if (s->tracks[i].path && strcmp(s->tracks[i].path, full) == 0) {
                if (cur_title[0]) { free(s->tracks[i].title); s->tracks[i].title = strdup(cur_title); }
                if (cur_artist[0]) { free(s->tracks[i].artist); s->tracks[i].artist = strdup(cur_artist); }
                if (cur_album[0]) { free(s->tracks[i].album); s->tracks[i].album = strdup(cur_album); }
                if (cur_duration[0]) {
                    double dd = atof(cur_duration);
                    if (dd > 0) s->tracks[i].duration_sec = dd;
                }
                break;
            }
        }
    }
    fclose(f);
    return 0;
}
