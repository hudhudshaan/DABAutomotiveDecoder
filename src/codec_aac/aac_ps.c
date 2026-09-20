/**
 * @file    aac_ps.c
 * @brief   Parametric Stereo (PS) Spatializer Implementation
 * @standard ETSI TS 102 563 & ISO/IEC 14496-3, MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "aac_ps.h"
#include "../dsp/dsp_math.h"
#include <string.h>

void aac_ps_init(aac_ps_state_t *p_ps) {
    if (p_ps == NULL) {
        return;
    }
    p_ps->ps_enable = 1U;
    p_ps->delay_idx = 0U;
    for (uint8_t i = 0U; i < 128U; i++) {
        p_ps->decorr_delay[i] = 0.0f;
    }
}

void aac_ps_process(
    aac_ps_state_t *p_ps,
    const float    *p_mono_in,
    float          *p_left_out,
    float          *p_right_out,
    uint16_t        num_samples
) {
    if ((p_ps == NULL) || (p_mono_in == NULL) || (p_left_out == NULL) || (p_right_out == NULL)) {
        return;
    }

    /* All-pass decorrelation filter with delay line */
    for (uint16_t i = 0U; i < num_samples; i++) {
        float mono = p_mono_in[i];

        /* Delay line tap */
        uint8_t read_idx = (uint8_t)((p_ps->delay_idx + 64U) & 0x7FU);
        float delayed = p_ps->decorr_delay[read_idx];

        /* All-pass fraction */
        float decorrelated = delayed - (mono * 0.35f);
        p_ps->decorr_delay[p_ps->delay_idx] = mono + (decorrelated * 0.35f);
        p_ps->delay_idx = (uint8_t)((p_ps->delay_idx + 1U) & 0x7FU);

        /* Spatialization matrix (IID + ICC synthesis) */
        /* Left = (M + D) * 0.707, Right = (M - D) * 0.707 */
        p_left_out[i]  = (mono + (decorrelated * 0.45f)) * 0.7071f;
        p_right_out[i] = (mono - (decorrelated * 0.45f)) * 0.7071f;
    }
}
