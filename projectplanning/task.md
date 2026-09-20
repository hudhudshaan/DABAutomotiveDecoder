# MS-1 to MS-4 Complete DAB/DAB+ Decoder — Task Tracker

## Phase 0: Setup
- [x] Read and parse Additional scope.docx
- [x] Audit existing dab_auto_audio_decoder code
- [x] Get user clarification on Q1-Q4
- [x] Confirm requirements with user
- [x] Create final package directory structure

## Phase 1: Core Infrastructure (MS-1 Architecture Freeze)
- [x] Migrate dab_instance.c: calloc→InitWithMem, static memory model
- [x] Update dab_decoder.h: add DAB_Decoder_GetHandleSize(), DAB_Decoder_InitWithMem()
- [x] MISRA fixes: automotive/ layer (log10→LUT, cosf, parameter mutation, default: clauses)
- [x] Fix NEON guard: || → && in dsp_math.h
- [x] Unified CMakeLists.txt with NEON ON/OFF, cross-compile support
- [x] cross_compile_aarch64.cmake toolchain file for Odroid N2+
- [x] README.md with build + test instructions

## Phase 2a: MP2/MUSICAM Decoder (MS-2)
- [x] Fix header sync parse (3-bit field, not 2-bit)
- [x] Add default: to switch(scfsi) in mp2_decoder.c
- [x] Implement Joint Stereo (M/S intensity coupling, mode_extension subbands)
- [x] Implement Dual Channel (independent L/R processing)
- [x] Implement Mono downmix
- [x] Complete 32-band polyphase synthesis filterbank (512-coeff window, IDCT)
- [x] NEON-accelerated polyphase synthesis (8-sample vector ops)
- [x] Validate: 1152 samples/frame at 48kHz, 576 at 24kHz

## Phase 2b: AAC-LC Core (MS-2)
- [x] ETSI TS 102 563 AU superframe header parser (AU start-offset table)
- [x] All 11 Huffman codebooks (CB1-CB11 + scale factor table)
- [x] Bounded escape sequence decoder (max n guard in aac_huffman.c)
- [x] 960-point IMDCT (long window) — KBD and sine window
- [x] 8×120-point IMDCT (short window sequence) — overlap-add
- [x] Long-start and Long-stop window transitions
- [x] Temporal Noise Shaping (TNS) up to order 20
- [x] M/S (Mid/Side) stereo decoding
- [x] Intensity Stereo (IS) decoding
- [x] Quantized spectral coefficients dequantization (|X|^(4/3) via LUT, no pow())
- [x] Fix parameter mutation (num_channels via output pointer)
- [x] Move time_buf[1024] + sbr_tmp[2048] into AAC state struct

## Phase 2c: SBR — Spectral Band Replication (MS-2)
- [x] ETSI TS 102 563 SBR header parsing
- [x] QMF analysis filterbank: 32 bands (prototype filter 640 coefficients)
- [x] SBR envelope decoding and dequantization
- [x] HF (High Frequency) generation via transpositor
- [x] HF envelope adjustment
- [x] Limiter
- [x] QMF synthesis filterbank: 64 bands
- [x] NEON-accelerated QMF matrix multiply

## Phase 2d: PS — Parametric Stereo (MS-2)
- [x] Hybrid QMF analysis (71 subbands: 32 QMF + additional hybrid split)
- [x] PS header parsing (IID/ICC/IPD/OPD parameters)
- [x] IID (Inter-channel Intensity Difference) processing
- [x] ICC (Inter-channel Coherence Correlation) decorrelation
- [x] IPD/OPD (phase difference) — optional but include
- [x] Hybrid QMF synthesis

## Phase 3: Integration (MS-3 + MS-4 with new codec cores)
- [x] Integrate MP2 + AAC into unified dab_instance.c
- [x] Integrate MS-3 concealment into unified engine
- [x] Integrate MS-4 AQI + trigger flags into unified engine
- [x] Handle size calculation and static assertion

## Phase 4: Test Harness
- [x] dab_test_harness.c CLI tool
  - [x] --input, --codec, --output, --log, --error-inject flags
  - [x] .au file reader (header + payload)
  - [x] PCM .wav output writer
  - [x] Execution log with per-frame status
  - [x] MIPS estimation via clock_gettime
- [x] au_file_reader.c/h
- [x] au_file_writer.c/h

## Phase 5: Synthetic Test Stream Generator
- [x] generate_streams.c: generates all .au test files programmatically
  - [x] clean_mp2_48k_stereo.au (1000 frames, 1kHz sine, known CRC)
  - [x] clean_mp2_24k_mono.au (500 frames half-rate mono)
  - [x] clean_aac_48k_stereo.au (500 DAB+ superframes)
  - [x] error_injected_single_crc.au (frame 100 CRC corrupted)
  - [x] error_injected_burst5.au (frames 200-204 lost)
  - [x] scenario_channel_switch.au (alternating good/bad)
- [x] README_streams.md (stream format documentation)

## Phase 6: Test Suite
- [x] test_ms1_api.c: handle size, API existence, InitWithMem
- [x] test_ms2_mp2.c: bit-exact MP2 decode, all modes (Stereo/JS/DC/Mono)
- [x] test_ms2_aac.c: bit-exact AAC-LC decode, 960-window verify
- [x] test_ms3_concealment.c: concealment FSM all transitions
- [x] test_ms4_quality.c: AQI fast-drop/slow-rise, trigger flags
- [x] test_multi_instance.c: 2 simultaneous instances, state isolation
- [x] test_robustness.c: error injection, burst loss, malformed AU
- [x] test_stress_24h.c: 24-hour equivalent continuous decode scaffold

## Phase 7: Documentation
- [x] docs/api_reference.md: all public functions, types, constants
- [x] docs/architecture_spec.md: block diagram, memory map, state machines
- [x] docs/mips_memory_report.md: Pure C vs NEON comparison, handle sizes
- [x] docs/test_report.md: test results, bit-exact verification, stress results

## Phase 8: Final Packaging
- [x] Final compilation check (all targets)
- [x] Run all tests
- [x] Create ZIP: dab_automotive_decoder_package.zip
