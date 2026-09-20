/**
 * @file    dab_crc.c
 * @brief   ETSI EN 300 401 §12.2 CRC-16/CCITT Engine Implementation
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "dab_crc.h"
#include <stddef.h>

uint16_t dab_crc16_compute(const uint8_t *data, uint16_t len) {
    if ((data == NULL) || (len == 0U)) {
        return 0U;
    }

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

DAB_AUStatus dab_au_evaluate_crc(
    const uint8_t *au_data,
    uint16_t au_len,
    const DAB_SignalStatus *sig_status,
    uint32_t *p_total_cnt,
    uint32_t *p_crc_err_cnt
) {
    if (p_total_cnt != NULL) {
        (*p_total_cnt)++;
    }

    /* Check if baseband explicitly signaled CRC status */
    if (sig_status != NULL) {
        if (sig_status->au_crc_pass == 0U) {
            if (p_crc_err_cnt != NULL) {
                (*p_crc_err_cnt)++;
            }
            return DAB_AU_CRC_ERR;
        } else {
            return DAB_AU_GOOD;
        }
    }

    /* Check for NULL or too short AU */
    if ((au_data == NULL) || (au_len < DAB_AU_MIN_LEN_BYTES)) {
        if (p_crc_err_cnt != NULL) {
            (*p_crc_err_cnt)++;
        }
        return DAB_AU_LOST;
    }

    /* Compute CRC over AU payload (length - 2) */
    uint16_t payload_len = (uint16_t)(au_len - DAB_AU_CRC_BYTES);
    uint16_t crc_calc = dab_crc16_compute(au_data, payload_len);

    /* Extract 16-bit CRC big endian */
    uint16_t crc_exp = (uint16_t)(((uint16_t)au_data[payload_len] << 8U) | (uint16_t)au_data[payload_len + 1U]);

    /* Support both non-inverted (ETSI EN 300 401) and inverted (ETSI TS 102 563) CRC */
    if ((crc_calc != crc_exp) && ((uint16_t)(crc_calc ^ 0xFFFFU) != crc_exp)) {
        if (p_crc_err_cnt != NULL) {
            (*p_crc_err_cnt)++;
        }
        return DAB_AU_CRC_ERR;
    }

    return DAB_AU_GOOD;
}
