/**
 * @file    dsp_math.h
 * @brief   DSP Math Primitives, Fixed-Point Helpers & NEON Intrinsics Abstraction
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef DSP_MATH_H
#define DSP_MATH_H

#include <stdint.h>
#include <stddef.h>

/* ARM64 NEON Detection: MUST use && (AND) per automotive audit */
#if defined(__ARM_NEON) && defined(__aarch64__)
#include <arm_neon.h>
#define DAB_HAS_NEON 1
#else
#define DAB_HAS_NEON 0
#endif

#define DAB_Q15_ONE     (32767)
#define DAB_Q15_ZERO    (0)
#define DAB_PI_F        (3.14159265358979323846f)

#ifdef __cplusplus
extern "C" {
#endif

/* Clamping utility */
static inline int16_t dsp_clamp16(int32_t val) {
    if (val > 32767) {
        return (int16_t)32767;
    } else if (val < -32768) {
        return (int16_t)-32768;
    } else {
        return (int16_t)val;
    }
}

/* Q15 multiply and accumulate */
static inline int16_t dsp_mult_q15(int16_t a, int16_t b) {
    int32_t prod = ((int32_t)a * (int32_t)b + 16384) >> 15;
    return dsp_clamp16(prod);
}

/* Fast LUT-based cosine approximation in Q15 [0..128 indices for 0..pi] */
int16_t dsp_cos_q15(uint32_t idx);

/* Float trigonometric approximations (no double precision) */
float dsp_sin_f32(float rad);
float dsp_cos_f32(float rad);

/* Vector operations (pure C fallbacks) */
void dsp_vector_scale_q15_c(const int16_t *src, int16_t *dst, uint16_t count, int16_t gain_q15);
void dsp_vector_copy_s16_c(int16_t *dst, const int16_t *src, uint16_t count);
void dsp_vector_zero_s16_c(int16_t *dst, uint16_t count);
void dsp_vector_ramp_s16_c(int16_t *dst, uint16_t count, int32_t start_q15, int32_t end_q15);

/* NEON SIMD Accelerated Vector Operations */
#if DAB_HAS_NEON
void dsp_vector_scale_q15_neon(const int16_t *src, int16_t *dst, uint16_t count, int16_t gain_q15);
void dsp_vector_copy_s16_neon(int16_t *dst, const int16_t *src, uint16_t count);
void dsp_vector_zero_s16_neon(int16_t *dst, uint16_t count);
void dsp_vector_ramp_s16_neon(int16_t *dst, uint16_t count, int32_t start_q15, int32_t end_q15);
#endif

/* Dispatchers that select NEON if available or C fallback */
void dsp_apply_gain(const int16_t *src, int16_t *dst, uint16_t count, int16_t gain_q15, uint8_t enable_neon);
void dsp_pcm_copy(int16_t *dst, const int16_t *src, uint16_t count, uint8_t enable_neon);
void dsp_pcm_zero(int16_t *dst, uint16_t count, uint8_t enable_neon);
void dsp_apply_ramp(int16_t *dst, uint16_t count, int32_t start_q15, int32_t end_q15, uint8_t enable_neon);

#ifdef __cplusplus
}
#endif

#endif /* DSP_MATH_H */
