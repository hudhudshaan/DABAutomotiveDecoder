/**
 * @file    pop_prevention.h
 * @brief   Automotive Pop-Noise Prevention via Smooth Crossfade
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef POP_PREVENTION_H
#define POP_PREVENTION_H

#include <stdint.h>
#include "dab_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t sample_rate_hz;
    uint16_t crossfade_samples;
    uint8_t  in_transition;
} pop_prevention_t;

void pop_prevention_init(pop_prevention_t *pop, uint32_t sample_rate_hz);
void pop_prevention_apply(pop_prevention_t *pop, int16_t *pcm, uint16_t num_samples_per_ch, uint8_t num_channels, uint8_t is_recovery_frame);
void pop_prevention_reset(pop_prevention_t *pop);

#ifdef __cplusplus
}
#endif

#endif /* POP_PREVENTION_H */
