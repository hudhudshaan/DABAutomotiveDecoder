/**
 * @file    test_ms3_concealment.c
 * @brief   Milestone MS-3 Multi-Stage Concealment & Soft Mute Unit Tests
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
    printf("  MS-3 Concealment & Soft Mute Unit Tests\n");
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

    /* 1. Good AU */
    DAB_Decoder_DecodeAU(h, au_buf, sizeof(au_buf), NULL, pcm_out, &status);
    failures += check("Good AU reports DAB_CONCEAL_NONE", status.conceal_mode == DAB_CONCEAL_NONE);
    failures += check("Good AU has consec_loss = 0", status.consec_loss == 0U);
    failures += check("Good AU has mute state IDLE and full gain", (status.mute_state == DAB_MUTE_IDLE) && (status.cur_gain_q15 == 32767));

    /* 2. Single Lost Frame (1st loss) -> INTERPOLATE */
    DAB_Decoder_DecodeAU(h, NULL, 0U, NULL, pcm_out, &status);
    failures += check("1st lost AU triggers DAB_CONCEAL_INTERPOLATE", status.conceal_mode == DAB_CONCEAL_INTERPOLATE);
    failures += check("1st lost AU has consec_loss = 1", status.consec_loss == 1U);

    /* 3. Second Lost Frame (2nd loss) -> INTERPOLATE */
    DAB_Decoder_DecodeAU(h, NULL, 0U, NULL, pcm_out, &status);
    failures += check("2nd lost AU maintains DAB_CONCEAL_INTERPOLATE", status.conceal_mode == DAB_CONCEAL_INTERPOLATE);
    failures += check("2nd lost AU has consec_loss = 2", status.consec_loss == 2U);

    /* 4. Third Lost Frame (3rd loss) -> ATTENUATE */
    DAB_Decoder_DecodeAU(h, NULL, 0U, NULL, pcm_out, &status);
    failures += check("3rd lost AU enters DAB_CONCEAL_ATTENUATE", status.conceal_mode == DAB_CONCEAL_ATTENUATE);
    failures += check("3rd lost AU has consec_loss = 3", status.consec_loss == 3U);

    /* 5. Consecutive losses 4, 5, 6 -> ATTENUATE */
    DAB_Decoder_DecodeAU(h, NULL, 0U, NULL, pcm_out, &status);
    DAB_Decoder_DecodeAU(h, NULL, 0U, NULL, pcm_out, &status);
    DAB_Decoder_DecodeAU(h, NULL, 0U, NULL, pcm_out, &status);
    failures += check("6th lost AU remains in DAB_CONCEAL_ATTENUATE", status.conceal_mode == DAB_CONCEAL_ATTENUATE);

    /* 6. 7th Lost Frame -> MUTED */
    DAB_Decoder_DecodeAU(h, NULL, 0U, NULL, pcm_out, &status);
    failures += check("7th lost AU triggers DAB_CONCEAL_MUTED", status.conceal_mode == DAB_CONCEAL_MUTED);

    /* 7. Recovery Frame: Good AU arrives */
    DAB_Decoder_DecodeAU(h, au_buf, sizeof(au_buf), NULL, pcm_out, &status);
    failures += check("Recovery AU returns to DAB_CONCEAL_NONE", status.conceal_mode == DAB_CONCEAL_NONE);
    failures += check("Recovery AU triggers soft mute release", status.mute_state == DAB_MUTE_RELEASING);
    failures += check("Recovery AU resets consec_loss to 0", status.consec_loss == 0U);

    printf("\n=================================================================\n");
    if (failures == 0) {
        printf("  ALL MS-3 CONCEALMENT TESTS PASSED\n");
    } else {
        printf("  %d MS-3 CONCEALMENT TEST(S) FAILED\n", failures);
    }
    printf("=================================================================\n\n");
    return failures;
}
