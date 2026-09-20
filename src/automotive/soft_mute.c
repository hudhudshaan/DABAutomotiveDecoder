/**
 * @file    soft_mute.c
 * @brief   Automotive Soft Mute Gain Ramp Engine Implementation (MS-3)
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "soft_mute.h"
#include "../dsp/dsp_math.h"
#include <stddef.h>

void soft_mute_recalc_timing(soft_mute_engine_t *mute) {
    if (mute == NULL) {
        return;
    }
    if (mute->sample_rate_hz == 0U) {
        mute->sample_rate_hz = 48000U;
    }
    mute->attack_samples  = (mute->attack_ms * mute->sample_rate_hz) / 1000U;
    mute->release_samples = (mute->release_ms * mute->sample_rate_hz) / 1000U;

    if (mute->attack_samples < 1U) {
        mute->attack_samples = 1U;
    }
    if (mute->release_samples < 1U) {
        mute->release_samples = 1U;
    }
}

void soft_mute_init(
    soft_mute_engine_t *mute,
    uint32_t sample_rate_hz,
    uint32_t attack_ms,
    uint32_t release_ms,
    DAB_RampCurve curve
) {
    if (mute == NULL) {
        return;
    }
    mute->sample_rate_hz = sample_rate_hz;
    mute->attack_ms      = (attack_ms >= DAB_ATTACK_MS_MIN && attack_ms <= DAB_ATTACK_MS_MAX) ? attack_ms : DAB_ATTACK_MS_DEFAULT;
    mute->release_ms     = (release_ms >= DAB_RELEASE_MS_MIN && release_ms <= DAB_RELEASE_MS_MAX) ? release_ms : DAB_RELEASE_MS_DEFAULT;
    mute->curve          = curve;
    mute->state          = DAB_MUTE_IDLE;
    mute->ramp_pos       = 0U;
    mute->cur_gain_q15   = DAB_Q15_ONE;

    soft_mute_recalc_timing(mute);
}

static int32_t soft_mute_calc_gain(uint32_t pos, uint32_t total, DAB_RampCurve curve, uint8_t is_attacking) {
    if (total == 0U) {
        return is_attacking ? DAB_Q15_ZERO : DAB_Q15_ONE;
    }
    if (pos >= total) {
        return is_attacking ? DAB_Q15_ZERO : DAB_Q15_ONE;
    }

    int32_t raw_gain = DAB_Q15_ONE;

    switch (curve) {
        case DAB_RAMP_LINEAR:
            raw_gain = (int32_t)(((uint32_t)DAB_Q15_ONE * (total - pos)) / total);
            break;

        case DAB_RAMP_COSINE: {
            uint32_t idx = (pos * 127U) / total;
            raw_gain = (int32_t)dsp_cos_q15(idx);
            break;
        }

        case DAB_RAMP_EXPONENTIAL: {
            /* Squared decay approximation */
            uint32_t rem = total - pos;
            uint64_t num = (uint64_t)DAB_Q15_ONE * (uint64_t)rem * (uint64_t)rem;
            uint64_t den = (uint64_t)total * (uint64_t)total;
            raw_gain = (den > 0U) ? (int32_t)(num / den) : 0;
            break;
        }

        default:
            raw_gain = (int32_t)(((uint32_t)DAB_Q15_ONE * (total - pos)) / total);
            break;
    }

    if (is_attacking == 0U) {
        raw_gain = DAB_Q15_ONE - raw_gain;
    }

    if (raw_gain > DAB_Q15_ONE) {
        raw_gain = DAB_Q15_ONE;
    } else if (raw_gain < DAB_Q15_ZERO) {
        raw_gain = DAB_Q15_ZERO;
    } else {
        /* in range */
    }

    return raw_gain;
}

void soft_mute_trigger_attack(soft_mute_engine_t *mute) {
    if (mute == NULL) {
        return;
    }
    if ((mute->state == DAB_MUTE_IDLE) || (mute->state == DAB_MUTE_RELEASING)) {
        mute->state = DAB_MUTE_ATTACKING;
        mute->ramp_pos = 0U;
    } else {
        /* Already attacking or in sustain */
    }
}

void soft_mute_trigger_release(soft_mute_engine_t *mute) {
    if (mute == NULL) {
        return;
    }
    if ((mute->state == DAB_MUTE_SUSTAIN) || (mute->state == DAB_MUTE_ATTACKING)) {
        mute->state = DAB_MUTE_RELEASING;
        mute->ramp_pos = 0U;
    } else {
        /* Already releasing or idle */
    }
}

void soft_mute_apply(
    soft_mute_engine_t *mute,
    int16_t *pcm_interleaved,
    uint16_t num_samples_per_ch,
    uint8_t num_channels,
    uint8_t enable_neon
) {
    if ((mute == NULL) || (pcm_interleaved == NULL) || (num_samples_per_ch == 0U) || (num_channels == 0U)) {
        return;
    }

    uint16_t total_samples = (uint16_t)(num_samples_per_ch * num_channels);

    if (mute->state == DAB_MUTE_IDLE) {
        mute->cur_gain_q15 = DAB_Q15_ONE;
        return;
    }

    if (mute->state == DAB_MUTE_SUSTAIN) {
        mute->cur_gain_q15 = DAB_Q15_ZERO;
        dsp_pcm_zero(pcm_interleaved, total_samples, enable_neon);
        return;
    }

    uint8_t is_attacking = (mute->state == DAB_MUTE_ATTACKING) ? 1U : 0U;
    uint32_t ramp_total = is_attacking ? mute->attack_samples : mute->release_samples;

    for (uint16_t s = 0U; s < num_samples_per_ch; s++) {
        int32_t gain = soft_mute_calc_gain(mute->ramp_pos, ramp_total, mute->curve, is_attacking);
        mute->cur_gain_q15 = gain;

        for (uint8_t ch = 0U; ch < num_channels; ch++) {
            uint16_t idx = (uint16_t)(s * num_channels + ch);
            int32_t val = ((int32_t)pcm_interleaved[idx] * gain + 16384) >> 15;
            pcm_interleaved[idx] = dsp_clamp16(val);
        }

        mute->ramp_pos++;
        if (mute->ramp_pos >= ramp_total) {
            if (mute->state == DAB_MUTE_ATTACKING) {
                mute->state = DAB_MUTE_SUSTAIN;
                mute->cur_gain_q15 = DAB_Q15_ZERO;
            } else if (mute->state == DAB_MUTE_RELEASING) {
                mute->state = DAB_MUTE_IDLE;
                mute->cur_gain_q15 = DAB_Q15_ONE;
            } else {
                /* no state transition */
            }
            break;
        }
    }
}

int32_t soft_mute_get_gain_q15(const soft_mute_engine_t *mute) {
    if (mute == NULL) {
        return DAB_Q15_ONE;
    }
    return mute->cur_gain_q15;
}

void soft_mute_reset(soft_mute_engine_t *mute) {
    if (mute == NULL) {
        return;
    }
    mute->state        = DAB_MUTE_IDLE;
    mute->ramp_pos     = 0U;
    mute->cur_gain_q15 = DAB_Q15_ONE;
}
