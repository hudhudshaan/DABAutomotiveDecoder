/**
 * @file    dsp_math.c
 * @brief   DSP Math Primitives & Pure C Vector Operations
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "dsp_math.h"
#include <math.h>

/* 128-entry Q15 Half-Cosine descending LUT (from 32767 down to 0) */
static const int16_t s_cos_lut[128] = {
    32767, 32757, 32728, 32679, 32609, 32520, 32411, 32283,
    32135, 31968, 31782, 31577, 31353, 31110, 30849, 30570,
    30273, 29958, 29625, 29275, 28908, 28524, 28123, 27706,
    27273, 26824, 26360, 25881, 25387, 24879, 24357, 23822,
    23274, 22714, 22141, 21557, 20962, 20356, 19740, 19114,
    18480, 17836, 17185, 16526, 15860, 15188, 14509, 13826,
    13137, 12445, 11749, 11050, 10349,  9647,  8944,  8241,
     7539,  6838,  6139,  5442,  4749,  4060,  3375,  2696,
     2023,  1356,   697,    45,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0
};

int16_t dsp_cos_q15(uint32_t idx) {
    if (idx >= 128U) {
        return 0;
    }
    return s_cos_lut[idx];
}

/* Float trigonometric functions using C99 libm */
float dsp_sin_f32(float rad) {
    return sinf(rad);
}

float dsp_cos_f32(float rad) {
    return cosf(rad);
}


void dsp_vector_scale_q15_c(const int16_t *src, int16_t *dst, uint16_t count, int16_t gain_q15) {
    if ((src == NULL) || (dst == NULL) || (count == 0U)) {
        return;
    }
    for (uint16_t i = 0U; i < count; i++) {
        int32_t temp = ((int32_t)src[i] * (int32_t)gain_q15 + 16384) >> 15;
        dst[i] = dsp_clamp16(temp);
    }
}

void dsp_vector_copy_s16_c(int16_t *dst, const int16_t *src, uint16_t count) {
    if ((dst == NULL) || (src == NULL) || (count == 0U)) {
        return;
    }
    for (uint16_t i = 0U; i < count; i++) {
        dst[i] = src[i];
    }
}

void dsp_vector_zero_s16_c(int16_t *dst, uint16_t count) {
    if ((dst == NULL) || (count == 0U)) {
        return;
    }
    for (uint16_t i = 0U; i < count; i++) {
        dst[i] = 0;
    }
}

void dsp_vector_ramp_s16_c(int16_t *dst, uint16_t count, int32_t start_q15, int32_t end_q15) {
    if ((dst == NULL) || (count == 0U)) {
        return;
    }
    if (count == 1U) {
        int32_t temp = ((int32_t)dst[0] * start_q15 + 16384) >> 15;
        dst[0] = dsp_clamp16(temp);
        return;
    }
    for (uint16_t i = 0U; i < count; i++) {
        int32_t gain_i = start_q15 + ((end_q15 - start_q15) * (int32_t)i) / (int32_t)(count - 1U);
        if (gain_i > 32767) { gain_i = 32767; }
        if (gain_i < 0) { gain_i = 0; }
        int32_t temp = ((int32_t)dst[i] * gain_i + 16384) >> 15;
        dst[i] = dsp_clamp16(temp);
    }
}

/* Dispatchers */
void dsp_apply_gain(const int16_t *src, int16_t *dst, uint16_t count, int16_t gain_q15, uint8_t enable_neon) {
#if DAB_HAS_NEON
    if (enable_neon != 0U) {
        dsp_vector_scale_q15_neon(src, dst, count, gain_q15);
        return;
    }
#else
    (void)enable_neon;
#endif
    dsp_vector_scale_q15_c(src, dst, count, gain_q15);
}

void dsp_pcm_copy(int16_t *dst, const int16_t *src, uint16_t count, uint8_t enable_neon) {
#if DAB_HAS_NEON
    if (enable_neon != 0U) {
        dsp_vector_copy_s16_neon(dst, src, count);
        return;
    }
#else
    (void)enable_neon;
#endif
    dsp_vector_copy_s16_c(dst, src, count);
}

void dsp_pcm_zero(int16_t *dst, uint16_t count, uint8_t enable_neon) {
#if DAB_HAS_NEON
    if (enable_neon != 0U) {
        dsp_vector_zero_s16_neon(dst, count);
        return;
    }
#else
    (void)enable_neon;
#endif
    dsp_vector_zero_s16_c(dst, count);
}

void dsp_apply_ramp(int16_t *dst, uint16_t count, int32_t start_q15, int32_t end_q15, uint8_t enable_neon) {
#if DAB_HAS_NEON
    if (enable_neon != 0U) {
        dsp_vector_ramp_s16_neon(dst, count, start_q15, end_q15);
        return;
    }
#else
    (void)enable_neon;
#endif
    dsp_vector_ramp_s16_c(dst, count, start_q15, end_q15);
}
