# Automotive DAB / DAB+ Audio Decoder — API Reference Manual
**Milestone MS-1 to MS-4 Unified Deliverable**  
**Standard Compliance:** ETSI EN 300 401, ETSI TS 102 563, ISO/IEC 11172-3, ISO/IEC 14496-3  
**Automotive Quality:** MISRA-C:2012, ISO C99, Zero Dynamic Memory, Zero Mutable Global State  
**Option A Confirmation:** 100% Clean Proprietary IP, Zero GPL/LGPL Software Royalty  

---

## 1. Overview & Architecture Design

The Automotive DAB / DAB+ Audio Decoder is an ISO C99 software engine engineered specifically for automotive infotainment systems (e.g. Odroid N2+, ARM Cortex-A73/A55, and x86 testing hosts).

### Core Architectural Guarantees:
1. **Zero Dynamic Allocation (MISRA-C:2012 Rule 21.3):**  
   The decoder never calls `malloc()`, `calloc()`, `free()`, or `realloc()`. The host application supplies statically allocated memory blocks during initialization.
2. **Re-entrant Multi-Instance Thread-Safety:**  
   Zero static global mutable variables. Multiple tuner instances (e.g. Foreground DAB Audio + Background DAB Data/Monitoring) run concurrently without mutex contention.
3. **Standards-Compliant Pure AU Processing:**  
   Input bitstreams consist of pure Audio Units (Reed-Solomon pre-decoded at baseband), ending with a 2-byte CRC-16/CCITT per ETSI EN 300 401 §12.2.
4. **Automotive Audio Protection:**  
   Integrated multi-stage concealment FSM, raised-cosine/linear soft mute gain ramping, pop-noise prevention crossfade, and 2-bit real-time blending triggers for seamless FM/DAB transition.

---

## 2. API Constants & Enumerations

### Handle Sizes (Static Allocation)
```c
#define DAB_DECODER_MP2_HANDLE_SIZE     (32768U)  /* 32 KB */
#define DAB_DECODER_AAC_HANDLE_SIZE     (98304U)  /* 96 KB */
```

### Codec Types
```c
typedef enum {
    DAB_CODEC_MP2 = 0,  /**< DAB  — MPEG-1 Audio Layer II (MUSICAM) */
    DAB_CODEC_AAC = 1   /**< DAB+ — HE-AAC v2 (AAC-LC + SBR + PS)    */
} DAB_CodecType;
```

### Concealment Modes (MS-3)
```c
typedef enum {
    DAB_CONCEAL_NONE        = 0, /**< Normal healthy decoding */
    DAB_CONCEAL_INTERPOLATE = 1, /**< Short loss (1-2 AU): Parametric linear interpolation */
    DAB_CONCEAL_ATTENUATE   = 2, /**< Long loss (3-6 AU): Exponential gain attenuation */
    DAB_CONCEAL_MUTED       = 3  /**< Extended loss (>6 AU): Full soft mute sustain */
} DAB_ConcealmentMode;
```

### 2-Bit Real-Time Blending Trigger Flags (MS-4)
```c
typedef enum {
    BLEND_IDLE            = 0x00U, /**< 0b00: Normal healthy DAB reception */
    CONCEAL_TRIGGER       = 0x01U, /**< 0b01: Pre-trigger (interpolation/attenuation detected) */
    UNRECOVERABLE_TRIGGER = 0x02U  /**< 0b10: Hard-trigger (muted, initiate immediate FM blend) */
} DAB_BlendingTriggerState;
```

---

## 3. Function Reference

### `DAB_Decoder_GetHandleSize`
```c
size_t DAB_Decoder_GetHandleSize(DAB_CodecType codec_type);
```
- **Description:** Returns the minimum static buffer size (in bytes) required for a given codec instance.
- **Parameters:** `codec_type` — `DAB_CODEC_MP2` or `DAB_CODEC_AAC`.
- **Return Value:** Size in bytes.

---

### `DAB_Decoder_InitWithMem`
```c
DAB_Decoder_Handle DAB_Decoder_InitWithMem(
    void           *buf,
    size_t          buf_size,
    DAB_CodecType   codec_type,
    uint32_t        sample_rate_hz
);
```
- **Description:** Initializes a decoder instance inside caller-provided memory.
- **Parameters:**
  - `buf`: Pointer to caller-allocated static buffer (must be 8-byte aligned).
  - `buf_size`: Size of `buf` in bytes. Must be $\ge$ `DAB_Decoder_GetHandleSize(codec_type)`.
  - `codec_type`: `DAB_CODEC_MP2` or `DAB_CODEC_AAC`.
  - `sample_rate_hz`: Output sample rate: 16000, 24000, 32000, or 48000 Hz.
- **Return Value:** Opaque handle on success; `NULL` if parameters are invalid.

---

### `DAB_Decoder_DecodeAU`
```c
int32_t DAB_Decoder_DecodeAU(
    DAB_Decoder_Handle          handle,
    const uint8_t              *au_data,
    uint16_t                    au_len,
    const DAB_SignalStatus     *signal_status,
    int16_t                    *pcm_out,
    DAB_AudioStatus            *status_out
);
```
- **Description:** Main per-frame processing entry point.
  1. Computes CRC-16/CCITT over AU bitstream payload.
  2. Decodes MPEG Layer II or HE-AAC v2 core bitstream.
  3. Applies multi-stage concealment FSM if corrupted/lost.
  4. Applies pop-prevention crossfade on stream recovery.
  5. Ramps soft mute gain.
  6. Updates Asymmetric Audio Quality Index (0..100) and 2-bit blending trigger flags.
- **Parameters:**
  - `handle`: Valid decoder handle.
  - `au_data`: Pointer to AU frame (payload + 2-byte CRC). `NULL` signals lost AU.
  - `au_len`: Total byte length of `au_data`. `0` signals lost AU.
  - `signal_status`: Optional baseband RF/RS telemetry (`NULL` if unavailable).
  - `pcm_out`: Caller buffer receiving interleaved 16-bit stereo PCM ($L_0, R_0, L_1, R_1, \dots$).
  - `status_out`: Optional pointer receiving comprehensive frame telemetry report.
- **Return Value:** Number of decoded PCM samples per channel ($\ge 0$), or negative error code.

---

### `DAB_Decoder_SetConcealmentParam`
```c
int32_t DAB_Decoder_SetConcealmentParam(
    DAB_Decoder_Handle          handle,
    const DAB_ConcealmentConfig *config
);
```
- **Description:** Adjusts consecutive lost frame thresholds and attenuation step size.

---

### `DAB_Decoder_SetMuteTime`
```c
int32_t DAB_Decoder_SetMuteTime(
    DAB_Decoder_Handle  handle,
    uint32_t            attack_ms,
    uint32_t            release_ms
);
```
- **Description:** Configures attack time [5..100 ms] and release time [10..500 ms].

---

### `DAB_Decoder_SetRampCurve`
```c
int32_t DAB_Decoder_SetRampCurve(DAB_Decoder_Handle handle, DAB_RampCurve curve);
```
- **Description:** Configures gain ramp shape: `DAB_RAMP_LINEAR`, `DAB_RAMP_COSINE`, or `DAB_RAMP_EXPONENTIAL`.

---

### `DAB_Decoder_SetQualityRiseStep`
```c
int32_t DAB_Decoder_SetQualityRiseStep(DAB_Decoder_Handle handle, uint8_t rise_step);
```
- **Description:** Adjusts Asymmetric AQI recovery rate [1..10 points per good AU].

---

### `DAB_Decoder_GetQualityStatus`
```c
int32_t DAB_Decoder_GetQualityStatus(
    DAB_Decoder_Handle       handle,
    uint8_t                 *quality_out,
    DAB_BlendingTriggerState *trigger_out
);
```
- **Description:** Queries instantaneous Quality Index (0..100) and 2-bit blending trigger flags.

---

### `DAB_Decoder_GetAUStats` & `DAB_Decoder_ResetStats`
```c
int32_t DAB_Decoder_GetAUStats(DAB_Decoder_Handle handle, uint32_t *total_out, uint32_t *err_out);
int32_t DAB_Decoder_ResetStats(DAB_Decoder_Handle handle);
```
- **Description:** Queries or resets cumulative AU frame counters and CRC error counters.

---

### `DAB_Decoder_Reset`
```c
int32_t DAB_Decoder_Reset(DAB_Decoder_Handle handle);
```
- **Description:** Flushes filter delay lines and resets concealment FSM while preserving user configuration.
