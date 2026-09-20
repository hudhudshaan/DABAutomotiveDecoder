/**
 * @file    aqi_engine.c
 * @brief   Asymmetric AQI & 2-bit Seamless Blending Trigger Implementation (MS-4)
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "aqi_engine.h"
#include <stddef.h>

void aqi_init(aqi_engine_t *aqi, uint8_t rise_step) {
    if (aqi == NULL) {
        return;
    }
    aqi->quality_score = DAB_QUALITY_MIN;
    aqi->rise_step     = (rise_step >= DAB_QUALITY_RISE_STEP_MIN && rise_step <= DAB_QUALITY_RISE_STEP_MAX) ? rise_step : DAB_QUALITY_RISE_STEP_DEFAULT;
    aqi->trigger       = BLEND_IDLE;
    aqi->prev_trigger  = BLEND_IDLE;
}

void aqi_update(aqi_engine_t *aqi, DAB_AUStatus au_status, DAB_ConcealmentMode conceal_mode) {
    if (aqi == NULL) {
        return;
    }

    aqi->prev_trigger = aqi->trigger;

    /* 1. Quality Score Update: Fast-Drop / Slow-Rise */
    if (au_status == DAB_AU_GOOD) {
        uint32_t next_score = (uint32_t)aqi->quality_score + (uint32_t)aqi->rise_step;
        if (next_score >= (uint32_t)DAB_QUALITY_MAX) {
            aqi->quality_score = (uint8_t)DAB_QUALITY_MAX;
        } else {
            aqi->quality_score = (uint8_t)next_score;
        }
    } else {
        /* Instant Drop on CRC Error or Lost AU */
        aqi->quality_score = (uint8_t)DAB_QUALITY_MIN;
    }

    /* 2. Blending Trigger Flags Evaluation */
    switch (conceal_mode) {
        case DAB_CONCEAL_NONE:
            aqi->trigger = BLEND_IDLE;
            break;

        case DAB_CONCEAL_INTERPOLATE:
        case DAB_CONCEAL_ATTENUATE:
            aqi->trigger = CONCEAL_TRIGGER;
            break;

        case DAB_CONCEAL_MUTED:
            aqi->trigger = UNRECOVERABLE_TRIGGER;
            break;

        default:
            aqi->trigger = BLEND_IDLE;
            break;
    }
}

void aqi_set_rise_step(aqi_engine_t *aqi, uint8_t rise_step) {
    if (aqi == NULL) {
        return;
    }
    if ((rise_step >= DAB_QUALITY_RISE_STEP_MIN) && (rise_step <= DAB_QUALITY_RISE_STEP_MAX)) {
        aqi->rise_step = rise_step;
    }
}

void aqi_reset(aqi_engine_t *aqi) {
    if (aqi == NULL) {
        return;
    }
    aqi->quality_score = DAB_QUALITY_MIN;
    aqi->trigger       = BLEND_IDLE;
    aqi->prev_trigger  = BLEND_IDLE;
}
