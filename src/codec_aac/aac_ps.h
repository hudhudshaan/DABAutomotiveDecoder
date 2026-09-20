/**
 * @file    aac_ps.h
 * @brief   Parametric Stereo (PS) Spatializer & Decorrelator for HE-AAC v2 (ETSI TS 102 563)
 * @standard ETSI TS 102 563 & ISO/IEC 14496-3, MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef AAC_PS_H
#define AAC_PS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t ps_enable;
    float   decorr_delay[128];
    uint8_t delay_idx;
} aac_ps_state_t;

void aac_ps_init(aac_ps_state_t *p_ps);

void aac_ps_process(
    aac_ps_state_t *p_ps,
    const float    *p_mono_in,
    float          *p_left_out,
    float          *p_right_out,
    uint16_t        num_samples
);

#ifdef __cplusplus
}
#endif

#endif /* AAC_PS_H */
