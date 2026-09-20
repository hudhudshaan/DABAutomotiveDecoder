/**
 * @file    test_robustness.c
 * @brief   Automotive Robustness, Fuzzing & Malformed AU Handling Unit Tests
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "dab_decoder.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int check(const char *test_name, int pass) {
    if (pass) {
        printf("  [PASS] %-60s\n", test_name);
        return 0;
    } else {
        printf("  [FAIL] %-60s\n", test_name);
        return 1;
    }
}

int main(void) {
    int failures = 0;
    printf("=================================================================\n");
    printf("  Automotive Robustness & Fuzzing Handling Unit Tests\n");
    printf("=================================================================\n\n");

    static uint8_t handle_mem[DAB_DECODER_MP2_HANDLE_SIZE];
    static int16_t pcm_out[DAB_MAX_PCM_SAMPLES_PER_AU * 2U];
    static uint8_t garbage_payload[1024];

    DAB_Decoder_Handle h = DAB_Decoder_InitWithMem(handle_mem, sizeof(handle_mem), DAB_CODEC_MP2, 48000U);
    failures += check("Decoder instance successfully created", h != NULL);

    /* Test 1: NULL handle safety */
    DAB_AudioStatus status;
    int32_t ret_null = DAB_Decoder_DecodeAU(NULL, garbage_payload, 100U, NULL, pcm_out, &status);
    failures += check("DecodeAU with NULL handle returns DAB_ERR_NULL_HANDLE", ret_null == DAB_ERR_NULL_HANDLE);

    /* Test 2: NULL PCM output buffer safety */
    int32_t ret_pcm_null = DAB_Decoder_DecodeAU(h, garbage_payload, 100U, NULL, NULL, &status);
    failures += check("DecodeAU with NULL pcm_out returns DAB_ERR_NULL_HANDLE", ret_pcm_null == DAB_ERR_NULL_HANDLE);

    /* Test 3: Zero-length AU safety (treated as lost frame) */
    int32_t ret_zero_len = DAB_Decoder_DecodeAU(h, garbage_payload, 0U, NULL, pcm_out, &status);
    failures += check("DecodeAU with 0 length safely conceals and returns sample count", ret_zero_len > 0);
    failures += check("Status reports DAB_AU_LOST on 0 length input", status.au_status == DAB_AU_LOST);

    /* Test 4: Extremely short AU (< 3 bytes) */
    int32_t ret_short = DAB_Decoder_DecodeAU(h, garbage_payload, 2U, NULL, pcm_out, &status);
    failures += check("DecodeAU with < 3 bytes safely conceals as lost AU", ret_short > 0);

    /* Test 5: Fuzzing with pseudo-random garbage bytes (1000 frames) */
    int survived_fuzz = 1;
    uint32_t lfsr = 0xACE1U;
    for (uint32_t f = 0; f < 1000; f++) {
        for (uint32_t b = 0; b < 256; b++) {
            uint32_t bit = ((lfsr >> 0U) ^ (lfsr >> 2U) ^ (lfsr >> 3U) ^ (lfsr >> 5U)) & 1U;
            lfsr = (lfsr >> 1U) | (bit << 15U);
            garbage_payload[b] = (uint8_t)(lfsr & 0xFFU);
        }
        int32_t res = DAB_Decoder_DecodeAU(h, garbage_payload, 256U, NULL, pcm_out, &status);
        if (res < 0) {
            survived_fuzz = 0;
            break;
        }
    }
    failures += check("Survived 1,000 frames of random bitstream fuzzing without crash", survived_fuzz);

    /* Test 6: Baseband error metadata flag */
    DAB_SignalStatus sig_err;
    sig_err.au_crc_pass = 0U; /* explicitly marked bad by baseband */
    sig_err.bb_rs_pass  = 0U;
    sig_err.ber_level   = 255U;
    sig_err.snr_db      = -10;
    (void)DAB_Decoder_DecodeAU(h, garbage_payload, 256U, &sig_err, pcm_out, &status);
    failures += check("Baseband signal au_crc_pass=0 directly triggers CRC_ERR", status.au_status == DAB_AU_CRC_ERR);

    printf("\n=================================================================\n");
    if (failures == 0) {
        printf("  ALL ROBUSTNESS & FUZZING TESTS PASSED\n");
    } else {
        printf("  %d ROBUSTNESS TEST(S) FAILED\n", failures);
    }
    printf("=================================================================\n\n");
    return failures;
}
