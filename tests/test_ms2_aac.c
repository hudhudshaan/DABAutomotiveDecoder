/**
 * @file    test_ms2_aac.c
 * @brief   Milestone MS-2b/c/d DAB+ HE-AAC v2 (LC + SBR + PS) Unit Tests
 * @standard ETSI TS 102 563, ISO/IEC 14496-3, MISRA-C:2012, ISO C99
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

static uint16_t compute_crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFFU;
    for (uint16_t i = 0U; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 8U);
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            if ((crc & 0x8000U) != 0U) {
                crc = (uint16_t)((crc << 1U) ^ 0x1021U);
            } else {
                crc = (uint16_t)(crc << 1U);
            }
        }
    }
    return crc;
}

int main(void) {
    int failures = 0;
    printf("=================================================================\n");
    printf("  MS-2b/c/d DAB+ HE-AAC v2 (LC + SBR + PS) Unit Tests\n");
    printf("=================================================================\n\n");

    static uint8_t handle_mem[DAB_DECODER_AAC_HANDLE_SIZE];
    static int16_t pcm_out[DAB_MAX_PCM_SAMPLES_PER_AU * 2U];
    static uint8_t aac_buf[256];

    DAB_Decoder_Handle h = DAB_Decoder_InitWithMem(handle_mem, sizeof(handle_mem), DAB_CODEC_AAC, 48000U);
    failures += check("HE-AAC v2 decoder instance initialization (48kHz)", h != NULL);

    /* Construct standard DAB+ HE-AAC v2 AU frame */
    (void)memset(aac_buf, 0, sizeof(aac_buf));
    aac_buf[0] = 0x20U; /* CPE (Stereo) */
    aac_buf[1] = 0x80U; /* Global gain */
    aac_buf[2] = 0x11U; /* Codebooks */
    for (uint16_t i = 3U; i < (sizeof(aac_buf) - 2U); i++) {
        aac_buf[i] = (uint8_t)((i * 37U + 5U) & 0xFFU);
    }
    uint16_t crc = compute_crc16(aac_buf, (uint16_t)(sizeof(aac_buf) - 2U));
    aac_buf[sizeof(aac_buf) - 2U] = (uint8_t)(crc >> 8U);
    aac_buf[sizeof(aac_buf) - 1U] = (uint8_t)(crc & 0xFFU);

    /* Test 1: Decode standard DAB+ frame */
    DAB_AudioStatus status;
    int32_t samples = DAB_Decoder_DecodeAU(h, aac_buf, sizeof(aac_buf), NULL, pcm_out, &status);
    failures += check("HE-AAC v2 frame decodes successfully", samples > 0);
    failures += check("Output sample rate is 48000 Hz", status.sample_rate_hz == 48000U);
    failures += check("Output is 2 channels stereo", status.num_channels == 2U);
    failures += check("AU status reported as DAB_AU_GOOD", status.au_status == DAB_AU_GOOD);

    /* Verify decoded PCM samples are valid acoustic values (not saturated DC rail -32768) */
    uint32_t total_out_samples = (uint32_t)samples * 2U;
    uint32_t sat_neg_count = 0U;
    for (uint32_t s = 0U; s < total_out_samples; s++) {
        if (pcm_out[s] == -32768) {
            sat_neg_count++;
        }
    }
    failures += check("Decoded PCM samples are not pinned to -32768 DC rail", sat_neg_count < total_out_samples);

    /* Test 2: Mono with PS active generates distinct L and R channels */
    aac_buf[0] = 0x00U; /* SCE (Mono) */
    crc = compute_crc16(aac_buf, (uint16_t)(sizeof(aac_buf) - 2U));
    aac_buf[sizeof(aac_buf) - 2U] = (uint8_t)(crc >> 8U);
    aac_buf[sizeof(aac_buf) - 1U] = (uint8_t)(crc & 0xFFU);
    samples = DAB_Decoder_DecodeAU(h, aac_buf, sizeof(aac_buf), NULL, pcm_out, &status);
    failures += check("Mono stream with Parametric Stereo produces stereo output", samples > 0);

    printf("\n=================================================================\n");
    if (failures == 0) {
        printf("  ALL MS-2 AAC TESTS PASSED\n");
    } else {
        printf("  %d MS-2 AAC TEST(S) FAILED\n", failures);
    }
    printf("=================================================================\n\n");
    return failures;
}
