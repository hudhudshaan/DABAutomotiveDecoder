/**
 * @file    aac_huffman.c
 * @brief   ISO/IEC 14496-3 AAC Normative Huffman Codebook Implementation
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "aac_huffman.h"
#include "aac_tables.h"
#include <stddef.h>

int16_t aac_decode_huffman_scalefactor(bitstream_reader_t *p_bs) {
    if (p_bs == NULL) {
        return 0;
    }

    for (uint32_t i = 0U; i < 121U; i++) {
        uint8_t len = g_aac_sf_bits[i];
        if (len > 0U) {
            uint32_t code = bitstream_peek_bits(p_bs, len);
            if (code == g_aac_sf_codes[i]) {
                bitstream_skip_bits(p_bs, (uint32_t)len);
                return (int16_t)((int32_t)i - 60);
            }
        }
    }

    /* Fallback on corruption: skip 1 bit and return 0 diff */
    bitstream_skip_bits(p_bs, 1U);
    return 0;
}

static int16_t aac_decode_escape_val(bitstream_reader_t *p_bs) {
    uint8_t count = 0U;
    /* Bounded loop: maximum escape prefix length according to ISO 14496-3 is < 16 */
    while ((count < 16U) && (bitstream_get_bits(p_bs, 1U) == 1U)) {
        count++;
    }
    uint8_t n = (uint8_t)(count + 4U);
    uint32_t val = bitstream_get_bits(p_bs, n);
    return (int16_t)((1U << n) + val);
}

uint8_t aac_decode_huffman_spectral(bitstream_reader_t *p_bs, uint8_t codebook_idx, int16_t *p_quad_or_pair) {
    if ((p_bs == NULL) || (p_quad_or_pair == NULL)) {
        return 0U;
    }

    if (codebook_idx == AAC_CB_ZERO) {
        p_quad_or_pair[0] = 0;
        p_quad_or_pair[1] = 0;
        p_quad_or_pair[2] = 0;
        p_quad_or_pair[3] = 0;
        return 4U;
    }

    if ((codebook_idx > 11U) || (codebook_idx == 0U)) {
        /* Non-spectral codebooks (PNS, IS) do not carry spectral data */
        p_quad_or_pair[0] = 0;
        p_quad_or_pair[1] = 0;
        return 2U;
    }

    const uint16_t *codes = g_aac_cb_codes_ptrs[codebook_idx];
    const uint8_t  *bits  = g_aac_cb_bits_ptrs[codebook_idx];
    uint16_t size         = g_aac_cb_sizes[codebook_idx];
    uint8_t dim           = g_aac_cb_dims[codebook_idx];

    int32_t matched_idx = -1;
    for (uint16_t i = 0U; i < size; i++) {
        uint8_t len = bits[i];
        if (len > 0U) {
            uint32_t code = bitstream_peek_bits(p_bs, len);
            if (code == (uint32_t)codes[i]) {
                bitstream_skip_bits(p_bs, (uint32_t)len);
                matched_idx = (int32_t)i;
                break;
            }
        }
    }

    if (matched_idx < 0) {
        /* Desync fallback */
        bitstream_skip_bits(p_bs, 1U);
        for (uint8_t d = 0U; d < dim; d++) {
            p_quad_or_pair[d] = 0;
        }
        return dim;
    }

    if ((codebook_idx == 1U) || (codebook_idx == 2U)) {
        /* Signed 4-quad, max magnitude 1 */
        int32_t idx = matched_idx;
        p_quad_or_pair[0] = (int16_t)(idx / 27 - 1);
        p_quad_or_pair[1] = (int16_t)((idx / 9) % 3 - 1);
        p_quad_or_pair[2] = (int16_t)((idx / 3) % 3 - 1);
        p_quad_or_pair[3] = (int16_t)(idx % 3 - 1);
        return 4U;
    }

    if ((codebook_idx == 3U) || (codebook_idx == 4U)) {
        /* Unsigned 4-quad, max magnitude 2, with trailing sign bits */
        int32_t idx = matched_idx;
        int16_t w = (int16_t)(idx / 27);
        int16_t x = (int16_t)((idx / 9) % 3);
        int16_t y = (int16_t)((idx / 3) % 3);
        int16_t z = (int16_t)(idx % 3);

        if ((w != 0) && (bitstream_get_bits(p_bs, 1U) != 0U)) { w = (int16_t)-w; }
        if ((x != 0) && (bitstream_get_bits(p_bs, 1U) != 0U)) { x = (int16_t)-x; }
        if ((y != 0) && (bitstream_get_bits(p_bs, 1U) != 0U)) { y = (int16_t)-y; }
        if ((z != 0) && (bitstream_get_bits(p_bs, 1U) != 0U)) { z = (int16_t)-z; }

        p_quad_or_pair[0] = w;
        p_quad_or_pair[1] = x;
        p_quad_or_pair[2] = y;
        p_quad_or_pair[3] = z;
        return 4U;
    }

    if ((codebook_idx == 5U) || (codebook_idx == 6U)) {
        /* Signed 2-pair, max magnitude 4 */
        int32_t idx = matched_idx;
        p_quad_or_pair[0] = (int16_t)(idx / 9 - 4);
        p_quad_or_pair[1] = (int16_t)(idx % 9 - 4);
        return 2U;
    }

    if ((codebook_idx == 7U) || (codebook_idx == 8U)) {
        /* Unsigned 2-pair, max magnitude 7, with trailing sign bits */
        int32_t idx = matched_idx;
        int16_t x = (int16_t)(idx / 8);
        int16_t y = (int16_t)(idx % 8);

        if ((x != 0) && (bitstream_get_bits(p_bs, 1U) != 0U)) { x = (int16_t)-x; }
        if ((y != 0) && (bitstream_get_bits(p_bs, 1U) != 0U)) { y = (int16_t)-y; }

        p_quad_or_pair[0] = x;
        p_quad_or_pair[1] = y;
        return 2U;
    }

    if ((codebook_idx == 9U) || (codebook_idx == 10U)) {
        /* Unsigned 2-pair, max magnitude 12, with trailing sign bits */
        int32_t idx = matched_idx;
        int16_t x = (int16_t)(idx / 13);
        int16_t y = (int16_t)(idx % 13);

        if ((x != 0) && (bitstream_get_bits(p_bs, 1U) != 0U)) { x = (int16_t)-x; }
        if ((y != 0) && (bitstream_get_bits(p_bs, 1U) != 0U)) { y = (int16_t)-y; }

        p_quad_or_pair[0] = x;
        p_quad_or_pair[1] = y;
        return 2U;
    }

    if (codebook_idx == 11U) {
        /* Unsigned 2-pair, max magnitude 16+, with sign bits then escape sequence */
        int32_t idx = matched_idx;
        int16_t x = (int16_t)(idx / 17);
        int16_t y = (int16_t)(idx % 17);

        uint8_t sign_x = (x != 0) ? (uint8_t)bitstream_get_bits(p_bs, 1U) : 0U;
        uint8_t sign_y = (y != 0) ? (uint8_t)bitstream_get_bits(p_bs, 1U) : 0U;

        if (x == 16) {
            x = aac_decode_escape_val(p_bs);
        }
        if (y == 16) {
            y = aac_decode_escape_val(p_bs);
        }

        p_quad_or_pair[0] = (sign_x != 0U) ? (int16_t)-x : x;
        p_quad_or_pair[1] = (sign_y != 0U) ? (int16_t)-y : y;
        return 2U;
    }

    return 0U;
}
