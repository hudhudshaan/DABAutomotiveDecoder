/**
 * @file    mp2_synth.h
 * @brief   Polyphase 32-Subband Synthesis Filterbank Header for DAB MUSICAM (ISO/IEC 11172-3)
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef MP2_SYNTH_H
#define MP2_SYNTH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float fifo_buffer[2][512]; /**< Synthesis FIFO delay line per channel */
} mp2_synth_state_t;

void mp2_synth_init(mp2_synth_state_t *p_synth);
void mp2_synth_process_subband_block(
    mp2_synth_state_t *p_synth,
    float subband_samples[2][32],
    float pcm_out_samples[2][32],
    uint8_t num_channels,
    uint8_t enable_neon
);

#ifdef __cplusplus
}
#endif

#endif /* MP2_SYNTH_H */
