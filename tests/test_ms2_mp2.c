/**
 * @file    test_ms2_mp2.c
 * @brief   Milestone MS-2a DAB MUSICAM (MPEG-1/2 Layer II) Unit Tests
 * @standard ISO/IEC 11172-3, ETSI EN 300 401, MISRA-C:2012, ISO C99
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

/* Helper to compute AU CRC-16 */
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
    printf("  MS-2a DAB MUSICAM (ISO/IEC 11172-3 Layer II) Unit Tests\n");
    printf("=================================================================\n\n");

    static uint8_t handle_mem[DAB_DECODER_MP2_HANDLE_SIZE];
    static int16_t pcm_out[DAB_MAX_PCM_SAMPLES_PER_AU * 2U];
    static uint8_t au_buf[576];

    DAB_Decoder_Handle h = DAB_Decoder_InitWithMem(handle_mem, sizeof(handle_mem), DAB_CODEC_MP2, 48000U);
    failures += check("MP2 decoder instance initialization (48kHz Stereo)", h != NULL);

    /* Construct standard 48kHz Stereo MP2 AU frame */
    (void)memset(au_buf, 0, sizeof(au_buf));
    au_buf[0] = 0xFFU;
    au_buf[1] = 0xFDU; /* Layer II, Prot 1 */
    au_buf[2] = 0x94U; /* 192k, 48k */
    au_buf[3] = 0x00U; /* Stereo (mode 0) */
    au_buf[4] = 0x11U; /* Allocations */
    au_buf[5] = 0x22U;
    for (uint16_t i = 6U; i < (sizeof(au_buf) - 2U); i++) {
        au_buf[i] = (uint8_t)((i * 17U) & 0xFFU);
    }
    uint16_t crc = compute_crc16(au_buf, (uint16_t)(sizeof(au_buf) - 2U));
    au_buf[sizeof(au_buf) - 2U] = (uint8_t)(crc >> 8U);
    au_buf[sizeof(au_buf) - 1U] = (uint8_t)(crc & 0xFFU);

    /* Test 1: Decode Stereo 48kHz frame */
    DAB_AudioStatus status;
    int32_t samples = DAB_Decoder_DecodeAU(h, au_buf, sizeof(au_buf), NULL, pcm_out, &status);
    failures += check("Stereo 48kHz frame decodes exactly 1152 samples/channel", samples == 1152);
    failures += check("AU status reported as DAB_AU_GOOD", status.au_status == DAB_AU_GOOD);
    failures += check("Output PCM contains non-zero synthesized audio", (pcm_out[0] != 0) || (pcm_out[10] != 0));

    /* Test 2: Joint Stereo frame decoding */
    au_buf[3] = 0x40U; /* Mode 1 (Joint Stereo), mode_extension 0 (jsbound = 4) */
    crc = compute_crc16(au_buf, (uint16_t)(sizeof(au_buf) - 2U));
    au_buf[sizeof(au_buf) - 2U] = (uint8_t)(crc >> 8U);
    au_buf[sizeof(au_buf) - 1U] = (uint8_t)(crc & 0xFFU);
    samples = DAB_Decoder_DecodeAU(h, au_buf, sizeof(au_buf), NULL, pcm_out, &status);
    failures += check("Joint Stereo frame decodes 1152 samples without error", samples == 1152);

    /* Test 3: Dual Channel frame decoding */
    au_buf[3] = 0x80U; /* Mode 2 (Dual Channel) */
    crc = compute_crc16(au_buf, (uint16_t)(sizeof(au_buf) - 2U));
    au_buf[sizeof(au_buf) - 2U] = (uint8_t)(crc >> 8U);
    au_buf[sizeof(au_buf) - 1U] = (uint8_t)(crc & 0xFFU);
    samples = DAB_Decoder_DecodeAU(h, au_buf, sizeof(au_buf), NULL, pcm_out, &status);
    failures += check("Dual Channel frame decodes 1152 samples without error", samples == 1152);

    /* Test 4: Half-Rate 24kHz Mono decoding */
    DAB_Decoder_Handle h24 = DAB_Decoder_InitWithMem(handle_mem, sizeof(handle_mem), DAB_CODEC_MP2, 24000U);
    static uint8_t au24[288];
    (void)memset(au24, 0, sizeof(au24));
    au24[0] = 0xFFU;
    au24[1] = 0xF5U; /* MPEG-2 LSF, Layer II */
    au24[2] = 0x56U; /* 24 kHz */
    au24[3] = 0xC0U; /* Mono */
    for (uint16_t i = 4U; i < (sizeof(au24) - 2U); i++) {
        au24[i] = (uint8_t)((i * 31U) & 0xFFU);
    }
    crc = compute_crc16(au24, (uint16_t)(sizeof(au24) - 2U));
    au24[sizeof(au24) - 2U] = (uint8_t)(crc >> 8U);
    au24[sizeof(au24) - 1U] = (uint8_t)(crc & 0xFFU);
    samples = DAB_Decoder_DecodeAU(h24, au24, sizeof(au24), NULL, pcm_out, &status);
    failures += check("24kHz Half-Rate decodes exactly 576 samples/channel", samples == 576);

    printf("\n=================================================================\n");
    if (failures == 0) {
        printf("  ALL MS-2a MP2 TESTS PASSED\n");
    } else {
        printf("  %d MS-2a MP2 TEST(S) FAILED\n", failures);
    }
    printf("=================================================================\n\n");
    return failures;
}
