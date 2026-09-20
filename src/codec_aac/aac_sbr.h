/**
 * @file    aac_sbr.h
 * @brief   Spectral Band Replication (SBR) HF Reconstruction & QMF Filterbank Header
 * @standard ETSI TS 102 563 & ISO/IEC 14496-3, MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef AAC_SBR_H
#define AAC_SBR_H

#include <stdint.h>
#include <stddef.h>
#include "../core/bitstream_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SBR_MAX_CHANNELS            (2U)
#define SBR_QMF_ANALYSIS_BANDS      (32U)
#define SBR_QMF_SYNTHESIS_BANDS     (64U)
#define SBR_MAX_OUTPUT_SAMPLES      (3840U)

typedef struct {
    uint8_t  sbr_header_present;
    uint8_t  bs_amp_res;
    uint8_t  bs_start_freq;
    uint8_t  bs_stop_freq;
    uint8_t  bs_xover_band;
    float    qmf_analysis_fifo[SBR_MAX_CHANNELS][320];
    float    qmf_synthesis_fifo[SBR_MAX_CHANNELS][1280];
    float    prev_envelope[SBR_MAX_CHANNELS][64];
} aac_sbr_state_t;

void aac_sbr_init(aac_sbr_state_t *p_sbr);

int32_t aac_sbr_parse_header(aac_sbr_state_t *p_sbr, bitstream_reader_t *p_bs);

void aac_sbr_process(
    aac_sbr_state_t *p_sbr,
    const float     *p_core_pcm[2],
    uint16_t         core_samples,
    float           *p_sbr_pcm[2],
    uint16_t        *p_out_samples,
    uint8_t          num_channels,
    uint8_t          enable_neon
);

#ifdef __cplusplus
}
#endif

#endif /* AAC_SBR_H */
