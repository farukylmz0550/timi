#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>

void msp_str_trim(char *s) {
    if (!s) return;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') memmove(s, s+1, strlen(s));
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1] == '\r' || s[len-1] == '\n')) s[--len] = 0;
}
