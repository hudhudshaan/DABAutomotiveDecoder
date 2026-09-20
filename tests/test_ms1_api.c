/**
 * @file    test_ms1_api.c
 * @brief   Milestone MS-1 Architecture Freeze & Static Memory API Unit Tests
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
    printf("  MS-1 Architecture Freeze & Static Memory API Unit Tests\n");
    printf("=================================================================\n\n");

    /* Test 1: Query required handle buffer sizes */
    size_t mp2_size = DAB_Decoder_GetHandleSize(DAB_CODEC_MP2);
    size_t aac_size = DAB_Decoder_GetHandleSize(DAB_CODEC_AAC);
    failures += check("DAB_Decoder_GetHandleSize(MP2) returns expected size", mp2_size == DAB_DECODER_MP2_HANDLE_SIZE);
    failures += check("DAB_Decoder_GetHandleSize(AAC) returns expected size", aac_size == DAB_DECODER_AAC_HANDLE_SIZE);

    /* Test 2: Buffer too small rejection */
    static uint8_t tiny_buf[128];
    DAB_Decoder_Handle h_tiny = DAB_Decoder_InitWithMem(tiny_buf, sizeof(tiny_buf), DAB_CODEC_MP2, 48000U);
    failures += check("InitWithMem rejects buffer smaller than required context size", h_tiny == NULL);

    /* Test 3: NULL buffer rejection */
    DAB_Decoder_Handle h_null = DAB_Decoder_InitWithMem(NULL, 100000U, DAB_CODEC_MP2, 48000U);
    failures += check("InitWithMem rejects NULL buffer", h_null == NULL);

    /* Test 4: Invalid sample rate rejection */
    static uint8_t valid_mem[DAB_DECODER_AAC_HANDLE_SIZE];
    DAB_Decoder_Handle h_bad_sr = DAB_Decoder_InitWithMem(valid_mem, sizeof(valid_mem), DAB_CODEC_MP2, 44100U);
    failures += check("InitWithMem rejects unsupported sample rate (44.1 kHz)", h_bad_sr == NULL);

    /* Test 5: Successful initialization in static buffer */
    DAB_Decoder_Handle h_mp2 = DAB_Decoder_InitWithMem(valid_mem, sizeof(valid_mem), DAB_CODEC_MP2, 48000U);
    failures += check("InitWithMem successfully initializes MP2 48kHz instance", h_mp2 != NULL);

    /* Test 6: Setting Soft Mute parameters with valid and invalid bounds */
    int32_t ret_mute_ok = DAB_Decoder_SetMuteTime(h_mp2, 20U, 100U);
    int32_t ret_mute_bad1 = DAB_Decoder_SetMuteTime(h_mp2, 2U, 100U);   /* Attack < 5 */
    int32_t ret_mute_bad2 = DAB_Decoder_SetMuteTime(h_mp2, 20U, 600U);  /* Release > 500 */
    failures += check("SetMuteTime accepts valid timing [20ms, 100ms]", ret_mute_ok == DAB_OK);
    failures += check("SetMuteTime rejects attack_ms < 5ms", ret_mute_bad1 == DAB_ERR_INVALID_PARAM);
    failures += check("SetMuteTime rejects release_ms > 500ms", ret_mute_bad2 == DAB_ERR_INVALID_PARAM);

    /* Test 7: Setting Soft Mute curve profile */
    int32_t ret_curve_ok = DAB_Decoder_SetRampCurve(h_mp2, DAB_RAMP_EXPONENTIAL);
    int32_t ret_curve_bad = DAB_Decoder_SetRampCurve(h_mp2, (DAB_RampCurve)99);
    failures += check("SetRampCurve accepts DAB_RAMP_EXPONENTIAL", ret_curve_ok == DAB_OK);
    failures += check("SetRampCurve rejects invalid curve index", ret_curve_bad == DAB_ERR_INVALID_PARAM);

    /* Test 8: Setting Concealment parameters */
    DAB_ConcealmentConfig cfg;
    cfg.short_loss_thresh = 3U;
    cfg.long_loss_thresh  = 8U;
    cfg.attn_step_q15     = 28000;
    int32_t ret_cfg_ok = DAB_Decoder_SetConcealmentParam(h_mp2, &cfg);
    cfg.short_loss_thresh = 10U;
    cfg.long_loss_thresh  = 5U; /* invalid: short >= long */
    int32_t ret_cfg_bad = DAB_Decoder_SetConcealmentParam(h_mp2, &cfg);
    failures += check("SetConcealmentParam accepts valid thresholds", ret_cfg_ok == DAB_OK);
    failures += check("SetConcealmentParam rejects short >= long threshold", ret_cfg_bad == DAB_ERR_INVALID_PARAM);

    /* Test 9: Setting Quality rise step */
    int32_t ret_rise_ok = DAB_Decoder_SetQualityRiseStep(h_mp2, 4U);
    int32_t ret_rise_bad = DAB_Decoder_SetQualityRiseStep(h_mp2, 15U);
    failures += check("SetQualityRiseStep accepts valid step [4]", ret_rise_ok == DAB_OK);
    failures += check("SetQualityRiseStep rejects step > 10", ret_rise_bad == DAB_ERR_INVALID_PARAM);

    /* Test 10: Reset operation */
    int32_t ret_reset = DAB_Decoder_Reset(h_mp2);
    failures += check("DAB_Decoder_Reset returns DAB_OK", ret_reset == DAB_OK);

    printf("\n=================================================================\n");
    if (failures == 0) {
        printf("  ALL 10 MS-1 TESTS PASSED\n");
    } else {
        printf("  %d MS-1 TEST(S) FAILED\n", failures);
    }
    printf("=================================================================\n\n");
    return failures;
}
