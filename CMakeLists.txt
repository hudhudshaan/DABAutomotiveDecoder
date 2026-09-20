# ==============================================================================
# Master CMakeLists.txt — Automotive DAB/DAB+ Audio Decoder (MS-1 to MS-4)
# Standards: ETSI EN 300 401, ETSI TS 102 563, ISO/IEC 11172-3, ISO/IEC 14496-3
# Compliance: MISRA-C:2012, ISO C99, Zero malloc, Zero global mutable variables
# Platforms: x86 Windows (MSYS2/UCRT64), x86 Linux, Odroid N2+ (aarch64)
# ==============================================================================

cmake_minimum_required(VERSION 3.16)
project(dab_automotive_decoder VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Compiler warnings and strict automotive safety flags
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Werror
        -fno-common
    )
endif()

# ARM NEON acceleration option
option(DAB_ENABLE_NEON "Enable ARMv8-A NEON SIMD optimizations" OFF)
if(DAB_ENABLE_NEON)
    add_compile_definitions(__ARM_NEON __aarch64__)
    if(NOT MSVC)
        add_compile_options(-march=armv8-a+simd)
    endif()
    message(STATUS "ARM NEON SIMD vectorization: ENABLED")
else()
    message(STATUS "ARM NEON SIMD vectorization: DISABLED (Pure C99 mode)")
endif()

# Master include directories
include_directories(
    include
    src/core
    src/dsp
    src/automotive
    src/codec_mp2
    src/codec_aac
)

# Core decoder static library
add_library(dab_decoder STATIC
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

# Host-side Test Harness CLI
add_executable(dab_test_harness harness/dab_test_harness.c)
target_link_libraries(dab_test_harness PRIVATE dab_decoder)

# ==============================================================================
# INTEGRATION: Link automotive targets to decoder static library
# ==============================================================================
# 1. Define the executable target using your C runner file
add_executable(automotive_decoder harness/dab_test_harness.c)
target_link_libraries(automotive_decoder PRIVATE dab_decoder)
# ==============================================================================

# Synthetic Test Stream Generator
add_executable(generate_streams test_streams/generate_streams.c)

# Enable CTest suite
enable_testing()

# Unit tests
set(TEST_TARGETS
    test_ms1_api
    test_ms2_mp2
    test_ms2_aac
    test_ms3_concealment
    test_ms4_quality
    test_multi_instance
    test_robustness
    test_stress_24h
)

foreach(test_name ${TEST_TARGETS})
    add_executable(${test_name} tests/${test_name}.c)
    target_link_libraries(${test_name} PRIVATE dab_decoder)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# ODR-AudioEnc Dedicated Pipeline Test Target
add_executable(automotive_decoder_runner harness/automotive_decoder_runner.c)
target_link_libraries(automotive_decoder_runner PRIVATE dab_decoder)
