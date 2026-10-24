#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "msp/core.h"

double msp_probe_duration(const char *path);
double msp_cache_get(const char *key);
void msp_cache_set(const char *key, double duration);

void msp_str_trim(char *s);

int msp_source_load_m3u(const char *m3u_path, msp_source_t *s) {
    if (!m3u_path || !s) return -1;
    FILE *f = fopen(m3u_path, "r");
    if (!f) return -1;
    char line[1024];
    const char *dir = strrchr(m3u_path, '/');
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        msp_str_trim(line);
        if (*line == 0) continue;
        char full[4096];
        if (line[0] == '/') {
            strncpy(full, line, sizeof(full)-1);
        } else if (dir) {
            size_t dlen = (size_t)(dir - m3u_path);
            memcpy(full, m3u_path, dlen);
            full[dlen] = '/';
            strncpy(full + dlen + 1, line, sizeof(full) - dlen - 2);
        } else {
            strncpy(full, line, sizeof(full)-1);
        }
        full[sizeof(full)-1] = 0;
        struct stat st;
        if (stat(full, &st) != 0) continue;
        s->tracks = realloc(s->tracks, (s->track_count + 1) * sizeof(msp_track_t));
        memset(&s->tracks[s->track_count], 0, sizeof(msp_track_t));
        s->tracks[s->track_count].path = strdup(full);
        const char *base = strrchr(full, '/');
        base = base ? base + 1 : full;
        s->tracks[s->track_count].title = strdup(base);
        double d = msp_cache_get(full);
        if (d <= 0.0) {
            d = msp_probe_duration(full);
            if (d > 0.0) msp_cache_set(full, d);
        }
        s->tracks[s->track_count].duration_sec = d > 0.0 ? d : 0.0;
        s->track_count++;
    }
    fclose(f);
    return 0;
}
