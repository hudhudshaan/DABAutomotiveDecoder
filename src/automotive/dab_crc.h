/**
 * @file    dab_crc.h
 * @brief   ETSI EN 300 401 §12.2 CRC-16/CCITT Engine for Audio Unit Verification
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef DAB_CRC_H
#define DAB_CRC_H

#include <stdint.h>
#include "dab_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

uint16_t dab_crc16_compute(const uint8_t *data, uint16_t len);

DAB_AUStatus dab_au_evaluate_crc(
    const uint8_t *au_data,
    uint16_t au_len,
    const DAB_SignalStatus *sig_status,
    uint32_t *p_total_cnt,
    uint32_t *p_crc_err_cnt
);

#ifdef __cplusplus
}
#endif

#endif /* DAB_CRC_H */
