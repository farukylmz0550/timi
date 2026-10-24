#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "msp/core.h"

static void sanitize_name(char *s) {
    for (char *p = s; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20) { *p = '_'; continue; }
        switch (*p) {
            case '<': case '>': case ':': case '"': case '/': case '\\': case '|': case '?': case '*': *p = '_'; break;
            default: break;
        }
    }
    while (*s && (*s == ' ' || *s == '.')) memmove(s, s + 1, strlen(s));
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '.')) s[--len] = 0;
    if (*s == 0) strcpy(s, "_");
}

static void copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return;
    /* ensure dir */
    char tmp[4096];
    strncpy(tmp, dst, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1]=0;
    for (char *p = tmp + 1; *p; ++p) if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return; }
    char buf[4096];
    size_t r;
    while ((r = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, r, out);
    fclose(in); fclose(out);
}

int msp_export_bundle(const msp_plan_t *p, const msp_source_t *s, const char *dest_dir, char **out_path) {
    if (!p || !s || !dest_dir) return -1;
    struct stat st;
    if (stat(dest_dir, &st) != 0) {
        /* mkdir recursive */
        char tmp[4096];
        strncpy(tmp, dest_dir, sizeof(tmp)-1); tmp[sizeof(tmp)-1]=0;
        for (char *x = tmp + 1; *x; ++x) if (*x == '/') { *x = 0; mkdir(tmp, 0755); *x = '/'; }
        mkdir(tmp, 0755);
    }
    /* copy files */
    for (size_t i = 0; i < p->item_count; ++i) {
        const char *path = p->items[i].path;
        if (!path) continue;
        const char *base = strrchr(path, '/');
        base = base ? base + 1 : path;
        char name[512];
        strncpy(name, base, sizeof(name)-1); name[sizeof(name)-1]=0;
        sanitize_name(name);
        char dst[4096];
        snprintf(dst, sizeof(dst), "%s/%s", dest_dir, name);
        copy_file(path, dst);
    }
    /* write minimal yaml */
    char ypath[4096];
    snprintf(ypath, sizeof(ypath), "%s/data.yaml", dest_dir);
    FILE *f = fopen(ypath, "w");
    if (!f) return -1;
    fprintf(f, "version: 2\n");
    if (s->artist) fprintf(f, "artist: \"%s\"\n", s->artist);
    if (s->title) fprintf(f, "title: \"%s\"\n", s->title);
    else if (s->name) fprintf(f, "title: \"%s\"\n", s->name);
    fprintf(f, "type: mixed\n");
    fprintf(f, "playlist:\n");
    for (size_t i = 0; i < p->item_count; ++i) {
        const char *path = p->items[i].path;
        if (!path) continue;
        const char *base = strrchr(path, '/');
        base = base ? base + 1 : path;
        char name[512];
        strncpy(name, base, sizeof(name)-1); name[sizeof(name)-1]=0;
        sanitize_name(name);
        fprintf(f, "  - file: \"%s\"\n", name);
        fprintf(f, "    title: \"%s\"\n", p->items[i].title ? p->items[i].title : name);
        if (p->items[i].artist) fprintf(f, "    artist: \"%s\"\n", p->items[i].artist);
    }
    fclose(f);
    if (out_path) {
        *out_path = strdup(ypath);
    }
    return 0;
}
