# Automotive DAB / DAB+ Audio Decoder — Software Architecture Specification
**Document Version:** 2.0.0 (MS-1 Architecture Freeze)  
**Target Platform:** Odroid N2+ (Amlogic S922X, 4x Cortex-A73 @ 2.2GHz + 2x Cortex-A53 @ 1.8GHz)  
**Verification Host:** x86_64 Windows (MSYS2/UCRT64) / x86_64 Linux  
**Standards:** ETSI EN 300 401, ETSI TS 102 563, ISO/IEC 11172-3, ISO/IEC 14496-3  
**Option A Compliance:** 100% Original IP, Clean Room Design, Zero GPL/LGPL Components  

---

## 1. System Context & Component Architecture

```
                                  BASEBAND LAYER (RF / Channel Decoder)
                                                    │
                                                    ▼
                     Pure Audio Unit (AU) Bitstream + Optional Signal Metadata
                     [AU Payload (N bytes) | ETSI CRC-16 (2 bytes)]
                                                    │
                                                    ▼
┌───────────────────────────────────────────────────────────────────────────────────────────┐
│                    DAB / DAB+ AUTOMOTIVE AUDIO DECODER ENGINE (Opaque Handle)             │
│                                                                                           │
│   ┌───────────────────────────────────────────────────────────────────────────────────┐   │
│   │ 1. AU CRC-16/CCITT Engine (ETSI EN 300 401 §12.2)                                 │   │
│   │    - Polynomial: 0x1021, Initial: 0xFFFF, Zero reflection                         │   │
│   └─────────────────────────────────────────┬─────────────────────────────────────────┘   │
│                                             ▼                                             │
│                       [GOOD CRC] ─────────────── [CRC ERROR / LOST]                       │
│                           │                               │                               │
│                           ▼                               │                               │
│   ┌───────────────────────────────────────────────┐       │                               │
│   │ 2. Core Codec Decoders (Zero Dynamic Memory)  │       │                               │
│   │    ├── DAB: MPEG-1/2 Layer II (MUSICAM)       │       │                               │
│   │    │   - 32-band Polyphase Synthesis          │       │                               │
│   │    │   - Stereo, Joint Stereo, Dual, Mono     │       │                               │
│   │    │   - 48 kHz (1152 smp), 24 kHz (576 smp)  │       │                               │
│   │    └── DAB+: HE-AAC v2 (ETSI TS 102 563)      │       │                               │
│   │        - 960-sample window AAC-LC Core        │       │                               │
│   │        - SBR 64-band QMF HF Reconstruction    │       │                               │
│   │        - PS 34-band Hybrid QMF Spatialization │       │                               │
│   └───────────────────────┬───────────────────────┘       │                               │
│                           │ Raw PCM                       │ (No PCM)                      │
│                           ▼                               ▼                               │
│   ┌───────────────────────────────────────────────────────────────────────────────────┐   │
│   │ 3. Multi-Stage Concealment FSM (MS-3)                                             │   │
│   │    - Healthy:       Pass-through & update history buffer (prev_pcm)               │   │
│   │    - Short Loss:    Parametric linear interpolation fade                          │   │
│   │    - Long Loss:     Exponential attenuation curve (attn_gain *= step_q15)         │   │
│   │    - Extended Loss: Full mute trigger & silence output                            │   │
│   └─────────────────────────────────────────┬─────────────────────────────────────────┘   │
│                                             ▼                                             │
│   ┌───────────────────────────────────────────────────────────────────────────────────┐   │
│   │ 4. Pop-Noise Prevention Engine                                                    │   │
│   │    - Raised-cosine smooth crossfade on stream recovery after loss                 │   │
│   └─────────────────────────────────────────┬─────────────────────────────────────────┘   │
│                                             ▼                                             │
│   ┌───────────────────────────────────────────────────────────────────────────────────┐   │
│   │ 5. Soft Mute Engine (MS-3)                                                        │   │
│   │    - Attack [5..100 ms], Release [10..500 ms]                                     │   │
│   │    - Linear, Cosine LUT (128-entry Q15), Exponential squared decay               │   │
│   └─────────────────────────────────────────┬─────────────────────────────────────────┘   │
│                                             ▼                                             │
│   ┌───────────────────────────────────────────────────────────────────────────────────┐   │
│   │ 6. Asymmetric Audio Quality Index (AQI) & Blending Triggers (MS-4)                │   │
│   │    - Fast-Drop: Instant drop to 0 on CRC error or lost AU                         │   │
│   │    - Slow-Rise: Configurable rise rate (+2/frame up to 100) on healthy AUs        │   │
│   │    - 2-Bit Trigger Flags: 0b00 (IDLE), 0b01 (CONCEAL), 0b10 (UNRECOVERABLE)       │   │
│   └─────────────────────────────────────────┬─────────────────────────────────────────┘   │
└─────────────────────────────────────────────┼─────────────────────────────────────────────┘
                                              ▼
               Output: Interleaved 16-Bit Stereo PCM + Telemetry Status Report
```

---

## 2. Memory Architecture & MISRA-C Compliance

### 2.1 Static Memory Allocation
- **Rule 21.3 Compliance:** No dynamic memory allocation primitives (`malloc`, `free`, `realloc`, `calloc`) are present in any source file.
- Memory layout guarantees:
  - Caller allocates a memory block:
    - **MP2 Context:** 32,768 bytes (`DAB_DECODER_MP2_HANDLE_SIZE`)
    - **AAC Context:** 98,304 bytes (`DAB_DECODER_AAC_HANDLE_SIZE`)
  - The opaque handle `DAB_Decoder_Handle` points directly to the head of this buffer.

### 2.2 Re-entrancy & Thread-Safety
- **Zero Global Mutable Variables:** All state is encapsulated within `DAB_Instance_Context`.
- Multiple decoder instances can run concurrently across multiple threads without locking.

### 2.3 Vectorization Strategy
- Vectorized routines use explicit architecture guards:
  ```c
  #if defined(__ARM_NEON) && defined(__aarch64__)
      /* ARMv8-A NEON SIMD intrinsics */
  #else
      /* Bit-exact ISO C99 pure fallback */
  #endif
  ```
- Guard uses logical `&&` (AND) rather than `||` (OR) to prevent compilation errors on non-ARM 64-bit systems.
