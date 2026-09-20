/**
 * @file    mp2_tables.h
 * @brief   ISO/IEC 11172-3 MPEG-1/2 Audio Layer II (MUSICAM) Precomputed Lookup Tables Header
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef MP2_TABLES_H
#define MP2_TABLES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t nbal;          /**< Number of bits for bit allocation field */
    uint8_t quant_idx[16]; /**< Quantization step class indices */
} mp2_subband_alloc_t;

/* 64-Entry Scale Factor Table per ISO/IEC 11172-3 Table C.1 */
extern const float g_mp2_scale_factors[64];

/* Quantization Multiplier (C) and Offset (D) Coefficients per ISO/IEC 11172-3 Table B.4 */
extern const float g_mp2_c_coeff[17];
extern const float g_mp2_d_coeff[17];
extern const uint8_t g_mp2_bits_per_sample[17];

/* ISO Bit Allocation Tables per ISO 11172-3 Table B.2a (48kHz) and Table B.2b (24kHz) */
extern const mp2_subband_alloc_t g_mp2_alloc_table_48k[32];
extern const mp2_subband_alloc_t g_mp2_alloc_table_24k[32];

/* Synthesis filter matrix coefficients */
extern const float g_mp2_synth_window[512];

#ifdef __cplusplus
}
#endif

#endif /* MP2_TABLES_H */
