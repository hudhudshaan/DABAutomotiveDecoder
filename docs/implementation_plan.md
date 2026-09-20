# Production-Grade ISO/IEC 14496-3 & ETSI TS 102 563 AAC-LC Engine Implementation Plan

## Problem Statement & Root Cause
The current DAB+ HE-AAC v2 decoder in `src/codec_aac/` was originally constructed with a synthetic test-vector parser and truncated 16-entry dummy Huffman codebooks. While the automotive container demuxing, CRC checking, concealment FSM, soft mute, AQI, and 960-point IMDCT have been successfully built, real broadcast bitstreams produced by broadcast encoders (such as `odr-audioenc` and Fraunhofer FDK) transmit full ISO/IEC 14496-3 syntax (`ics_info`, `section_data`, `scale_factor_data`, `spectral_data` with 81–289 entry Huffman trees, and DPCM differential scalefactors).

Without this normative syntax parser, real broadcast audio bitstreams desynchronize on bit 8, causing the IMDCT to output high-frequency shaped noise rather than audible music and speech.

---

## Architecture & Normative Specifications

```
ETSI TS 102 563 Superframe Stream (.au)
                   │
                   ▼
┌────────────────────────────────────────────────────────┐
│  AU Demuxer & CRC-16/CCITT Check (§12.2)               │
└──────────────────────────┬─────────────────────────────┘
                           │ Raw AU Payload
                           ▼
┌────────────────────────────────────────────────────────┐
│  ISO/IEC 14496-3 AAC-LC Bitstream Parser               │
│  ├── raw_data_block() [SCE / CPE / FIL / END]          │
│  ├── ics_info() [window_seq, max_sfb, grouping]        │
│  ├── section_data() [sfb section boundaries & CB 0..11]│
│  ├── scale_factor_data() [DPCM Huffman Table 4.6]      │
│  └── spectral_data() [CB 1..11 2D/4D Huffman Trees]    │
└──────────────────────────┬─────────────────────────────┘
                           │ Quantized indices + Scalefactors
                           ▼
┌────────────────────────────────────────────────────────┐
│  Dequantization & M/S Reconstruction Engine            │
│  spec[i] = sign(x) * |x|^(4/3) * 2^((sf - 100) / 4)    │
│  L = (M + S) / sqrt(2),  R = (M - S) / sqrt(2)         │
└──────────────────────────┬─────────────────────────────┘
                           │ 480 / 960 Spectral Lines
                           ▼
┌────────────────────────────────────────────────────────┐
│  960-Point IMDCT Engine (LUT-accelerated)              │
│  + Sine / KBD Overlap-Add Windowing                    │
└──────────────────────────┬─────────────────────────────┘
                           │ 960 Core Time Samples
                           ▼
┌────────────────────────────────────────────────────────┐
│  SBR Synthesis & PS Spatialization Engine              │
└──────────────────────────┬─────────────────────────────┘
                           │ 1920 Interleaved 48 kHz Samples
                           ▼
┌────────────────────────────────────────────────────────┐
│  Automotive Concealment, Pop Prevention & Soft Mute    │
└──────────────────────────┬─────────────────────────────┘
                           │ Validated 16-bit PCM
                           ▼
                 Loud & Clear Audio Out (.wav)
```

---

## User Review Required

> [!IMPORTANT]
> **Option A Cleanroom Verification**: All code and tables will be derived exclusively from publicly available ISO/IEC 14496-3 and ETSI TS 102 563 open specifications. No copyleft, GPL/LGPL, or third-party proprietary source code (e.g. libfdk-aac, FAAD2, FFmpeg) will be copied or linked into `libdab_decoder.a`.
>
> **Automotive Invariants Maintained**:
> 1. Zero `malloc()` / `free()` / `calloc()`. All buffers remain strictly caller-allocated within `DAB_DECODER_AAC_HANDLE_SIZE`.
> 2. Zero recursion; all loops bounded ($N \le 20$ for escape sequences, $N \le 480$ for spectral coefficients).
> 3. Float32 single-precision only (`sinf`, `cosf`, no double-precision float promotion).

---

## Proposed Changes

### Component 1: Normative Scale Factor Band (`sfb`) Tables & ISO Tables
#### [MODIFY] [`src/codec_aac/aac_tables.h`](../src/codec_aac/aac_tables.h)
#### [MODIFY] [`src/codec_aac/aac_tables.c`](../src/codec_aac/aac_tables.c)
- Add normative Scale Factor Band offset tables for 960-transform (`swb_offset_960`) for 48 kHz, 24 kHz, 32 kHz, 16 kHz.
- Add complete normative Huffman codebook lookup tables for Codebooks 1 through 11 (including full 2D pairs, 4D quads, signed/unsigned mappings, and max value bounds per ISO/IEC 14496-3 Table 4.1 through 4.11).

### Component 2: Huffman & Bitstream Parser Engine
#### [MODIFY] [`src/codec_aac/aac_huffman.h`](../src/codec_aac/aac_huffman.h)
#### [MODIFY] [`src/codec_aac/aac_huffman.c`](../src/codec_aac/aac_huffman.c)
- Implement full tree/LUT decoding for CB1–CB11.
- Support unsigned codebooks with trailing sign bit unpacking.
- Implement normative bounded escape sequence decoding for CB11 ($|v| = 16$).
- Implement DPCM scale factor decoder according to ISO 14496-3 Table 4.6.

### Component 3: ISO/IEC 14496-3 Syntax Unpacker & Dequantizer
#### [MODIFY] [`src/codec_aac/aac_decoder.h`](../src/codec_aac/aac_decoder.h)
#### [MODIFY] [`src/codec_aac/aac_decoder.c`](../src/codec_aac/aac_decoder.c)
- Implement `raw_data_block()` parsing supporting `ID_SCE` (0), `ID_CPE` (1), `ID_FIL` (6), `ID_END` (7).
- Implement `ics_info()`: `window_sequence`, `max_sfb`, `scale_factor_grouping`.
- Implement `section_data()`: parse variable-length section lengths and assign codebooks per sfb.
- Implement `scale_factor_data()`: decode differential scalefactors and calculate absolute scalefactor per sfb from `global_gain`.
- Implement `spectral_data()`: decode spectral lines per sfb using section codebooks.
- Implement normative dequantization with scale factor scaling:
  $$\text{spec}[i] = \text{sign}(x) \cdot |x|^{4/3} \cdot 2^{\frac{\text{scalefactor}[sfb] - 100}{4}}$$
- Implement M/S stereo matrixing ($L = (M+S)/\sqrt{2}, R = (M-S)/\sqrt{2}$) when `ms_mask_present` is flagged.

### Component 4: Test Suite & Runner Verification
#### [MODIFY] [`tests/test_ms2_aac.c`](../tests/test_ms2_aac.c)
- Expand test assertions to cover section decoding, scale factor application, and dynamic range verification.
#### [VERIFY] Re-decode `One_Voice_Children_s_Choir_-_Believer_Thunder__CeeNaija.com_.au` and verify crystal-clear singing voice and music playback in VLC.

---

## Verification Plan

### Automated Tests
1. **Unit Test Suite**: Run `ctest --output-on-failure` to verify all 8 test suites pass (MS-1, MS-2 MP2, MS-2 AAC, MS-3 Concealment, MS-4 Quality, Multi-Instance, Robustness, 24h Stress).
2. **Huffman Bitstream Decoder Verification**: Verify that synthetic bitstreams and real broadcast bitstreams decode without bitstream reader desynchronization or error codes.

### Audio Quality & Playback Verification
1. Run `automotive_decoder_runner.exe` on `One_Voice_Children_s_Choir_-_Believer_Thunder__CeeNaija.com_.au`.
2. Inspect the resulting WAV file:
   - Verify non-zero harmonic spectral peaks corresponding to singing vocals ($200\text{ Hz} \dots 4000\text{ Hz}$).
   - Verify dynamic range and zero DC rail pinning.
3. Audio Playback Test: Confirm clear acoustic playback in VLC media player of the vocal melody and lyrics of *Believer / Thunder*.
