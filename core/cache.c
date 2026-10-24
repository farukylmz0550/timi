#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static char *get_cache_path(void) {
    const char *home = getenv("HOME");
    if (!home) return NULL;
    char *p = malloc(1024);
    snprintf(p, 1024, "%s/.cache/microsd-music-player", home);
    mkdir(p, 0755);
    char *f = malloc(1100);
    snprintf(f, 1100, "%s/durations.tsv", p);
    free(p);
    return f;
}

double msp_cache_get(const char *key) {
    char *path = get_cache_path();
    if (!path || !key) { free(path); return 0.0; }
    FILE *fp = fopen(path, "r");
    free(path);
    if (!fp) return 0.0;
    struct stat st;
    if (stat(key, &st) != 0) { fclose(fp); return 0.0; }
    long mtime = (long)st.st_mtime;
    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = 0;
        if (strcmp(line, key) == 0) {
            char *tab2 = strchr(tab + 1, '\t');
            if (tab2) {
                *tab2 = 0;
                long mt = atol(tab + 1);
                if (mt == mtime) {
                    double d = atof(tab2 + 1);
                    fclose(fp);
                    return d > 0 ? d : 0.0;
                }
            }
            break;
        }
    }
    fclose(fp);
    return 0.0;
}

void msp_cache_set(const char *key, double duration) {
    if (!key || duration <= 0) return;
    char *path = get_cache_path();
    if (!path) return;
    struct stat st;
    long mtime = 0;
    if (stat(key, &st) == 0) mtime = (long)st.st_mtime;
    /* simple append/update not implemented fully, just append */
    FILE *fp = fopen(path, "a");
    if (fp) {
        fprintf(fp, "%s\t%ld\t%.3f\n", key, mtime, duration);
        fclose(fp);
    }
    free(path);
}

void msp_cache_flush(void) { /* no-op for now */ }
