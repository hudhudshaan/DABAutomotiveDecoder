/**
 * @file    dab_internal.h
 * @brief   Unified DAB/DAB+ Internal Decoder Context Definition
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef DAB_INTERNAL_H
#define DAB_INTERNAL_H

#include "dab_decoder.h"
#include "../codec_mp2/mp2_decoder.h"
#include "../codec_aac/aac_decoder.h"
#include "../automotive/dab_crc.h"
#include "../automotive/soft_mute.h"
#include "../automotive/concealment.h"
#include "../automotive/aqi_engine.h"
#include "../automotive/pop_prevention.h"
#include "../dsp/dsp_math.h"

#define DAB_DECODER_MAGIC   (0xDAB0C0DEU)

typedef union {
    mp2_decoder_state_t mp2;
    aac_decoder_state_t aac;
} dab_codec_state_t;

typedef struct {
    uint32_t             magic;
    DAB_CodecType        codec_type;
    uint32_t             sample_rate_hz;
    uint8_t              num_channels;
    uint8_t              enable_neon;
    uint8_t              reserved[2];

    /* Automotive Layer Sub-Engines */
    soft_mute_engine_t   mute;
    concealment_engine_t conceal;
    aqi_engine_t         aqi;
    pop_prevention_t     pop;

    /* Statistics */
    uint32_t             total_au_cnt;
    uint32_t             crc_error_au_cnt;

    /* Intermediate raw PCM buffer */
    int16_t              raw_pcm[DAB_MAX_PCM_SAMPLES_PER_AU * 2U];

    /* Codec Private State (placed at end for optimal memory packing) */
    dab_codec_state_t    codec;
} DAB_Instance_Context;

static inline DAB_Instance_Context *dab_get_context(DAB_Decoder_Handle handle) {
    if (handle == NULL) {
        return NULL;
    }
    DAB_Instance_Context *ctx = (DAB_Instance_Context *)handle;
    if (ctx->magic != DAB_DECODER_MAGIC) {
        return NULL;
    }
    return ctx;
}

#endif /* DAB_INTERNAL_H */
