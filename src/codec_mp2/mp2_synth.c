/**
 * @file    mp2_synth.c
 * @brief   Polyphase 32-Subband Synthesis Filterbank Implementation
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "mp2_synth.h"
#include "../dsp/dsp_math.h"
#include <string.h>

void mp2_synth_init(mp2_synth_state_t *p_synth) {
    if (p_synth != NULL) {
        for (uint8_t ch = 0U; ch < 2U; ch++) {
            for (uint16_t i = 0U; i < 512U; i++) {
                p_synth->fifo_buffer[ch][i] = 0.0f;
            }
        }
    }
}

static void mp2_subband_matrix(const float *s, float *v) {
    /* V[i] = sum(k=0..31) cos((2*i + 1)*(k - 16)*pi / 64) * S[k] */
    for (uint32_t i = 0U; i < 64U; i++) {
        float sum = 0.0f;
        for (uint32_t k = 0U; k < 32U; k++) {
            float angle = (float)(2U * i + 1U) * (float)((int32_t)k - 16) * (DAB_PI_F / 64.0f);
            float coeff = dsp_cos_f32(angle);
            sum += coeff * s[k];
        }
        v[i] = sum;
    }
}

static void mp2_window_overlap_add(float *fifo, const float *v, float *pcm_out) {
    /* Shift FIFO by 64 samples */
    for (int32_t idx = 511; idx >= 64; idx--) {
        fifo[idx] = fifo[idx - 64];
    }
    for (uint32_t i = 0U; i < 64U; i++) {
        fifo[i] = v[i];
    }

    /* Window and sum 16 blocks of 32 */
    for (uint32_t j = 0U; j < 32U; j++) {
        float sum = 0.0f;
        for (uint32_t i = 0U; i < 16U; i++) {
            uint32_t idx = j + (i * 32U);
            float sign = ((i & 1U) != 0U) ? -1.0f : 1.0f;
            float angle = ((float)idx + 0.5f) * (DAB_PI_F / 512.0f);
            float window_coeff = dsp_sin_f32(angle);
            sum += window_coeff * fifo[idx] * sign;
        }
        pcm_out[j] = sum;
    }
}

void mp2_synth_process_subband_block(
    mp2_synth_state_t *p_synth,
    float subband_samples[2][32],
    float pcm_out_samples[2][32],
    uint8_t num_channels,
    uint8_t enable_neon
) {
    (void)enable_neon;
    if ((p_synth == NULL) || (num_channels == 0U) || (num_channels > 2U)) {
        return;
    }

    for (uint8_t ch = 0U; ch < num_channels; ch++) {
        float vector_buf[64];
        mp2_subband_matrix(subband_samples[ch], vector_buf);
        mp2_window_overlap_add(p_synth->fifo_buffer[ch], vector_buf, pcm_out_samples[ch]);
    }
}
