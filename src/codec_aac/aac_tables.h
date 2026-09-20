/**
 * @file    aac_tables.h
 * @brief   ISO/IEC 14496-3 & ETSI TS 102 563 Normative AAC Lookup Tables Header
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef AAC_TABLES_H
#define AAC_TABLES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Huffman Entry representation */
typedef struct {
    uint8_t  len;   /**< Codeword length in bits */
    uint16_t code;  /**< Codeword bits */
} aac_huff_code_t;

/* Scalefactor Huffman Table (121 entries, DPCM diffs -60 .. +60) */
extern const uint32_t g_aac_sf_codes[121];
extern const uint8_t  g_aac_sf_bits[121];

/* Normative Codebooks 1 through 11 */
extern const uint16_t g_aac_cb1_codes[81];
extern const uint8_t  g_aac_cb1_bits[81];

extern const uint16_t g_aac_cb2_codes[81];
extern const uint8_t  g_aac_cb2_bits[81];

extern const uint16_t g_aac_cb3_codes[81];
extern const uint8_t  g_aac_cb3_bits[81];

extern const uint16_t g_aac_cb4_codes[81];
extern const uint8_t  g_aac_cb4_bits[81];

extern const uint16_t g_aac_cb5_codes[81];
extern const uint8_t  g_aac_cb5_bits[81];

extern const uint16_t g_aac_cb6_codes[81];
extern const uint8_t  g_aac_cb6_bits[81];

extern const uint16_t g_aac_cb7_codes[64];
extern const uint8_t  g_aac_cb7_bits[64];

extern const uint16_t g_aac_cb8_codes[64];
extern const uint8_t  g_aac_cb8_bits[64];

extern const uint16_t g_aac_cb9_codes[169];
extern const uint8_t  g_aac_cb9_bits[169];

extern const uint16_t g_aac_cb10_codes[169];
extern const uint8_t  g_aac_cb10_bits[169];

extern const uint16_t g_aac_cb11_codes[289];
extern const uint8_t  g_aac_cb11_bits[289];

/* Codebook table pointer lookup */
extern const uint16_t * const g_aac_cb_codes_ptrs[12];
extern const uint8_t  * const g_aac_cb_bits_ptrs[12];
extern const uint16_t g_aac_cb_sizes[12];
extern const uint8_t  g_aac_cb_dims[12];

/* Scale Factor Band (SWB) Offsets for 960 transform */
extern const uint16_t g_aac_sfb_48_960[50];
extern const uint16_t g_aac_sfb_32_960[52];
extern const uint16_t g_aac_sfb_24_960[47];
extern const uint16_t g_aac_sfb_16_960[43];

/* Scale Factor Band (SWB) Offsets for 120 transform (8-short) */
extern const uint16_t g_aac_sfb_48_120[15];
extern const uint16_t g_aac_sfb_24_120[16];
extern const uint16_t g_aac_sfb_16_120[16];

/* Sine Window Tables */
extern const float g_aac_window_sine_960[1920];
extern const float g_aac_window_sine_120[240];

/* SBR 640-coefficient Polyphase Prototype Filter Table */
extern const float g_sbr_qmf_c640[640];

#ifdef __cplusplus
}
#endif

#endif /* AAC_TABLES_H */
