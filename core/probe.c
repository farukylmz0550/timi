#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "msp/core.h"

double msp_probe_duration(const char *path) {
    if (!path) return 0.0;
    /* try to use ffprobe if available */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 '%s' 2>/dev/null", path);
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0.0;
    char buf[128];
    if (!fgets(buf, sizeof(buf), fp)) { pclose(fp); return 0.0; }
    pclose(fp);
    double d = atof(buf);
    if (d <= 0.0) return 0.0;
    return d;
}
