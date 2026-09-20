/**
 * @file    pop_prevention.c
 * @brief   Automotive Pop-Noise Prevention Implementation
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "pop_prevention.h"
#include "../dsp/dsp_math.h"
#include <stddef.h>

void pop_prevention_init(pop_prevention_t *pop, uint32_t sample_rate_hz) {
    if (pop == NULL) {
        return;
    }
    pop->sample_rate_hz = sample_rate_hz;
    /* 5ms crossfade */
    pop->crossfade_samples = (uint16_t)((sample_rate_hz * 5U) / 1000U);
    if (pop->crossfade_samples < 8U) {
        pop->crossfade_samples = 8U;
    }
    pop->in_transition = 0U;
}

void pop_prevention_apply(
    pop_prevention_t *pop,
    int16_t *pcm,
    uint16_t num_samples_per_ch,
    uint8_t num_channels,
    uint8_t is_recovery_frame
) {
    if ((pop == NULL) || (pcm == NULL) || (num_samples_per_ch == 0U) || (num_channels == 0U)) {
        return;
    }

    if (is_recovery_frame != 0U) {
        pop->in_transition = 1U;
    }

    if (pop->in_transition != 0U) {
        uint16_t fade_len = pop->crossfade_samples;
        if (fade_len > num_samples_per_ch) {
            fade_len = num_samples_per_ch;
        }

        for (uint16_t i = 0U; i < fade_len; i++) {
            /* Smooth raised-cosine ramp from 0 to Q15_ONE */
            uint32_t lut_idx = 127U - ((uint32_t)i * 127U) / (uint32_t)fade_len;
            int32_t gain = (int32_t)dsp_cos_q15(lut_idx);

            for (uint8_t ch = 0U; ch < num_channels; ch++) {
                uint16_t idx = (uint16_t)(i * num_channels + ch);
                int32_t val = ((int32_t)pcm[idx] * gain + 16384) >> 15;
                pcm[idx] = dsp_clamp16(val);
            }
        }
        pop->in_transition = 0U;
    }
}

void pop_prevention_reset(pop_prevention_t *pop) {
    if (pop == NULL) {
        return;
    }
    pop->in_transition = 0U;
}
