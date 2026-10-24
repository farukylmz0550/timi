#ifndef MSP_BACKEND_H
#define MSP_BACKEND_H

#include "msp/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msp_player msp_player_t;

int msp_player_create(const msp_plan_t *plan, const msp_opts_t *opts, msp_player_t **out);
int msp_player_play(msp_player_t *p);
int msp_player_pause(msp_player_t *p);
int msp_player_stop(msp_player_t *p);
int msp_player_next(msp_player_t *p);
int msp_player_prev(msp_player_t *p);
int msp_player_seek(msp_player_t *p, double sec);
void msp_player_free(msp_player_t *p);

#ifdef __cplusplus
}
#endif
#endif /* MSP_BACKEND_H */
