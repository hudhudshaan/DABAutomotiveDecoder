# ==============================================================================
# Makefile — Automotive DAB/DAB+ Audio Decoder (MS-1 to MS-4)
# Standards: ETSI EN 300 401, ETSI TS 102 563, ISO/IEC 11172-3, ISO/IEC 14496-3
# Compliance: MISRA-C:2012, ISO C99, Zero malloc, Zero global mutable variables
# ==============================================================================

CC ?= gcc
AR ?= ar
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 -fno-common
INCLUDES = -Iinclude -Isrc/core -Isrc/dsp -Isrc/automotive -Isrc/codec_mp2 -Isrc/codec_aac

# Detect aarch64 for NEON
ARCH := $(shell $(CC) -dumpmachine 2>/dev/null)
ifneq (,$(findstring aarch64,$(ARCH)))
    CFLAGS += -D__ARM_NEON -D__aarch64__ -march=armv8-a+simd
endif

SRC = \
    src/dsp/dsp_math.c \
    src/dsp/dsp_neon.c \
    src/core/bitstream_reader.c \
    src/automotive/dab_crc.c \
    src/automotive/soft_mute.c \
    src/automotive/aqi_engine.c \
    src/automotive/pop_prevention.c \
    src/automotive/concealment.c \
    src/codec_mp2/mp2_tables.c \
    src/codec_mp2/mp2_synth.c \
    src/codec_mp2/mp2_decoder.c \
    src/codec_aac/aac_tables.c \
    src/codec_aac/aac_huffman.c \
    src/codec_aac/aac_imdct.c \
    src/codec_aac/aac_sbr.c \
    src/codec_aac/aac_ps.c \
    src/codec_aac/aac_decoder.c \
    src/core/dab_instance.c

OBJ = $(SRC:.c=.o)
LIB = libdab_decoder.a

TESTS = \
    tests/test_ms1_api \
    tests/test_ms2_mp2 \
    tests/test_ms2_aac \
    tests/test_ms3_concealment \
    tests/test_ms4_quality \
    tests/test_multi_instance \
    tests/test_robustness \
    tests/test_stress_24h

.PHONY: all clean test

all: $(LIB) harness/dab_test_harness harness/automotive_decoder_runner test_streams/generate_streams $(TESTS)

$(LIB): $(OBJ)
	$(AR) rcs $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

harness/dab_test_harness: harness/dab_test_harness.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -ldab_decoder -o $@

harness/automotive_decoder_runner: harness/automotive_decoder_runner.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -ldab_decoder -o $@

test_streams/generate_streams: test_streams/generate_streams.c
	$(CC) $(CFLAGS) $< -o $@

tests/%: tests/%.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -ldab_decoder -o $@

test: $(TESTS) test_streams/generate_streams
	@echo "--- Generating synthetic AU test streams ---"
	@./test_streams/generate_streams test_streams
	@echo "--- Running MS-1 to MS-4 Test Suite ---"
	@for t in $(TESTS); do ./$$t || exit 1; done
	@echo "--- All Tests Successfully Passed! ---"

clean:
	rm -f $(OBJ) $(LIB) harness/dab_test_harness harness/automotive_decoder_runner test_streams/generate_streams $(TESTS)
	rm -f harness/*.exe test_streams/*.exe tests/*.exe *.exe
