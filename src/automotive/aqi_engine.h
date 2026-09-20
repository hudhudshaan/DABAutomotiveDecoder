/**
 * @file    aqi_engine.h
 * @brief   Asymmetric Audio Quality Index (AQI) & 2-bit Seamless Blending Trigger (MS-4)
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef AQI_ENGINE_H
#define AQI_ENGINE_H

#include <stdint.h>
#include "dab_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t                  quality_score;     /**< 0..100 */
    uint8_t                  rise_step;         /**< Default: 2 */
    DAB_BlendingTriggerState trigger;           /**< 2-bit trigger flag */
    DAB_BlendingTriggerState prev_trigger;      /**< Previous trigger state */
} aqi_engine_t;

void aqi_init(aqi_engine_t *aqi, uint8_t rise_step);
void aqi_update(aqi_engine_t *aqi, DAB_AUStatus au_status, DAB_ConcealmentMode conceal_mode);
void aqi_set_rise_step(aqi_engine_t *aqi, uint8_t rise_step);
void aqi_reset(aqi_engine_t *aqi);

#ifdef __cplusplus
}
#endif

#endif /* AQI_ENGINE_H */
