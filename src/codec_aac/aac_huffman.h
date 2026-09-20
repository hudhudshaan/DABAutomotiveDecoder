/**
 * @file    aac_huffman.h
 * @brief   ISO/IEC 14496-3 AAC Full Binary Huffman Codebook Package Header
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef AAC_HUFFMAN_H
#define AAC_HUFFMAN_H

#include "../core/bitstream_reader.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AAC_CB_ZERO                 (0U)
#define AAC_CB_1                    (1U)
#define AAC_CB_2                    (2U)
#define AAC_CB_3                    (3U)
#define AAC_CB_4                    (4U)
#define AAC_CB_5                    (5U)
#define AAC_CB_6                    (6U)
#define AAC_CB_7                    (7U)
#define AAC_CB_8                    (8U)
#define AAC_CB_9                    (9U)
#define AAC_CB_10                   (10U)
#define AAC_CB_ESC                  (11U)
#define AAC_CB_RESERVED             (12U)
#define AAC_CB_NOISE                (13U)
#define AAC_CB_INTENSITY_STEREO2    (14U)
#define AAC_CB_INTENSITY_STEREO     (15U)

int16_t aac_decode_huffman_scalefactor(bitstream_reader_t *p_bs);
uint8_t aac_decode_huffman_spectral(bitstream_reader_t *p_bs, uint8_t codebook_idx, int16_t *p_quad_or_pair);

#ifdef __cplusplus
}
#endif

#endif /* AAC_HUFFMAN_H */
