#ifndef MSP_CORE_H
#define MSP_CORE_H

#include "msp/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Kaynak algılama (tamamen çevrimdışı, yerel dosya sistemi) */
int msp_source_auto(msp_source_t **out);
int msp_source_detect(const char *path, msp_source_t **out);
void msp_source_free(msp_source_t *s);

/* Planlama: 45:00 hedef + 45 sn üst sınır */
int msp_plan_build(msp_source_t *s, const msp_opts_t *opts, msp_plan_t **out);
double msp_plan_total(const msp_plan_t *p);
void msp_plan_free(msp_plan_t *p);

/* Dışa aktarma: doğrudan hedefe yaz, ek sarmalayıcı klasör oluşturma */
int msp_export_bundle(const msp_plan_t *p, const msp_source_t *s, const char *dest_dir, char **out_path);

/* Yardımcılar */
const char *msp_format_time(double sec, char *buf, size_t len);
void msp_cache_flush(void);

#ifdef __cplusplus
}
#endif
#endif /* MSP_CORE_H */

double msp_probe_duration(const char *path);
double msp_cache_get(const char *key);
void msp_cache_set(const char *key, double duration);

void msp_str_trim(char *s);

int msp_source_load_yaml(const char *yaml_path, msp_source_t *s);

int msp_source_load_m3u(const char *m3u_path, msp_source_t *s);
