/**
 * @file    aac_decoder.h
 * @brief   DAB+ HE-AAC v2 (AAC-LC + SBR + PS) Core AU Decoder Header (ETSI TS 102 563)
 * @standard ETSI TS 102 563 & ISO/IEC 14496-3, MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef AAC_DECODER_H
#define AAC_DECODER_H

#include <stdint.h>
#include <stddef.h>
#include "aac_imdct.h"
#include "aac_sbr.h"
#include "aac_ps.h"
#include "dab_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    aac_imdct_state_t imdct_state;
    aac_sbr_state_t   sbr_state;
    aac_ps_state_t    ps_state;
    float             spec[2][960];
    float             core_pcm[2][960];
    float             sbr_pcm[2][1920];
    int16_t           scalefactors[2][64];
    uint32_t          sample_rate_hz;
    uint8_t           num_channels;
    uint8_t           sbr_active;
    uint8_t           ps_active;
} aac_decoder_state_t;

void aac_decoder_init(aac_decoder_state_t *p_state);

int32_t aac_decoder_process_au(
    aac_decoder_state_t *p_state,
    const uint8_t       *p_au_data,
    uint16_t             au_len,
    int16_t             *pcm_out_interleaved,
    uint16_t            *p_num_samples_per_ch,
    uint8_t              enable_neon
);

#ifdef __cplusplus
}
#endif

#endif /* AAC_DECODER_H */
