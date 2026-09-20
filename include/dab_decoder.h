/**
 * @file    dab_decoder.h
 * @brief   Master Public C API — Automotive DAB / DAB+ Audio Decoder Engine
 *          MS-1 Architecture Freeze (Unified MS-1 through MS-4)
 *
 * @details Complete production-ready decoder API for automotive infotainment systems.
 *          Supports DAB (MPEG-1 Layer II / MUSICAM) and DAB+ (HE-AAC v2: LC + SBR + PS).
 *          Zero dynamic memory — caller provides static handle buffers.
 *          Thread-safe multi-instance — all state in caller-allocated context.
 *          MISRA-C:2012 compliant — ISO C99 — aarch64 NEON + Pure C builds.
 *
 * @standard ETSI EN 300 401, ETSI TS 102 563, ISO/IEC 11172-3, ISO/IEC 14496-3
 * @platform x86 Windows/Linux (test), Odroid N2+ aarch64 (production)
 * @version  2.0.0 (MS-1 to MS-4 unified)
 *
 * @par MISRA-C:2012 Deviations:
 *   - Rule 11.5: void* opaque handle — intentional, documented architectural pattern
 *   - Rule 20.7: stdbool.h bool — permitted in ISO C99 with derogation
 *
 * @copyright 2026 — All rights reserved. 100% proprietary IP. Zero GPL/LGPL dependencies.
 */

#ifndef DAB_DECODER_H
#define DAB_DECODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* =========================================================================
 * HANDLE SIZES — Static Memory Allocation Constants
 * Caller must provide a buffer of at least this size, aligned to 8 bytes.
 * ========================================================================= */

/** @brief Handle buffer size required for DAB MP2/MUSICAM decoder instance (32 KB) */
#define DAB_DECODER_MP2_HANDLE_SIZE     (32768U)

/** @brief Handle buffer size required for DAB+ HE-AAC v2 decoder instance (96 KB) */
#define DAB_DECODER_AAC_HANDLE_SIZE     (98304U)

/* =========================================================================
 * FRAME GEOMETRY CONSTANTS
 * ========================================================================= */

/** @brief Max stereo PCM samples output per AU frame (AAC 960 × 2x SBR) */
#define DAB_MAX_PCM_SAMPLES_PER_AU      (1920U)

/** @brief MP2: samples per AU at 48 kHz */
#define DAB_MP2_SAMPLES_PER_AU_48K      (1152U)
/** @brief MP2: samples per AU at 24 kHz (half-rate) */
#define DAB_MP2_SAMPLES_PER_AU_24K      (576U)

/** @brief AAC-LC: core samples per AU at ≥32 kHz (960-window, ETSI TS 102 563 mandatory) */
#define DAB_AAC_CORE_SAMPLES_960        (960U)
/** @brief AAC-LC: core samples per AU at 16/24 kHz (480-window) */
#define DAB_AAC_CORE_SAMPLES_480        (480U)
/** @brief AAC + SBR: output samples per AU at 48 kHz (960 × 2 upsample) */
#define DAB_AAC_SBR_SAMPLES_48K         (1920U)
/** @brief AAC + SBR: output samples per AU at 32 kHz (960 × 2 upsample) */
#define DAB_AAC_SBR_SAMPLES_32K         (1920U)

/** @brief AU header CRC size per ETSI EN 300 401 / ETSI TS 102 563 */
#define DAB_AU_CRC_BYTES                (2U)
/** @brief Minimum AU payload size (header + 1 payload byte + CRC) */
#define DAB_AU_MIN_LEN_BYTES            (3U)

/* =========================================================================
 * SOFT MUTE PARAMETER BOUNDS (MS-3)
 * ========================================================================= */
#define DAB_ATTACK_MS_MIN               (5U)
#define DAB_ATTACK_MS_MAX               (100U)
#define DAB_ATTACK_MS_DEFAULT           (15U)
#define DAB_RELEASE_MS_MIN              (10U)
#define DAB_RELEASE_MS_MAX              (500U)
#define DAB_RELEASE_MS_DEFAULT          (75U)

/* =========================================================================
 * QUALITY INDEX BOUNDS (MS-4)
 * ========================================================================= */
#define DAB_QUALITY_MAX                 (100U)
#define DAB_QUALITY_MIN                 (0U)
#define DAB_QUALITY_RISE_STEP_DEFAULT   (2U)
#define DAB_QUALITY_RISE_STEP_MIN       (1U)
#define DAB_QUALITY_RISE_STEP_MAX       (10U)

/* =========================================================================
 * RETURN CODES
 * ========================================================================= */
#define DAB_OK                          (0)
#define DAB_ERR_NULL_HANDLE             (-1)
#define DAB_ERR_INVALID_PARAM           (-2)
#define DAB_ERR_BUFFER_TOO_SMALL        (-3)
#define DAB_ERR_BAD_MAGIC               (-4)
#define DAB_ERR_CODEC_FAIL              (-5)
#define DAB_ERR_CRC                     (-6)

/* =========================================================================
 * ENUMERATIONS
 * ========================================================================= */

/**
 * @brief Codec type selection.
 */
typedef enum {
    DAB_CODEC_MP2 = 0,  /**< DAB  — MPEG-1 Audio Layer II / MUSICAM
                          *   Standard: ISO/IEC 11172-3 & ETSI EN 300 401 */
    DAB_CODEC_AAC = 1   /**< DAB+ — HE-AAC v2 (AAC-LC + SBR + PS)
                          *   Standard: ETSI TS 102 563 & ISO/IEC 14496-3 */
} DAB_CodecType;

/**
 * @brief Active concealment mode (MS-3).
 */
typedef enum {
    DAB_CONCEAL_NONE        = 0, /**< Normal — no concealment applied */
    DAB_CONCEAL_INTERPOLATE = 1, /**< Short loss (1-2 AU): parametric interpolation */
    DAB_CONCEAL_ATTENUATE   = 2, /**< Long loss (3-6 AU): gain attenuation curve */
    DAB_CONCEAL_MUTED       = 3  /**< Extended loss (>6 AU): soft mute applied */
} DAB_ConcealmentMode;

/**
 * @brief Soft mute gain ramp curve profiles (MS-3).
 */
typedef enum {
    DAB_RAMP_LINEAR      = 0, /**< Linear gain ramp */
    DAB_RAMP_COSINE      = 1, /**< Raised-cosine smooth ramp (default) */
    DAB_RAMP_EXPONENTIAL = 2  /**< Exponential gain ramp */
} DAB_RampCurve;

/**
 * @brief Soft mute engine state (MS-3).
 */
typedef enum {
    DAB_MUTE_IDLE       = 0, /**< Normal playback — full gain */
    DAB_MUTE_ATTACKING  = 1, /**< Ramping down toward mute */
    DAB_MUTE_SUSTAIN    = 2, /**< Holding at full mute */
    DAB_MUTE_RELEASING  = 3  /**< Ramping up toward full gain */
} DAB_MuteState;

/**
 * @brief AU (Audio Unit) evaluation status per ETSI EN 300 401 §12.2 (MS-2).
 */
typedef enum {
    DAB_AU_GOOD    = 0, /**< AU CRC pass — decoded normally */
    DAB_AU_CRC_ERR = 1, /**< AU CRC fail — concealment applied */
    DAB_AU_LOST    = 2  /**< AU lost / NULL input — concealment applied */
} DAB_AUStatus;

/**
 * @brief 2-bit Real-time Blending Trigger Flags for seamless FM/DAB switching (MS-4).
 *
 * @details Bit encoding:
 *   0b00 = BLEND_IDLE           — normal operation, no action
 *   0b01 = CONCEAL_TRIGGER      — pre-trigger: short-term concealment detected, prepare switch
 *   0b10 = UNRECOVERABLE_TRIGGER — hard-trigger: muted, initiate immediate switch
 */
typedef enum {
    BLEND_IDLE               = 0x00U, /**< 0b00 — no action */
    CONCEAL_TRIGGER          = 0x01U, /**< 0b01 — pre-trigger (interpolate/attenuate) */
    UNRECOVERABLE_TRIGGER    = 0x02U  /**< 0b10 — hard-trigger (muted) */
} DAB_BlendingTriggerState;

/* =========================================================================
 * STRUCTURES
 * ========================================================================= */

/**
 * @brief Opaque decoder handle — points into a caller-owned static buffer.
 * @note  MISRA-C:2012 Rule 11.5 deviation — intentional opaque handle pattern.
 */
typedef void *DAB_Decoder_Handle;

/**
 * @brief Baseband signal status metadata — input per AU frame (MS-2).
 */
typedef struct {
    uint8_t  au_crc_pass;  /**< 1 = CRC pass, 0 = CRC fail (set by BB layer) */
    uint8_t  bb_rs_pass;   /**< 1 = RS success, 0 = RS failure (informational) */
    uint8_t  ber_level;    /**< Bit Error Rate estimate (0=clean, 255=max BER) */
    int8_t   snr_db;       /**< Estimated SNR in dB (-128..+127) */
} DAB_SignalStatus;

/**
 * @brief Concealment configuration parameters (MS-3).
 */
typedef struct {
    uint8_t short_loss_thresh; /**< Consecutive lost AUs before INTERPOLATE (default 2) */
    uint8_t long_loss_thresh;  /**< Consecutive lost AUs before ATTENUATE   (default 6) */
    uint8_t reserved[2];       /**< Padding — must be zero */
    int32_t attn_step_q15;     /**< Q15 attenuation multiplier per AU (default 29491 ≈ 0.9) */
} DAB_ConcealmentConfig;

/**
 * @brief Comprehensive per-frame audio status telemetry report (MS-2 to MS-4).
 */
typedef struct {
    /* ---- MS-2: AU Evaluation ---- */
    DAB_AUStatus            au_status;         /**< GOOD / CRC_ERR / LOST */
    uint32_t                total_au_cnt;       /**< Cumulative total AUs processed */
    uint32_t                crc_error_au_cnt;   /**< Cumulative CRC error AUs */

    /* ---- MS-3: Concealment & Mute ---- */
    DAB_ConcealmentMode     conceal_mode;       /**< Active concealment mode */
    DAB_MuteState           mute_state;         /**< Current soft mute FSM state */
    int32_t                 cur_gain_q15;       /**< Current gain in Q15 (32767 = full) */
    uint8_t                 consec_loss;        /**< Consecutive lost AU count */

    /* ---- MS-4: Quality Index & Blending Trigger ---- */
    uint8_t                 audio_quality;      /**< Asymmetric Quality Index 0-100 */
    DAB_BlendingTriggerState trigger;           /**< 2-bit blending trigger flag */

    /* ---- Decoded PCM Info ---- */
    uint16_t                pcm_samples_out;    /**< Samples output this frame (per channel) */
    uint32_t                sample_rate_hz;     /**< Output sample rate in Hz */
    uint8_t                 num_channels;       /**< Output channels (1=Mono, 2=Stereo) */
    uint8_t                 reserved[3];        /**< Padding — always zero */
} DAB_AudioStatus;

/* =========================================================================
 * CORE API (MS-1 Architecture-Frozen Prototypes)
 * ========================================================================= */

/**
 * @brief Query required handle buffer size for a given codec.
 *
 * @param[in] codec_type  DAB_CODEC_MP2 or DAB_CODEC_AAC
 * @return                Required buffer size in bytes (use as compile-time constant
 *                        or stack-allocate the named macro directly).
 */
size_t DAB_Decoder_GetHandleSize(DAB_CodecType codec_type);

/**
 * @brief Initialise a decoder instance in a caller-provided static buffer.
 *
 * @details No malloc/calloc/free. All state is stored in [buf, buf+buf_size).
 *          The buffer must remain valid for the lifetime of the instance.
 *          Minimum alignment: 8 bytes (use ALIGN(8) or static array with explicit size).
 *
 * @param[in] buf           Pointer to caller-allocated buffer.
 * @param[in] buf_size      Size of buffer in bytes. Must be ≥ DAB_Decoder_GetHandleSize().
 * @param[in] codec_type    DAB_CODEC_MP2 or DAB_CODEC_AAC.
 * @param[in] sample_rate_hz Output sample rate: 16000, 24000, 32000, or 48000.
 * @return  Valid opaque handle on success, NULL on error.
 */
DAB_Decoder_Handle DAB_Decoder_InitWithMem(
    void           *buf,
    size_t          buf_size,
    DAB_CodecType   codec_type,
    uint32_t        sample_rate_hz
);

/**
 * @brief Configure Spectral Band Replication (SBR) upsampling active status.
 *
 * @param[in] handle     Decoder instance handle.
 * @param[in] sbr_active 1 = SBR active (2x upsampling to 32k or 48k output), 0 = standalone AAC-LC core.
 * @return DAB_OK on success, negative error code on failure.
 */
int32_t DAB_Decoder_SetSbrActive(DAB_Decoder_Handle handle, uint8_t sbr_active);

/**
 * @brief AU-based frame decode — the main per-frame processing entry point.
 *
 * @details Parses AU bitstream, evaluates internal CRC per ETSI EN 300 401 §12.2,
 *          decodes PCM, applies MS-3 concealment + mute, updates MS-4 quality/trigger.
 *          Input: pure AU bitstream (RS pre-decoded at BB layer, CRC bytes included).
 *          Output: 16-bit PCM, stereo interleaved (L0, R0, L1, R1, ...).
 *
 * @param[in]  handle         Decoder instance handle.
 * @param[in]  au_data        Pointer to AU bitstream (payload + 2-byte CRC). NULL = lost AU.
 * @param[in]  au_len         Total AU length in bytes (payload + 2). 0 = lost AU.
 * @param[in]  signal_status  Optional BB signal metadata. NULL = assume CRC pass.
 * @param[out] pcm_out        Output PCM buffer (caller-provided). Stereo interleaved int16_t.
 *                            Must be ≥ DAB_MAX_PCM_SAMPLES_PER_AU × 2 × sizeof(int16_t).
 * @param[out] status_out     Per-frame audio status report. NULL = ignored.
 * @return  Number of PCM samples written per channel (≥0), or DAB_ERR_* (<0) on error.
 */
int32_t DAB_Decoder_DecodeAU(
    DAB_Decoder_Handle          handle,
    const uint8_t              *au_data,
    uint16_t                    au_len,
    const DAB_SignalStatus     *signal_status,
    int16_t                    *pcm_out,
    DAB_AudioStatus            *status_out
);

/**
 * @brief Set concealment engine parameters at runtime (MS-3).
 *
 * @param[in] handle  Decoder instance handle.
 * @param[in] config  Pointer to concealment configuration. Must not be NULL.
 * @return  DAB_OK on success, DAB_ERR_* on error.
 */
int32_t DAB_Decoder_SetConcealmentParam(
    DAB_Decoder_Handle          handle,
    const DAB_ConcealmentConfig *config
);

/**
 * @brief Set soft mute attack and release times in milliseconds (MS-3).
 *
 * @param[in] handle      Decoder instance handle.
 * @param[in] attack_ms   Attack time [5..100] ms. Default: 15 ms.
 * @param[in] release_ms  Release time [10..500] ms. Default: 75 ms.
 * @return  DAB_OK on success, DAB_ERR_INVALID_PARAM if out of range.
 */
int32_t DAB_Decoder_SetMuteTime(
    DAB_Decoder_Handle  handle,
    uint32_t            attack_ms,
    uint32_t            release_ms
);

/**
 * @brief Set soft mute gain ramp curve profile (MS-3).
 *
 * @param[in] handle  Decoder instance handle.
 * @param[in] curve   DAB_RAMP_LINEAR / DAB_RAMP_COSINE / DAB_RAMP_EXPONENTIAL.
 * @return  DAB_OK on success, DAB_ERR_* on error.
 */
int32_t DAB_Decoder_SetRampCurve(DAB_Decoder_Handle handle, DAB_RampCurve curve);

/**
 * @brief Set asymmetric quality index rise step (MS-4).
 *
 * @param[in] handle     Decoder instance handle.
 * @param[in] rise_step  Quality points added per good AU [1..10]. Default: 2.
 * @return  DAB_OK on success, DAB_ERR_INVALID_PARAM if out of range.
 */
int32_t DAB_Decoder_SetQualityRiseStep(DAB_Decoder_Handle handle, uint8_t rise_step);

/**
 * @brief Get current quality index and blending trigger state (MS-4).
 *
 * @param[in]  handle        Decoder instance handle.
 * @param[out] quality_out   Current audio quality index [0..100].
 * @param[out] trigger_out   Current 2-bit blending trigger flag.
 * @return  DAB_OK on success, DAB_ERR_* on error.
 */
int32_t DAB_Decoder_GetQualityStatus(
    DAB_Decoder_Handle       handle,
    uint8_t                 *quality_out,
    DAB_BlendingTriggerState *trigger_out
);

/**
 * @brief Get AU CRC statistics counters.
 *
 * @param[in]  handle       Decoder instance handle.
 * @param[out] total_out    Total AUs processed since init or last reset.
 * @param[out] err_out      Total CRC error AUs since init or last reset.
 * @return  DAB_OK on success, DAB_ERR_* on error.
 */
int32_t DAB_Decoder_GetAUStats(
    DAB_Decoder_Handle  handle,
    uint32_t           *total_out,
    uint32_t           *err_out
);

/**
 * @brief Reset AU CRC statistics counters and quality index to zero.
 *
 * @param[in] handle  Decoder instance handle.
 * @return  DAB_OK on success, DAB_ERR_* on error.
 */
int32_t DAB_Decoder_ResetStats(DAB_Decoder_Handle handle);

/**
 * @brief Full instance reset — returns decoder to post-init state.
 *        Codec state, concealment FSM, mute engine, and quality index all reset.
 *        Handle buffer and configuration parameters are preserved.
 *
 * @param[in] handle  Decoder instance handle.
 * @return  DAB_OK on success, DAB_ERR_* on error.
 */
int32_t DAB_Decoder_Reset(DAB_Decoder_Handle handle);

#ifdef __cplusplus
}
#endif

#endif /* DAB_DECODER_H */
