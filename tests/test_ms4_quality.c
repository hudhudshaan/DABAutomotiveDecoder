/**
 * @file    test_ms4_quality.c
 * @brief   Milestone MS-4 Quality Index & 2-bit Seamless Blending Trigger Unit Tests
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
    printf("  MS-4 Quality Index & Seamless Blending Trigger Unit Tests\n");
    printf("=================================================================\n\n");

    static uint8_t handle_mem[DAB_DECODER_MP2_HANDLE_SIZE];
    static int16_t pcm_out[DAB_MAX_PCM_SAMPLES_PER_AU * 2U];
    static uint8_t au_buf[576];

    DAB_Decoder_Handle h = DAB_Decoder_InitWithMem(handle_mem, sizeof(handle_mem), DAB_CODEC_MP2, 48000U);
    (void)memset(au_buf, 0, sizeof(au_buf));
    au_buf[0] = 0xFFU; au_buf[1] = 0xFDU; au_buf[2] = 0x94U; au_buf[3] = 0x00U;
    uint16_t crc = compute_crc16(au_buf, (uint16_t)(sizeof(au_buf) - 2U));
    au_buf[sizeof(au_buf) - 2U] = (uint8_t)(crc >> 8U);
    au_buf[sizeof(au_buf) - 1U] = (uint8_t)(crc & 0xFFU);

    DAB_AudioStatus status;
    (void)memset(&status, 0, sizeof(status));

    /* 1. Initial State */
    uint8_t init_q = 99U;
    DAB_BlendingTriggerState init_t = CONCEAL_TRIGGER;
    DAB_Decoder_GetQualityStatus(h, &init_q, &init_t);
    failures += check("Initial Quality Score is 0", init_q == 0U);
    failures += check("Initial Blending Trigger is BLEND_IDLE", init_t == BLEND_IDLE);

    /* 2. Slow-Rise: 50 good frames -> quality should reach 100 */
    for (uint32_t i = 0; i < 50; i++) {
        DAB_Decoder_DecodeAU(h, au_buf, sizeof(au_buf), NULL, pcm_out, &status);
    }
    failures += check("After 50 good frames, quality reaches maximum 100", status.audio_quality == 100U);
    failures += check("During healthy playback, Blending Trigger is BLEND_IDLE (0b00)", status.trigger == BLEND_IDLE);

    /* 3. Fast-Drop: Single corrupted frame -> quality drops immediately to 0 */
    DAB_Decoder_DecodeAU(h, NULL, 0U, NULL, pcm_out, &status);
    failures += check("Single lost frame causes instant Fast-Drop to 0", status.audio_quality == 0U);
    failures += check("Lost frame sets Blending Trigger to CONCEAL_TRIGGER (0b01)", status.trigger == CONCEAL_TRIGGER);

    /* 4. Extended loss -> triggers UNRECOVERABLE_TRIGGER (0b10) */
    for (uint32_t i = 0; i < 7; i++) {
        DAB_Decoder_DecodeAU(h, NULL, 0U, NULL, pcm_out, &status);
    }
    failures += check("Extended loss (>6 AUs) sets trigger to UNRECOVERABLE_TRIGGER (0b10)", status.trigger == UNRECOVERABLE_TRIGGER);

    /* 5. Telemetry Statistics Counters */
    uint32_t total_au = 0U, crc_err = 0U;
    DAB_Decoder_GetAUStats(h, &total_au, &crc_err);
    failures += check("total_au_cnt correctly tracked (50 + 1 + 7 = 58)", total_au == 58U);
    failures += check("crc_error_au_cnt correctly tracked (8 errors)", crc_err == 8U);

    /* 6. Reset Statistics */
    DAB_Decoder_ResetStats(h);
    DAB_Decoder_GetAUStats(h, &total_au, &crc_err);
    failures += check("ResetStats resets total_au_cnt to 0", total_au == 0U);
    failures += check("ResetStats resets crc_error_au_cnt to 0", crc_err == 0U);

    printf("\n=================================================================\n");
    if (failures == 0) {
        printf("  ALL MS-4 QUALITY & TRIGGER TESTS PASSED\n");
    } else {
        printf("  %d MS-4 QUALITY & TRIGGER TEST(S) FAILED\n", failures);
    }
    printf("=================================================================\n\n");
    return failures;
}
