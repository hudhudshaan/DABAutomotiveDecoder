#!/bin/bash
# ==============================================================================
# Automated Build & Full Milestone Verification Script (MS-1 to MS-4)
# ==============================================================================
set -e

export PATH="/ucrt64/bin:/usr/bin:$PATH"

echo "================================================================="
echo "  Automotive DAB/DAB+ Audio Decoder (MS-1 to MS-4) Build & Test  "
echo "  Compiler: $(gcc --version | head -n1)                          "
echo "================================================================="

cd "$(dirname "$0")"

CFLAGS="-std=c99 -Wall -Wextra -Wpedantic -Werror -O2 -fno-common"
INCLUDES="-Iinclude -Isrc/core -Isrc/dsp -Isrc/automotive -Isrc/codec_mp2 -Isrc/codec_aac"

echo "[1/4] Compiling Core Library Object Files..."
SRCS=(
    src/dsp/dsp_math.c
    src/dsp/dsp_neon.c
    src/core/bitstream_reader.c
    src/automotive/dab_crc.c
    src/automotive/soft_mute.c
    src/automotive/aqi_engine.c
    src/automotive/pop_prevention.c
    src/automotive/concealment.c
    src/codec_mp2/mp2_tables.c
    src/codec_mp2/mp2_synth.c
    src/codec_mp2/mp2_decoder.c
    src/codec_aac/aac_tables.c
    src/codec_aac/aac_huffman.c
    src/codec_aac/aac_imdct.c
    src/codec_aac/aac_sbr.c
    src/codec_aac/aac_ps.c
    src/codec_aac/aac_decoder.c
    src/core/dab_instance.c
)

OBJS=()
for src in "${SRCS[@]}"; do
    obj="${src%.c}.o"
    gcc $CFLAGS $INCLUDES -c "$src" -o "$obj"
    OBJS+=("$obj")
done

echo "[2/4] Creating Static Library libdab_decoder.a..."
ar rcs libdab_decoder.a "${OBJS[@]}"

echo "[3/4] Compiling Executables & Test Suite..."
gcc $CFLAGS $INCLUDES harness/dab_test_harness.c -L. -ldab_decoder -o harness/dab_test_harness.exe
gcc $CFLAGS test_streams/generate_streams.c -o test_streams/generate_streams.exe

TEST_APPS=(
    tests/test_ms1_api
    tests/test_ms2_mp2
    tests/test_ms2_aac
    tests/test_ms3_concealment
    tests/test_ms4_quality
    tests/test_multi_instance
    tests/test_robustness
    tests/test_stress_24h
)

for t in "${TEST_APPS[@]}"; do
    gcc $CFLAGS $INCLUDES "${t}.c" -L. -ldab_decoder -o "${t}.exe"
done

echo "[4/4] Executing Synthetic Stream Generation & Full Test Suite..."
test_streams/generate_streams.exe test_streams

echo ""
echo "================================================================="
echo "  RUNNING ALL 8 TEST SUITES (MS-1 THROUGH MS-4)                  "
echo "================================================================="

for t in "${TEST_APPS[@]}"; do
    echo "Running ${t}.exe..."
    "./${t}.exe" || exit 1
done

echo ""
echo "================================================================="
echo "  RUNNING TEST HARNESS ON ERROR-INJECTED SCENARIO STREAM         "
echo "================================================================="
harness/dab_test_harness.exe --input test_streams/scenario_channel_switch.au \
    --codec mp2 --sample-rate 48000 \
    --output test_streams/out_scenario.pcm \
    --wav test_streams/out_scenario.wav \
    --log test_streams/out_scenario.log

echo ""
echo "================================================================="
echo "  BUILD & TEST SUCCESSFUL — 100% PASS RATE ON ALL CRITERIA!       "
echo "================================================================="
