/**
 * @file    dsp_neon.c
 * @brief   ARM64 NEON Vectorized DSP Kernels
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "dsp_math.h"

#if DAB_HAS_NEON

void dsp_vector_scale_q15_neon(const int16_t *src, int16_t *dst, uint16_t count, int16_t gain_q15) {
    if ((src == NULL) || (dst == NULL) || (count == 0U)) {
        return;
    }
    uint16_t i = 0U;
    int16x8_t vgain = vdupq_n_s16(gain_q15);
    
    /* Process 8 int16 samples per iteration */
    for (; (i + 8U) <= count; i += 8U) {
        int16x8_t vin = vld1q_s16(&src[i]);
        /* vqdmulhq_s16: saturating doubling high half multiplication, effectively (a * b) >> 15 */
        int16x8_t vout = vqdmulhq_s16(vin, vgain);
        vst1q_s16(&dst[i], vout);
    }
    /* Scalar cleanup */
    for (; i < count; i++) {
        int32_t temp = ((int32_t)src[i] * (int32_t)gain_q15 + 16384) >> 15;
        dst[i] = dsp_clamp16(temp);
    }
}

void dsp_vector_copy_s16_neon(int16_t *dst, const int16_t *src, uint16_t count) {
    if ((dst == NULL) || (src == NULL) || (count == 0U)) {
        return;
    }
    uint16_t i = 0U;
    for (; (i + 8U) <= count; i += 8U) {
        int16x8_t v = vld1q_s16(&src[i]);
        vst1q_s16(&dst[i], v);
    }
    for (; i < count; i++) {
        dst[i] = src[i];
    }
}

void dsp_vector_zero_s16_neon(int16_t *dst, uint16_t count) {
    if ((dst == NULL) || (count == 0U)) {
        return;
    }
    uint16_t i = 0U;
    int16x8_t vz = vdupq_n_s16(0);
    for (; (i + 8U) <= count; i += 8U) {
        vst1q_s16(&dst[i], vz);
    }
    for (; i < count; i++) {
        dst[i] = 0;
    }
}

void dsp_vector_ramp_s16_neon(int16_t *dst, uint16_t count, int32_t start_q15, int32_t end_q15) {
    /* Fall back to scalar for sample-accurate per-sample gain interpolation */
    dsp_vector_ramp_s16_c(dst, count, start_q15, end_q15);
}

#endif /* DAB_HAS_NEON */
