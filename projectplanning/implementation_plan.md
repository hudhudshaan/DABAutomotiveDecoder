# MS-1 to MS-4 Complete DAB/DAB+ Automotive Decoder Package — Implementation Plan

---

## Part 1: Existing Decoder Code Assessment — Full Audit Results

### Score: 6.5 / 10 — Architecture ✅, Codec Cores ❌

#### ✅ Re-Usable As-Is (with minor MISRA fixes)

| Component | File | Quality |
|---|---|---|
| Automotive concealment FSM | `concealment.c/h` | ✅ Production quality |
| Asymmetric AQI (fast-drop/slow-rise) | `aqi_engine.c/h` | ✅ Excellent |
| Soft Mute (3 curve profiles) | `soft_mute.c/h` | ✅ Good |
| Pop-noise prevention (cosine crossfade) | `pop_prevention.c/h` | ✅ Good |
| Bitstream reader | `bitstream_reader.c/h` | ✅ Clean, safe |
| MP2 tables (scale factors, C/D coef, alloc tables) | `mp2_tables.c/h` | ✅ Standards-accurate |
| Multi-instance context struct architecture | `dab_instance.c` | ✅ Correct pattern |
| Public API shape | `dab_decoder.h` | ✅ Correct |
| NEON DSP helpers | `dsp_neon.c` | ✅ Correct guard/fallback |

#### ❌ Critical Blockers (MUST FIX — cannot ship automotive code with these)

| # | Issue | File & Line | Severity |
|---|---|---|---|
| 1 | **`calloc()`/`free()` used** — fatal MISRA R21.3 violation | `dab_instance.c` L53, L254 | 🔴 CRITICAL |
| 2 | **`float sbr_tmp[2048]`** on stack = 8 KB per call — stack overflow risk | `aac_decoder.c` L158 | 🔴 CRITICAL |
| 3 | **`float time_buf[1024]`** on stack = 4 KB per call | `aac_decoder.c` L136 | 🔴 CRITICAL |
| 4 | **Missing `default:`** in `switch(scfsi)` | `mp2_decoder.c` L93 | 🔴 CRITICAL (MISRA R16.4) |
| 5 | **AAC SBR is a stub** — 64-band QMF not implemented, 2-sample passthrough only | `dsp_math.c` L70-76 | 🔴 CRITICAL |
| 6 | **AAC PS is a stub** — `iid_gain=1.05f` literal, `sin()` not correct 34-band algorithm | `aac_decoder.c` L29-39 | 🔴 CRITICAL |
| 7 | **MP2 Joint Stereo / Dual Channel parsed but silently discarded** | `mp2_decoder.c` L53-54 | 🟠 HIGH |
| 8 | **NEON guard uses `||` not `&&`** (`__ARM_NEON || __aarch64__`) | `dsp_math.h` L25 | 🟠 HIGH |
| 9 | **Unbounded `while` loop** in Huffman escape decoder — malformed bitstream risk | `aac_huffman.c` L99 | 🟠 HIGH |
| 10 | **Header sync bit-count mismatch** — skips 2 bits for 3-bit ID+Layer+Protection field | `mp2_decoder.c` L46 | 🟠 HIGH |
| 11 | **`log10()` / `pow()` / `sin()` / `cos()` called** (double precision `math.h`) | Multiple files | 🟡 MEDIUM |
| 12 | **Parameter mutation** `num_channels = 2` modifies function arg | `aac_decoder.c` L177 | 🟡 MEDIUM (MISRA R17.8) |

#### Key Functional Gaps (incomplete implementations):

- **MP2 Joint Stereo and Dual Channel** — mode bits parsed, then `(void)mode` discarded. Required for real DAB broadcasts.
- **AAC short-window (8×120) IMDCT** — 960-window *selected* correctly but short-block switching not implemented.
- **SBR QMF synthesis** — required for HE-AAC v1/v2 (all 16/24 kHz DAB+ streams use SBR). Currently a 2-sample passthrough.
- **Parametric Stereo** — required for HE-AAC v2. Currently a simple `1.05×`/`0.95×` gain split.
- **AU superframe header parsing** — `aac_frame.c` reads AU size as a raw 12-bit field. Real ETSI TS 102 563 requires AU start-offset table parsing from the superframe header.
- **Only 2 of 11 AAC Huffman codebooks** have explicit tables (CB1 and CB11). Codebooks 2–10 use a generic fallback which may not decode all real streams.

#### IP Verdict: 100% CLEAN ✅
No GPL/LGPL code detected anywhere. No FFmpeg, FAAD2, libfdk-aac, or libmad present.  
All codec tables (ISO 11172-3, ISO 14496-3) are normative specification content, not copyrightable software.

---

## Part 2: Option A Legal / IP Confirmation

**YES — the code qualifies 100% for Option A with zero IP risk**, on the following basis:

| Question | Answer |
|---|---|
| Is any GPL/LGPL code included? | ❌ No — no FFmpeg, no FAAD2, no libfdk-aac, no mad/libmad |
| Are ETSI/ISO standards free to implement? | ✅ Yes — ETSI EN 300 401 and ETSI TS 102 563 are freely published. Implementing a standard is not a copyright violation. |
| Are the codec tables (Huffman, etc.) copyrighted? | ✅ No — standardized Huffman codebooks are normative parts of the standard, not creative expression |
| Is there any per-unit software royalty? | ❌ None — this is your own code |
| Via LA AAC patent royalty | ⚠️ Note: Via Licensing AAC pool (~\$0.20–\$0.75/unit) applies to **any** HE-AAC v2 **product** regardless of implementation. This is a **standard/patent royalty**, not a software royalty, and applies equally to Options A, B, and C. This is shown in your own comparison table. |

**Conclusion**: Option A = lowest TCO, 100% IP ownership, zero software copyright risk. ✅

---

## Part 3: Milestone Scope Mapping (MS-1 → MS-4)

| Milestone | Scope | Code Status |
|---|---|---|
| **MS-1** | Architecture freeze: opaque handle API, `DAB_AudioStatus`, AU input pipeline spec | ✅ Already delivered (header design, SW arch doc) |
| **MS-2** | DAB (MP2/MUSICAM) + DAB+ (HE-AAC v2 960-window) core decoders; static memory; AU CRC engine | 🟠 PARTIAL — Infrastructure ready, codec cores incomplete |
| **MS-3** | Concealment engine, mute control, multi-instance NEON | ✅ Already delivered (`dab_ms3/`) |
| **MS-4** | Quality Index + 2-bit trigger flags | ✅ Already delivered (`dab_ms4/`) |

---

## Part 4: New Work Required

### 4.1 Memory Model Migration (CRITICAL — affects all files)
Migrate from `malloc`-based `DAB_Decoder_Create()` to a `DAB_Decoder_InitWithMem(void *buf, size_t sz)` pattern matching our MS-3/MS-4 design. All context state lives in the caller-provided buffer.

### 4.2 MS-2 Core Decoders (LARGEST NEW WORK)

**DAB MP2 (MUSICAM) Decoder** — `src/codec_mp2/`
- Complete ISO/IEC 11172-3 Layer II bitstream parsing
- 32-subband bit allocation, scale factor decoding (SCFSI)
- Triplet (3/5/9-level) de-quantization to Q15 fixed-point
- 32-band polyphase synthesis filterbank (512-coefficient window, IDCT)
- Joint Stereo (M/S) and Dual Channel support
- 24 kHz half-rate and 48 kHz full-rate modes
- Output: 1152 samples/frame, 16-bit PCM stereo interleaved

**DAB+ HE-AAC v2 Decoder** — `src/codec_aac/`
- AU header parsing per ETSI TS 102 563 (960-window mandatory)
- AAC-LC: spectral decoding, IMDCT (960-point), TNS, MS/IS stereo
- SBR: HF reconstruction, QMF analysis/synthesis (64-band)
- PS: Parametric Stereo decorrelation and spatialization
- Output: 960/2048 samples (pre/post SBR), 16-bit PCM

### 4.3 Test Harness & Test Streams (STRONG FOCUS per Additional Scope)

**Test Harness** (`harness/`):
- CLI tool: `dab_test_harness --input clean_stream.au --codec mp2|aac --output out.pcm --log report.txt`
- Reads `.au` bitstream files, runs CRC check, decodes, writes PCM + log
- Configurable error injection: `--inject-errors BURST:3:frame10` etc.
- Benchmarking mode: MIPS estimation via cycle counter

**Test Stream Suite** (synthesized programmatically — no copyrighted material):
- `clean_mp2_48k_stereo.au` — 1000 frames, known sinusoid at 1 kHz
- `clean_mp2_24k_mono.au` — 500 frames half-rate mono
- `clean_aac_48k_stereo.au` — 500 DAB+ superframes HE-AAC v2
- `error_injected_single_crc.au` — frame 100 CRC corrupted
- `error_injected_burst5.au` — frames 200–204 lost
- `scenario_channel_switch.au` — alternating good/bad AU sequence
- `stress_24h_seed.au` — procedurally generated 24-hour equivalent stress seed

### 4.4 Documentation Deliverables
- API Reference Manual (HTML + PDF)
- SW Architecture Specification (block diagram, memory map)
- MIPS/Memory Footprint Report (Pure C vs NEON table)
- Bit-exact verification report
- 24-hour stress test results log

---

## Part 5: Package Structure (Final Delivery)

```
dab_automotive_decoder_complete/
├── include/
│   └── dab_decoder.h              ← Unified public API (MS-1 freeze)
├── src/
│   ├── core/
│   │   ├── dab_instance.c         ← Static-memory multi-instance (migrated from malloc)
│   │   └── bitstream_reader.c/h
│   ├── codec_mp2/                 ← Full MS-2 MP2/MUSICAM (NEW)
│   │   ├── mp2_decoder.c/h
│   │   ├── mp2_bitalloc.c/h
│   │   ├── mp2_synth.c/h          ← Full 32-band polyphase synthesis (NEW)
│   │   └── mp2_tables.c/h
│   ├── codec_aac/                 ← Full MS-2 HE-AAC v2 (NEW)
│   │   ├── aac_lc.c/h             ← AAC-LC core (NEW)
│   │   ├── aac_sbr.c/h            ← SBR HF reconstruction (NEW)
│   │   ├── aac_ps.c/h             ← Parametric Stereo (NEW)
│   │   ├── aac_imdct.c/h          ← 960-point IMDCT (NEW)
│   │   ├── aac_huffman.c/h
│   │   └── aac_tables.c/h
│   ├── automotive/                ← MS-3 + MS-4 (already done, integrate)
│   │   ├── concealment.c/h
│   │   ├── soft_mute.c/h
│   │   ├── pop_prevention.c/h
│   │   └── aqi_engine.c/h
│   └── dsp/
│       ├── dsp_math.c/h
│       └── dsp_neon.c/h
├── harness/                       ← Test Harness (NEW)
│   ├── dab_test_harness.c
│   └── au_file_reader.c/h
├── test_streams/                  ← Test Data Suite (NEW)
│   ├── clean_mp2_48k_stereo.au
│   ├── clean_mp2_24k_mono.au
│   ├── clean_aac_48k_stereo.au
│   ├── error_injected_single_crc.au
│   ├── error_injected_burst5.au
│   ├── scenario_channel_switch.au
│   └── generate_streams.py        ← Generator script
├── tests/                         ← Unit + Integration + Stress Tests
│   ├── test_ms1_architecture.c    ← Handle size, API availability
│   ├── test_ms2_mp2_decode.c      ← Bit-exact MP2 decoding
│   ├── test_ms2_aac_decode.c      ← Bit-exact AAC decoding
│   ├── test_ms3_concealment.c     ← (existing, updated)
│   ├── test_ms4_quality.c         ← (existing, updated)
│   ├── test_multi_instance.c      ← 2 simultaneous decoder instances
│   ├── test_robustness.c          ← Error injection scenarios
│   └── test_stress_24h.c          ← 24-hour continuous decode loop
├── docs/
│   ├── api_reference.md
│   ├── architecture_spec.md
│   ├── mips_memory_report.md
│   └── test_report.md
└── CMakeLists.txt
```

---

## Open Questions Before Proceeding

> [!IMPORTANT]
> **Q1: AAC decoder scope** — Implementing a full, production-quality, bit-exact HE-AAC v2 (AAC-LC + SBR + PS) decoder from scratch in C99 is a very substantial undertaking (typically 6–18 months for a standards body). The existing stub clearly does not achieve this. 
>
> **Do you want us to:** 
> - (A) Implement a **functional but simplified** HE-AAC v2 decoder that handles real DAB+ bitstreams correctly (achieves correct audio output) but may not cover every edge-case corner of the spec (PS with complex ICC matrices, etc.)?
> - (B) Implement the **AAC-LC core only** (which is the mandatory part per ETSI TS 102 563) and stub SBR/PS with documented placeholders, to be completed iteratively?
> - (C) Implement all three layers (AAC-LC + SBR + PS) fully, understanding this is a multi-sprint effort?

> [!IMPORTANT]
> **Q2: Test streams format** — The `.au` files in the requirements are raw AU bitstream files (RS pre-decoded). For the "clean stream" test vectors, do you have any reference `.au` files captured from a real DAB/DAB+ receiver? Or should we generate synthetic test vectors (known sinusoidal content with computed AU headers and CRC) that allow bit-exact verification?

> [!WARNING]
> **Q3: Target platform** — The acceptance testing requires an **Odroid N2+ board** (ARM Cortex-A73/A55, aarch64). The NEON code must run there. Are we generating code that you will test there, or do you need us to include a CI/CD script (cross-compilation, remote execution)?

> [!NOTE]
> **Q4: Delivery format** — You asked for a single ZIP. Should the ZIP be self-contained (including a `README.md` with build + test instructions for Odroid N2+) or do you need a formal document package (PDF API reference, MIPS report in a specific format)?

---

## Proposed Execution Order (Once Approved)

1. **Phase 1** — Memory model migration + API freeze (MS-1 consolidation)
2. **Phase 2** — MP2/MUSICAM full decoder with polyphase synthesis (MS-2a)
3. **Phase 3** — HE-AAC v2 core decoder (MS-2b) — scope per Q1 answer
4. **Phase 4** — Integrate all MS-3/MS-4 modules, unify under single CMake
5. **Phase 5** — Test harness + synthetic test stream generation
6. **Phase 6** — Unit tests, multi-instance tests, robustness tests, stress test scaffold
7. **Phase 7** — Documentation package (API ref, arch spec, MIPS/memory report)
8. **Phase 8** — Final ZIP packaging
