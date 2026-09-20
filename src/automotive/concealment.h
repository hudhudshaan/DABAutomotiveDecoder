/**
 * @file    concealment.h
 * @brief   Automotive Multi-Stage Concealment Engine (MS-3)
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef CONCEALMENT_H
#define CONCEALMENT_H

#include <stdint.h>
#include "dab_decoder.h"
#include "soft_mute.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DAB_CONCEAL_HISTORY_MAX_SAMPLES  (DAB_MAX_PCM_SAMPLES_PER_AU * 2U)

typedef struct {
    DAB_ConcealmentConfig config;
    DAB_ConcealmentMode   mode;
    uint8_t               consec_loss_cnt;
    uint8_t               prev_pcm_valid;
    uint16_t              prev_pcm_count;
    int32_t               attn_gain_q15;
    int16_t               prev_pcm[DAB_CONCEAL_HISTORY_MAX_SAMPLES];
} concealment_engine_t;

void concealment_init(concealment_engine_t *conc, const DAB_ConcealmentConfig *cfg);
void concealment_process(
    concealment_engine_t *conc,
    soft_mute_engine_t   *mute,
    DAB_AUStatus          au_status,
    const int16_t        *decoded_pcm_in,
    uint16_t              num_samples_per_ch,
    uint8_t               num_channels,
    int16_t              *pcm_out,
    uint8_t               enable_neon
);
void concealment_reset(concealment_engine_t *conc);

#ifdef __cplusplus
}
#endif

#endif /* CONCEALMENT_H */
