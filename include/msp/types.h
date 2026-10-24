#ifndef MSP_TYPES_H
#define MSP_TYPES_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msp_track {
    char *path;
    char *title;
    char *artist;
    char *album;
    char *genre;
    int year;
    int track_no;
    int disc_no;
    double duration_sec;
} msp_track_t;

typedef enum {
    MSP_KIND_FOLDER = 0,
    MSP_KIND_PLAYLIST = 1,
    MSP_KIND_BUNDLE = 2
} msp_kind_t;

typedef struct msp_source {
    char *root;
    char *name;
    char *title;
    char *artist;
    char *genre;
    char *description;
    char *cover;
    char *collection_type;
    msp_kind_t kind;
    msp_track_t *tracks;
    size_t track_count;
} msp_source_t;

typedef enum {
    MSP_MODE_BLOCK = 0,
    MSP_MODE_LOOP  = 1
} msp_mode_t;

typedef struct msp_plan {
    msp_track_t *items;
    size_t item_count;
    msp_mode_t mode;
    double stop_at;
    double fade_sec;
    double total_sec;
    char *source_name;
} msp_plan_t;

typedef struct msp_opts {
    bool shuffle;
    bool fewer_shuffle;
    int repeat_mode; /* 0=off, 1=all, 2=one */
    bool crossfade;
    bool automix;
    bool gapless;
    double crossfade_sec;
} msp_opts_t;

#ifdef __cplusplus
}
#endif
#endif /* MSP_TYPES_H */
