#define _GNU_SOURCE
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "msp/core.h"

double msp_probe_duration(const char *path);
double msp_cache_get(const char *key);
void msp_cache_set(const char *key, double duration);

static int is_audio_ext(const char *name) {
    const char *e = strrchr(name, '.');
    if (!e) return 0;
    e++;
    const char *exts[] = {"mp3","opus","ogg","oga","spx","flac","wav","wave","aiff","aif","aifc","au","snd","m4a","m4b","m4r","aac","wma","asf","mp2","mp1","mpga","ape","wv","wvp","tta","mpc","mpp","mp+","ac3","eac3","dts","dca","ra","rm","ram","amr","3gp","3g2","mp4","mka","mkv","webm", NULL};
    for (int i = 0; exts[i]; ++i) {
        if (strcasecmp(e, exts[i]) == 0) return 1;
    }
    return 0;
}

static void add_track(msp_source_t *s, const char *path) {
    s->tracks = realloc(s->tracks, (s->track_count + 1) * sizeof(msp_track_t));
    memset(&s->tracks[s->track_count], 0, sizeof(msp_track_t));
    s->tracks[s->track_count].path = strdup(path);
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    s->tracks[s->track_count].title = strdup(base);
    double d = msp_cache_get(path);
    if (d <= 0.0) {
        d = msp_probe_duration(path);
        if (d > 0.0) msp_cache_set(path, d);
    }
    s->tracks[s->track_count].duration_sec = d > 0.0 ? d : 0.0;
    s->track_count++;
}

static void scan_dir(const char *dir, msp_source_t *s) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[4096];
        snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            scan_dir(p, s);
        } else if (S_ISREG(st.st_mode) && is_audio_ext(e->d_name)) {
            add_track(s, p);
        }
    }
    closedir(d);
}

int msp_source_auto(msp_source_t **out) {
    if (!out) return -1;
    const char *user = getenv("USER");
    if (user) {
        char media[512];
        snprintf(media, sizeof(media), "/run/media/%s", user);
        DIR *d = opendir(media);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (e->d_name[0] == '.') continue;
                char cand[1024];
                snprintf(cand, sizeof(cand), "%s/%s", media, e->d_name);
                struct stat st;
                if (stat(cand, &st) == 0 && S_ISDIR(st.st_mode)) {
                    char y[1100];
                    snprintf(y, sizeof(y), "%s/data.yaml", cand);
                    if (access(y, R_OK) == 0) {
                        closedir(d);
                        return msp_source_detect(cand, out);
                    }
                }
            }
            closedir(d);
        }
    }
    const char *home = getenv("HOME");
    if (home) {
        char music[512];
        snprintf(music, sizeof(music), "%s/Music", home);
        struct stat st;
        if (stat(music, &st) == 0 && S_ISDIR(st.st_mode)) {
            return msp_source_detect(music, out);
        }
    }
    return -1;
}

int msp_source_load_yaml(const char *yaml_path, msp_source_t *s);

int msp_source_detect(const char *path, msp_source_t **out) {
    if (!path || !out) return -1;
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    msp_source_t *s = calloc(1, sizeof(msp_source_t));
    s->root = strdup(path);
    const char *base = strrchr(path, '/');
    s->name = strdup(base && base[1] ? base + 1 : path);
    if (S_ISDIR(st.st_mode)) {
        char y[1100];
        snprintf(y, sizeof(y), "%s/data.yaml", path);
        if (access(y, R_OK) == 0) {
            s->kind = MSP_KIND_BUNDLE;
            msp_source_load_yaml(y, s);
        } else {
            DIR *d = opendir(path);
            int has_m3u = 0;
            if (d) {
                struct dirent *e;
                while ((e = readdir(d))) {
                    if (e->d_name[0] == '.') continue;
                    const char *ext = strrchr(e->d_name, '.');
                    if (ext && (strcasecmp(ext + 1, "m3u") == 0 || strcasecmp(ext + 1, "m3u8") == 0)) {
                        has_m3u = 1;
                        break;
                    }
                }
                closedir(d);
            }
            s->kind = has_m3u ? MSP_KIND_PLAYLIST : MSP_KIND_FOLDER;
        }
        scan_dir(path, s);
    } else {
        free(s);
        return -1;
    }
    *out = s;
    return 0;
}

void msp_source_free(msp_source_t *s) {
    if (!s) return;
    for (size_t i = 0; i < s->track_count; ++i) {
        free(s->tracks[i].path);
        free(s->tracks[i].title);
        free(s->tracks[i].artist);
        free(s->tracks[i].album);
        free(s->tracks[i].genre);
    }
    free(s->tracks);
    free(s->root);
    free(s->name);
    free(s->title);
    free(s->artist);
    free(s->genre);
    free(s->description);
    free(s->cover);
    free(s->collection_type);
    free(s);
}
