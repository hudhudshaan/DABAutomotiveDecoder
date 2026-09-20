/**
 * @file    aac_imdct.h
 * @brief   960-Point & 120-Point IMDCT & Overlap-Add for DAB+ (ETSI TS 102 563)
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef AAC_IMDCT_H
#define AAC_IMDCT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AAC_MAX_CORE_SAMPLES   (960U)

typedef struct {
    float overlap[2][AAC_MAX_CORE_SAMPLES]; /**< Overlap buffer per channel */
} aac_imdct_state_t;

void aac_imdct_init(aac_imdct_state_t *p_imdct);

void aac_imdct_process_channel(
    aac_imdct_state_t *p_imdct,
    uint8_t            channel,
    const float       *p_spec,
    uint16_t           num_spec_lines,
    float             *p_time_out,
    uint8_t            window_sequence,
    uint8_t            enable_neon
);

#ifdef __cplusplus
}
#endif

#endif /* AAC_IMDCT_H */
