/**
 * @file    soft_mute.h
 * @brief   Automotive Soft Mute Gain Ramp Engine (MS-3)
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef SOFT_MUTE_H
#define SOFT_MUTE_H

#include <stdint.h>
#include "dab_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t       sample_rate_hz;
    uint32_t       attack_ms;
    uint32_t       release_ms;
    uint32_t       attack_samples;
    uint32_t       release_samples;
    DAB_RampCurve  curve;
    DAB_MuteState  state;
    uint32_t       ramp_pos;
    int32_t        cur_gain_q15;
} soft_mute_engine_t;

void    soft_mute_init(soft_mute_engine_t *mute, uint32_t sample_rate_hz, uint32_t attack_ms, uint32_t release_ms, DAB_RampCurve curve);
void    soft_mute_recalc_timing(soft_mute_engine_t *mute);
void    soft_mute_trigger_attack(soft_mute_engine_t *mute);
void    soft_mute_trigger_release(soft_mute_engine_t *mute);
void    soft_mute_apply(soft_mute_engine_t *mute, int16_t *pcm_interleaved, uint16_t num_samples_per_ch, uint8_t num_channels, uint8_t enable_neon);
int32_t soft_mute_get_gain_q15(const soft_mute_engine_t *mute);
void    soft_mute_reset(soft_mute_engine_t *mute);

#ifdef __cplusplus
}
#endif

#endif /* SOFT_MUTE_H */
