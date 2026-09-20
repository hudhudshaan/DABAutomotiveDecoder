/**
 * @file    concealment.c
 * @brief   Automotive Multi-Stage Concealment Engine Implementation (MS-3)
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "concealment.h"
#include "../dsp/dsp_math.h"
#include <stddef.h>

void concealment_init(concealment_engine_t *conc, const DAB_ConcealmentConfig *cfg) {
    if (conc == NULL) {
        return;
    }
    if (cfg != NULL) {
        conc->config = *cfg;
    } else {
        conc->config.short_loss_thresh = 2U;
        conc->config.long_loss_thresh  = 6U;
        conc->config.reserved[0]       = 0U;
        conc->config.reserved[1]       = 0U;
        conc->config.attn_step_q15     = 29491; /* ~0.9 in Q15 */
    }

    conc->mode            = DAB_CONCEAL_NONE;
    conc->consec_loss_cnt = 0U;
    conc->prev_pcm_valid  = 0U;
    conc->prev_pcm_count  = 0U;
    conc->attn_gain_q15   = DAB_Q15_ONE;
    dsp_vector_zero_s16_c(conc->prev_pcm, (uint16_t)DAB_CONCEAL_HISTORY_MAX_SAMPLES);
}

void concealment_process(
    concealment_engine_t *conc,
    soft_mute_engine_t   *mute,
    DAB_AUStatus          au_status,
    const int16_t        *decoded_pcm_in,
    uint16_t              num_samples_per_ch,
    uint8_t               num_channels,
    int16_t              *pcm_out,
    uint8_t               enable_neon
) {
    if ((conc == NULL) || (pcm_out == NULL) || (num_samples_per_ch == 0U) || (num_channels == 0U)) {
        return;
    }

    uint16_t total_samples = (uint16_t)(num_samples_per_ch * num_channels);
    if (total_samples > DAB_CONCEAL_HISTORY_MAX_SAMPLES) {
        total_samples = DAB_CONCEAL_HISTORY_MAX_SAMPLES;
    }

    if (au_status == DAB_AU_GOOD) {
        /* Frame is healthy */
        conc->consec_loss_cnt = 0U;
        conc->attn_gain_q15   = DAB_Q15_ONE;
        conc->mode            = DAB_CONCEAL_NONE;

        if (decoded_pcm_in != NULL) {
            dsp_pcm_copy(pcm_out, decoded_pcm_in, total_samples, enable_neon);
            dsp_pcm_copy(conc->prev_pcm, decoded_pcm_in, total_samples, enable_neon);
            conc->prev_pcm_count = total_samples;
            conc->prev_pcm_valid = 1U;
        } else {
            dsp_pcm_zero(pcm_out, total_samples, enable_neon);
        }

        /* Recover from mute if needed */
        if (mute != NULL) {
            if ((mute->state == DAB_MUTE_ATTACKING) || (mute->state == DAB_MUTE_SUSTAIN)) {
                soft_mute_trigger_release(mute);
            }
        }
        return;
    }

    /* CRC Error or Lost AU */
    if (conc->consec_loss_cnt < 255U) {
        conc->consec_loss_cnt++;
    }

    if (conc->consec_loss_cnt <= conc->config.short_loss_thresh) {
        /* Stage 1: Parametric Interpolation (Linear fade-down from previous frame) */
        conc->mode = DAB_CONCEAL_INTERPOLATE;
        if (conc->prev_pcm_valid != 0U) {
            uint16_t cnt = (conc->prev_pcm_count < total_samples) ? conc->prev_pcm_count : total_samples;
            dsp_pcm_copy(pcm_out, conc->prev_pcm, cnt, enable_neon);
            if (cnt < total_samples) {
                dsp_pcm_zero(&pcm_out[cnt], (uint16_t)(total_samples - cnt), enable_neon);
            }
            dsp_apply_ramp(pcm_out, total_samples, DAB_Q15_ONE, 0, enable_neon);
        } else {
            dsp_pcm_zero(pcm_out, total_samples, enable_neon);
        }
    } else if (conc->consec_loss_cnt <= conc->config.long_loss_thresh) {
        /* Stage 2: Attenuation Curve */
        conc->mode = DAB_CONCEAL_ATTENUATE;
        conc->attn_gain_q15 = ((int32_t)conc->attn_gain_q15 * (int32_t)conc->config.attn_step_q15 + 16384) >> 15;
        if (conc->attn_gain_q15 > DAB_Q15_ONE) {
            conc->attn_gain_q15 = DAB_Q15_ONE;
        } else if (conc->attn_gain_q15 < 0) {
            conc->attn_gain_q15 = 0;
        } else {
            /* in range */
        }

        if (conc->prev_pcm_valid != 0U) {
            uint16_t cnt = (conc->prev_pcm_count < total_samples) ? conc->prev_pcm_count : total_samples;
            dsp_apply_gain(conc->prev_pcm, pcm_out, cnt, (int16_t)conc->attn_gain_q15, enable_neon);
            if (cnt < total_samples) {
                dsp_pcm_zero(&pcm_out[cnt], (uint16_t)(total_samples - cnt), enable_neon);
            }
        } else {
            dsp_pcm_zero(pcm_out, total_samples, enable_neon);
        }
    } else {
        /* Stage 3: Full Soft Mute */
        conc->mode = DAB_CONCEAL_MUTED;
        if (mute != NULL) {
            soft_mute_trigger_attack(mute);
        }
        dsp_pcm_zero(pcm_out, total_samples, enable_neon);
    }
}

void concealment_reset(concealment_engine_t *conc) {
    if (conc == NULL) {
        return;
    }
    conc->mode            = DAB_CONCEAL_NONE;
    conc->consec_loss_cnt = 0U;
    conc->prev_pcm_valid  = 0U;
    conc->prev_pcm_count  = 0U;
    conc->attn_gain_q15   = DAB_Q15_ONE;
    dsp_vector_zero_s16_c(conc->prev_pcm, (uint16_t)DAB_CONCEAL_HISTORY_MAX_SAMPLES);
}
