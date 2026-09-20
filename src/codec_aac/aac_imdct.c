/**
 * @file    aac_imdct.c
 * @brief   960-Point & 120-Point IMDCT & Windowing Implementation for DAB+
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "aac_imdct.h"
#include "aac_tables.h"
#include "../dsp/dsp_math.h"
#include <string.h>

static float s_imdct_cos_lut[3840];
static uint8_t s_imdct_cos_lut_inited = 0U;

static void aac_imdct_ensure_table(void) {
    if (s_imdct_cos_lut_inited == 0U) {
        for (uint32_t i = 0U; i < 3840U; i++) {
            s_imdct_cos_lut[i] = dsp_cos_f32((float)i * (DAB_PI_F / 3840.0f));
        }
        s_imdct_cos_lut_inited = 1U;
    }
}

static inline float aac_imdct_cos(uint32_t idx_mod_7680) {
    uint32_t m = idx_mod_7680 % 7680U;
    if (m < 3840U) {
        return s_imdct_cos_lut[m];
    } else {
        return -s_imdct_cos_lut[m - 3840U];
    }
}

void aac_imdct_init(aac_imdct_state_t *p_imdct) {
    if (p_imdct == NULL) {
        return;
    }
    for (uint8_t ch = 0U; ch < 2U; ch++) {
        for (uint16_t i = 0U; i < AAC_MAX_CORE_SAMPLES; i++) {
            p_imdct->overlap[ch][i] = 0.0f;
        }
    }
    aac_imdct_ensure_table();
}

static void aac_imdct_long(const float *p_spec, float *p_time_out) {
    aac_imdct_ensure_table();
    const float factor = 2.0f / 960.0f;

    for (uint32_t n = 0U; n < 1920U; n++) {
        float sum = 0.0f;
        uint32_t n_part = 2U * n + 1U + 960U;
        for (uint32_t k = 0U; k < 960U; k++) {
            if (p_spec[k] != 0.0f) {
                uint32_t m = (n_part * (2U * k + 1U)) % 7680U;
                sum += p_spec[k] * aac_imdct_cos(m);
            }
        }
        p_time_out[n] = sum * factor;
    }
}

static void aac_imdct_short_single(const float *p_spec_120, float *p_time_out_240) {
    aac_imdct_ensure_table();
    const float factor = 2.0f / 120.0f;

    for (uint32_t n = 0U; n < 240U; n++) {
        float sum = 0.0f;
        uint32_t n_part = 2U * n + 1U + 120U;
        for (uint32_t k = 0U; k < 120U; k++) {
            if (p_spec_120[k] != 0.0f) {
                uint32_t m = (n_part * (2U * k + 1U) * 8U) % 7680U;
                sum += p_spec_120[k] * aac_imdct_cos(m);
            }
        }
        p_time_out_240[n] = sum * factor;
    }
}

void aac_imdct_process_channel(
    aac_imdct_state_t *p_imdct,
    uint8_t            channel,
    const float       *p_spec,
    uint16_t           num_spec_lines,
    float             *p_time_out,
    uint8_t            window_sequence,
    uint8_t            enable_neon
) {
    (void)enable_neon;
    (void)num_spec_lines;
    if ((p_imdct == NULL) || (p_spec == NULL) || (p_time_out == NULL) || (channel >= 2U)) {
        return;
    }

    float time_buf[1920];
    (void)memset(time_buf, 0, sizeof(time_buf));

    if (window_sequence == 2U) {
        /* EIGHT_SHORT_SEQUENCE: 8 short windows of 120 lines -> 240 samples each */
        for (uint16_t w = 0U; w < 8U; w++) {
            const float *short_spec = &p_spec[w * 120U];
            float short_time[240];
            aac_imdct_short_single(short_spec, short_time);

            /* Window with 240-point sine window and overlap-add at offset 420 + w * 120 */
            uint16_t offset = (uint16_t)(420U + w * 120U);
            for (uint16_t i = 0U; i < 240U; i++) {
                time_buf[offset + i] += short_time[i] * g_aac_window_sine_120[i];
            }
        }
    } else {
        /* Long transform (960 lines -> 1920 time samples) */
        aac_imdct_long(p_spec, time_buf);

        /* Windowing based on window sequence */
        if (window_sequence == 0U) {
            /* ONLY_LONG_SEQUENCE */
            for (uint16_t i = 0U; i < 1920U; i++) {
                time_buf[i] *= g_aac_window_sine_960[i];
            }
        } else if (window_sequence == 1U) {
            /* LONG_START_SEQUENCE: long sine (960), flat 1.0 (420), short sine right (120), flat 0 (420) */
            for (uint16_t i = 0U; i < 960U; i++) {
                time_buf[i] *= g_aac_window_sine_960[i];
            }
            /* 960..1379: 1.0 (no scaling) */
            for (uint16_t i = 0U; i < 120U; i++) {
                time_buf[1380U + i] *= g_aac_window_sine_120[120U + i];
            }
            for (uint16_t i = 1500U; i < 1920U; i++) {
                time_buf[i] = 0.0f;
            }
        } else {
            /* LONG_STOP_SEQUENCE (3U): flat 0 (420), short sine left (120), flat 1.0 (420), long sine (960) */
            for (uint16_t i = 0U; i < 420U; i++) {
                time_buf[i] = 0.0f;
            }
            for (uint16_t i = 0U; i < 120U; i++) {
                time_buf[420U + i] *= g_aac_window_sine_120[i];
            }
            /* 540..959: 1.0 (no scaling) */
            for (uint16_t i = 960U; i < 1920U; i++) {
                time_buf[i] *= g_aac_window_sine_960[i];
            }
        }
    }

    /* Overlap-Add: first 960 samples combine with stored overlap, last 960 become new overlap */
    for (uint16_t i = 0U; i < 960U; i++) {
        p_time_out[i] = time_buf[i] + p_imdct->overlap[channel][i];
        p_imdct->overlap[channel][i] = time_buf[960U + i];
    }
}
