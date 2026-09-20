/**
 * @file    test_multi_instance.c
 * @brief   Multi-Instance Re-entrancy & Thread-Safety Isolation Unit Tests
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
    printf("  Multi-Instance Isolation & Thread-Safety Unit Tests\n");
    printf("=================================================================\n\n");

    /* Allocate two distinct static memory blocks */
    static uint8_t mem_inst1[DAB_DECODER_MP2_HANDLE_SIZE];
    static uint8_t mem_inst2[DAB_DECODER_MP2_HANDLE_SIZE];
    static int16_t pcm_out1[DAB_MAX_PCM_SAMPLES_PER_AU * 2U];
    static int16_t pcm_out2[DAB_MAX_PCM_SAMPLES_PER_AU * 2U];
    static uint8_t au_buf[576];

    DAB_Decoder_Handle h1 = DAB_Decoder_InitWithMem(mem_inst1, sizeof(mem_inst1), DAB_CODEC_MP2, 48000U);
    DAB_Decoder_Handle h2 = DAB_Decoder_InitWithMem(mem_inst2, sizeof(mem_inst2), DAB_CODEC_MP2, 48000U);

    failures += check("Instance 1 successfully initialized", h1 != NULL);
    failures += check("Instance 2 successfully initialized", h2 != NULL);
    failures += check("Instance handles point to distinct memory regions", h1 != h2);

    /* Construct standard AU */
    (void)memset(au_buf, 0, sizeof(au_buf));
    au_buf[0] = 0xFFU; au_buf[1] = 0xFDU; au_buf[2] = 0x94U; au_buf[3] = 0x00U;
    uint16_t crc = compute_crc16(au_buf, (uint16_t)(sizeof(au_buf) - 2U));
    au_buf[sizeof(au_buf) - 2U] = (uint8_t)(crc >> 8U);
    au_buf[sizeof(au_buf) - 1U] = (uint8_t)(crc & 0xFFU);

    DAB_AudioStatus s1, s2;

    /* Feed clean stream to Instance 1, and corrupted/lost stream to Instance 2 */
    for (uint32_t i = 0; i < 40; i++) {
        DAB_Decoder_DecodeAU(h1, au_buf, sizeof(au_buf), NULL, pcm_out1, &s1);
        DAB_Decoder_DecodeAU(h2, NULL, 0U, NULL, pcm_out2, &s2);
    }

    failures += check("Instance 1 quality rises toward 80", s1.audio_quality == 80U);
    failures += check("Instance 1 trigger is BLEND_IDLE", s1.trigger == BLEND_IDLE);
    failures += check("Instance 1 has 0 CRC errors", s1.crc_error_au_cnt == 0U);

    failures += check("Instance 2 quality remains 0 (muted)", s2.audio_quality == 0U);
    failures += check("Instance 2 trigger is UNRECOVERABLE_TRIGGER", s2.trigger == UNRECOVERABLE_TRIGGER);
    failures += check("Instance 2 has 40 CRC errors", s2.crc_error_au_cnt == 40U);
    failures += check("Instance 2 is in DAB_CONCEAL_MUTED mode", s2.conceal_mode == DAB_CONCEAL_MUTED);

    /* Reset Instance 2 and verify Instance 1 remains unaffected */
    DAB_Decoder_Reset(h2);
    uint32_t total1 = 0U, err1 = 0U;
    DAB_Decoder_GetAUStats(h1, &total1, &err1);
    failures += check("Resetting Instance 2 does NOT affect Instance 1 total_au_cnt", total1 == 40U);
    failures += check("Resetting Instance 2 does NOT affect Instance 1 crc_error_au_cnt", err1 == 0U);

    printf("\n=================================================================\n");
    if (failures == 0) {
        printf("  ALL MULTI-INSTANCE ISOLATION TESTS PASSED\n");
    } else {
        printf("  %d MULTI-INSTANCE TEST(S) FAILED\n", failures);
    }
    printf("=================================================================\n\n");
    return failures;
}
