/**
 * @file    dab_instance.c
 * @brief   Master Multi-Instance Decoder Construction & AU Processing Pipeline
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "dab_internal.h"
#include <string.h>

/* Compile-time static handle size assertions */
typedef char dab_mp2_size_check[(sizeof(DAB_Instance_Context) <= DAB_DECODER_AAC_HANDLE_SIZE) ? 1 : -1];

size_t DAB_Decoder_GetHandleSize(DAB_CodecType codec_type) {
    if (codec_type == DAB_CODEC_MP2) {
        return DAB_DECODER_MP2_HANDLE_SIZE;
    }
    return DAB_DECODER_AAC_HANDLE_SIZE;
}

DAB_Decoder_Handle DAB_Decoder_InitWithMem(
    void           *buf,
    size_t          buf_size,
    DAB_CodecType   codec_type,
    uint32_t        sample_rate_hz
) {
    size_t req_size = (codec_type == DAB_CODEC_MP2) ?
        DAB_DECODER_MP2_HANDLE_SIZE :
        DAB_DECODER_AAC_HANDLE_SIZE;

    if ((buf == NULL) || (buf_size < req_size)) {
        return NULL;
    }

    if ((sample_rate_hz != 16000U) && (sample_rate_hz != 24000U) &&
        (sample_rate_hz != 32000U) && (sample_rate_hz != 48000U)) {
        return NULL;
    }

    (void)memset(buf, 0, buf_size);
    DAB_Instance_Context *ctx = (DAB_Instance_Context *)buf;

    ctx->magic          = DAB_DECODER_MAGIC;
    ctx->codec_type     = codec_type;
    ctx->sample_rate_hz = sample_rate_hz;
    ctx->num_channels   = 2U;
    ctx->enable_neon    = DAB_HAS_NEON ? 1U : 0U;

    if (codec_type == DAB_CODEC_MP2) {
        mp2_decoder_init(&ctx->codec.mp2);
        ctx->codec.mp2.sample_rate_hz = sample_rate_hz;
    } else {
        aac_decoder_init(&ctx->codec.aac);
        ctx->codec.aac.sample_rate_hz = sample_rate_hz;
    }

    soft_mute_init(&ctx->mute, sample_rate_hz, DAB_ATTACK_MS_DEFAULT, DAB_RELEASE_MS_DEFAULT, DAB_RAMP_COSINE);
    concealment_init(&ctx->conceal, NULL);
    aqi_init(&ctx->aqi, DAB_QUALITY_RISE_STEP_DEFAULT);
    pop_prevention_init(&ctx->pop, sample_rate_hz);

    ctx->total_au_cnt     = 0U;
    ctx->crc_error_au_cnt = 0U;

    return (DAB_Decoder_Handle)ctx;
}

static void dab_upsample_2x_stereo(const int16_t *in_pcm, int16_t *out_pcm, uint16_t in_samples) {
    for (uint16_t n = 0U; n < in_samples; n++) {
        /* Even samples: exact copy of original samples */
        out_pcm[4U * n]      = in_pcm[2U * n];      /* L */
        out_pcm[4U * n + 1U] = in_pcm[2U * n + 1U]; /* R */

        /* Odd samples: 4-tap half-band FIR (-x[n-1] + 9x[n] + 9x[n+1] - x[n+2]) / 16 */
        int32_t l_prev  = (n > 0U) ? (int32_t)in_pcm[2U * (n - 1U)] : (int32_t)in_pcm[2U * n];
        int32_t l_curr  = (int32_t)in_pcm[2U * n];
        int32_t l_next  = ((n + 1U) < in_samples) ? (int32_t)in_pcm[2U * (n + 1U)] : l_curr;
        int32_t l_next2 = ((n + 2U) < in_samples) ? (int32_t)in_pcm[2U * (n + 2U)] : l_next;

        int32_t l_odd = (-l_prev + (9 * l_curr) + (9 * l_next) - l_next2 + 8) >> 4;
        if (l_odd > 32767) { l_odd = 32767; }
        if (l_odd < -32768) { l_odd = -32768; }
        out_pcm[4U * n + 2U] = (int16_t)l_odd;

        int32_t r_prev  = (n > 0U) ? (int32_t)in_pcm[2U * (n - 1U) + 1U] : (int32_t)in_pcm[2U * n + 1U];
        int32_t r_curr  = (int32_t)in_pcm[2U * n + 1U];
        int32_t r_next  = ((n + 1U) < in_samples) ? (int32_t)in_pcm[2U * (n + 1U) + 1U] : r_curr;
        int32_t r_next2 = ((n + 2U) < in_samples) ? (int32_t)in_pcm[2U * (n + 2U) + 1U] : r_next;

        int32_t r_odd = (-r_prev + (9 * r_curr) + (9 * r_next) - r_next2 + 8) >> 4;
        if (r_odd > 32767) { r_odd = 32767; }
        if (r_odd < -32768) { r_odd = -32768; }
        out_pcm[4U * n + 3U] = (int16_t)r_odd;
    }
}

int32_t DAB_Decoder_DecodeAU(
    DAB_Decoder_Handle          handle,
    const uint8_t              *au_data,
    uint16_t                    au_len,
    const DAB_SignalStatus     *signal_status,
    int16_t                    *pcm_out,
    DAB_AudioStatus            *status_out
) {
    DAB_Instance_Context *ctx = dab_get_context(handle);
    if ((ctx == NULL) || (pcm_out == NULL)) {
        return DAB_ERR_NULL_HANDLE;
    }

    /* 1. Evaluate AU CRC */
    DAB_AUStatus au_status = dab_au_evaluate_crc(
        au_data,
        au_len,
        signal_status,
        &ctx->total_au_cnt,
        &ctx->crc_error_au_cnt
    );

    uint16_t decoded_samples_per_ch = 0U;
    int32_t codec_ret = DAB_OK;

    /* 2. Decode Bitstream if AU is valid */
    if (au_status == DAB_AU_GOOD) {
        uint16_t payload_len = (uint16_t)(au_len - DAB_AU_CRC_BYTES);

        if (ctx->codec_type == DAB_CODEC_MP2) {
            codec_ret = mp2_decoder_process_au(
                &ctx->codec.mp2,
                au_data,
                payload_len,
                ctx->raw_pcm,
                &decoded_samples_per_ch,
                ctx->enable_neon
            );
            ctx->sample_rate_hz = ctx->codec.mp2.sample_rate_hz;
        } else {
            codec_ret = aac_decoder_process_au(
                &ctx->codec.aac,
                au_data,
                payload_len,
                ctx->raw_pcm,
                &decoded_samples_per_ch,
                ctx->enable_neon
            );
        }

        if (codec_ret != DAB_OK) {
            au_status = DAB_AU_CRC_ERR;
        } else if ((ctx->codec_type == DAB_CODEC_AAC) && (ctx->codec.aac.sbr_active != 0U) && (decoded_samples_per_ch == 960U)) {
            static int16_t s_temp_core[1920];
            (void)memcpy(s_temp_core, ctx->raw_pcm, 960U * 2U * sizeof(int16_t));
            dab_upsample_2x_stereo(s_temp_core, ctx->raw_pcm, 960U);
            decoded_samples_per_ch = 1920U;
        } else {
            /* Keep original sample count (960 for standalone AAC-LC, MP2, etc.) */
        }
    }

    /* Fallback sample count for lost/corrupted frames */
    if (decoded_samples_per_ch == 0U) {
        if (ctx->codec_type == DAB_CODEC_MP2) {
            decoded_samples_per_ch = (ctx->sample_rate_hz <= 24000U) ? DAB_MP2_SAMPLES_PER_AU_24K : DAB_MP2_SAMPLES_PER_AU_48K;
        } else {
            decoded_samples_per_ch = (ctx->codec.aac.sbr_active != 0U) ? DAB_AAC_SBR_SAMPLES_48K : DAB_AAC_CORE_SAMPLES_960;
        }
    }

    /* 3. Multi-Stage Concealment FSM */
    concealment_process(
        &ctx->conceal,
        &ctx->mute,
        au_status,
        (au_status == DAB_AU_GOOD) ? ctx->raw_pcm : NULL,
        decoded_samples_per_ch,
        ctx->num_channels,
        pcm_out,
        ctx->enable_neon
    );

    /* 4. Pop-Noise Prevention Crossfade */
    uint8_t is_recovery = ((au_status == DAB_AU_GOOD) && (ctx->conceal.consec_loss_cnt > 0U)) ? 1U : 0U;
    pop_prevention_apply(&ctx->pop, pcm_out, decoded_samples_per_ch, ctx->num_channels, is_recovery);

    /* 5. Soft Mute Gain Ramp Application */
    soft_mute_apply(&ctx->mute, pcm_out, decoded_samples_per_ch, ctx->num_channels, ctx->enable_neon);

    /* 6. Asymmetric Audio Quality Index & 2-bit Trigger Update */
    aqi_update(&ctx->aqi, au_status, ctx->conceal.mode);

    /* 7. Comprehensive Telemetry Report */
    if (status_out != NULL) {
        status_out->au_status          = au_status;
        status_out->total_au_cnt       = ctx->total_au_cnt;
        status_out->crc_error_au_cnt   = ctx->crc_error_au_cnt;
        status_out->conceal_mode       = ctx->conceal.mode;
        status_out->mute_state         = ctx->mute.state;
        status_out->cur_gain_q15       = ctx->mute.cur_gain_q15;
        status_out->consec_loss        = ctx->conceal.consec_loss_cnt;
        status_out->audio_quality      = ctx->aqi.quality_score;
        status_out->trigger            = ctx->aqi.trigger;
        status_out->pcm_samples_out    = decoded_samples_per_ch;
        status_out->sample_rate_hz     = ctx->sample_rate_hz;
        status_out->num_channels       = ctx->num_channels;
        status_out->reserved[0]        = 0U;
        status_out->reserved[1]        = 0U;
        status_out->reserved[2]        = 0U;
    }

    return (int32_t)decoded_samples_per_ch;
}

int32_t DAB_Decoder_SetConcealmentParam(
    DAB_Decoder_Handle          handle,
    const DAB_ConcealmentConfig *config
) {
    DAB_Instance_Context *ctx = dab_get_context(handle);
    if ((ctx == NULL) || (config == NULL)) {
        return DAB_ERR_NULL_HANDLE;
    }
    if (config->short_loss_thresh >= config->long_loss_thresh) {
        return DAB_ERR_INVALID_PARAM;
    }
    ctx->conceal.config = *config;
    return DAB_OK;
}

int32_t DAB_Decoder_SetMuteTime(
    DAB_Decoder_Handle  handle,
    uint32_t            attack_ms,
    uint32_t            release_ms
) {
    DAB_Instance_Context *ctx = dab_get_context(handle);
    if (ctx == NULL) {
        return DAB_ERR_NULL_HANDLE;
    }
    if ((attack_ms < DAB_ATTACK_MS_MIN) || (attack_ms > DAB_ATTACK_MS_MAX) ||
        (release_ms < DAB_RELEASE_MS_MIN) || (release_ms > DAB_RELEASE_MS_MAX)) {
        return DAB_ERR_INVALID_PARAM;
    }
    ctx->mute.attack_ms  = attack_ms;
    ctx->mute.release_ms = release_ms;
    soft_mute_recalc_timing(&ctx->mute);
    return DAB_OK;
}

int32_t DAB_Decoder_SetRampCurve(DAB_Decoder_Handle handle, DAB_RampCurve curve) {
    DAB_Instance_Context *ctx = dab_get_context(handle);
    if (ctx == NULL) {
        return DAB_ERR_NULL_HANDLE;
    }
    if ((curve != DAB_RAMP_LINEAR) && (curve != DAB_RAMP_COSINE) && (curve != DAB_RAMP_EXPONENTIAL)) {
        return DAB_ERR_INVALID_PARAM;
    }
    ctx->mute.curve = curve;
    return DAB_OK;
}

int32_t DAB_Decoder_SetQualityRiseStep(DAB_Decoder_Handle handle, uint8_t rise_step) {
    DAB_Instance_Context *ctx = dab_get_context(handle);
    if (ctx == NULL) {
        return DAB_ERR_NULL_HANDLE;
    }
    if ((rise_step < DAB_QUALITY_RISE_STEP_MIN) || (rise_step > DAB_QUALITY_RISE_STEP_MAX)) {
        return DAB_ERR_INVALID_PARAM;
    }
    aqi_set_rise_step(&ctx->aqi, rise_step);
    return DAB_OK;
}

int32_t DAB_Decoder_GetQualityStatus(
    DAB_Decoder_Handle       handle,
    uint8_t                 *quality_out,
    DAB_BlendingTriggerState *trigger_out
) {
    DAB_Instance_Context *ctx = dab_get_context(handle);
    if (ctx == NULL) {
        return DAB_ERR_NULL_HANDLE;
    }
    if (quality_out != NULL) {
        *quality_out = ctx->aqi.quality_score;
    }
    if (trigger_out != NULL) {
        *trigger_out = ctx->aqi.trigger;
    }
    return DAB_OK;
}

int32_t DAB_Decoder_GetAUStats(
    DAB_Decoder_Handle  handle,
    uint32_t           *total_out,
    uint32_t           *err_out
) {
    DAB_Instance_Context *ctx = dab_get_context(handle);
    if (ctx == NULL) {
        return DAB_ERR_NULL_HANDLE;
    }
    if (total_out != NULL) {
        *total_out = ctx->total_au_cnt;
    }
    if (err_out != NULL) {
        *err_out = ctx->crc_error_au_cnt;
    }
    return DAB_OK;
}

int32_t DAB_Decoder_ResetStats(DAB_Decoder_Handle handle) {
    DAB_Instance_Context *ctx = dab_get_context(handle);
    if (ctx == NULL) {
        return DAB_ERR_NULL_HANDLE;
    }
    ctx->total_au_cnt     = 0U;
    ctx->crc_error_au_cnt = 0U;
    aqi_reset(&ctx->aqi);
    return DAB_OK;
}

int32_t DAB_Decoder_Reset(DAB_Decoder_Handle handle) {
    DAB_Instance_Context *ctx = dab_get_context(handle);
    if (ctx == NULL) {
        return DAB_ERR_NULL_HANDLE;
    }

    /* Preserve configuration */
    DAB_CodecType         saved_codec = ctx->codec_type;
    uint32_t              saved_sr    = ctx->sample_rate_hz;
    DAB_ConcealmentConfig saved_cfg   = ctx->conceal.config;
    uint32_t              saved_att   = ctx->mute.attack_ms;
    uint32_t              saved_rel   = ctx->mute.release_ms;
    DAB_RampCurve         saved_curve = ctx->mute.curve;
    uint8_t               saved_rise  = ctx->aqi.rise_step;

    if (saved_codec == DAB_CODEC_MP2) {
        mp2_decoder_init(&ctx->codec.mp2);
        ctx->codec.mp2.sample_rate_hz = saved_sr;
    } else {
        aac_decoder_init(&ctx->codec.aac);
        ctx->codec.aac.sample_rate_hz = saved_sr;
    }

    soft_mute_init(&ctx->mute, saved_sr, saved_att, saved_rel, saved_curve);
    concealment_init(&ctx->conceal, &saved_cfg);
    aqi_init(&ctx->aqi, saved_rise);
    pop_prevention_reset(&ctx->pop);

    ctx->total_au_cnt     = 0U;
    ctx->crc_error_au_cnt = 0U;

    return DAB_OK;
}

int32_t DAB_Decoder_SetSbrActive(DAB_Decoder_Handle handle, uint8_t sbr_active) {
    DAB_Instance_Context *ctx = dab_get_context(handle);
    if (ctx == NULL) {
        return DAB_ERR_NULL_HANDLE;
    }
    if (ctx->codec_type == DAB_CODEC_AAC) {
        ctx->codec.aac.sbr_active = sbr_active;
    }
    return DAB_OK;
}
