/**
 * @file    mp2_decoder.h
 * @brief   DAB MUSICAM (ISO/IEC 11172-3 Layer II) Core AU Decoder Header
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef MP2_DECODER_H
#define MP2_DECODER_H

#include <stdint.h>
#include <stddef.h>
#include "mp2_synth.h"
#include "mp2_tables.h"
#include "dab_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t           bit_alloc[2][32];
    uint8_t           scfsi[2][32];
    float             scale_factors[2][32][3];
    float             sb_samples[2][36][32];
    mp2_synth_state_t synth_state;
    uint32_t          sample_rate_hz;
    uint8_t           num_channels;
    uint8_t           mode;
    uint8_t           mode_extension;
} mp2_decoder_state_t;

void mp2_decoder_init(mp2_decoder_state_t *p_state);

int32_t mp2_decoder_process_au(
    mp2_decoder_state_t *p_state,
    const uint8_t       *p_au_data,
    uint16_t             au_len,
    int16_t             *pcm_out_interleaved,
    uint16_t            *p_num_samples_per_ch,
    uint8_t              enable_neon
);

#ifdef __cplusplus
}
#endif

#endif /* MP2_DECODER_H */
