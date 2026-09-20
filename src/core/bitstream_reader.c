/**
 * @file    bitstream_reader.c
 * @brief   Safe, Bounds-Checked Bitstream Reader Implementation
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "bitstream_reader.h"

void bitstream_init(bitstream_reader_t *bs, const uint8_t *p_buffer, uint32_t size_bytes) {
    if (bs == NULL) {
        return;
    }
    bs->p_data = p_buffer;
    bs->total_bytes = size_bytes;
    bs->total_bits = size_bytes * 8U;
    bs->bit_offset = 0U;
}

uint32_t bitstream_peek_bits(const bitstream_reader_t *bs, uint8_t num_bits) {
    if ((bs == NULL) || (bs->p_data == NULL) || (num_bits == 0U) || (num_bits > 32U)) {
        return 0U;
    }

    uint32_t val = 0U;
    uint32_t current_bit = bs->bit_offset;

    for (uint8_t i = 0U; i < num_bits; i++) {
        if (current_bit >= bs->total_bits) {
            val <<= (num_bits - i);
            break;
        }
        uint32_t byte_idx = current_bit >> 3U;
        uint32_t bit_idx  = 7U - (current_bit & 7U);
        uint32_t bit_val  = ((uint32_t)bs->p_data[byte_idx] >> bit_idx) & 1U;
        val = (val << 1U) | bit_val;
        current_bit++;
    }

    return val;
}

uint32_t bitstream_get_bits(bitstream_reader_t *bs, uint8_t num_bits) {
    if ((bs == NULL) || (num_bits == 0U) || (num_bits > 32U)) {
        return 0U;
    }

    uint32_t val = bitstream_peek_bits(bs, num_bits);
    bs->bit_offset += (uint32_t)num_bits;
    if (bs->bit_offset > bs->total_bits) {
        bs->bit_offset = bs->total_bits;
    }

    return val;
}

void bitstream_skip_bits(bitstream_reader_t *bs, uint32_t num_bits) {
    if (bs == NULL) {
        return;
    }
    bs->bit_offset += num_bits;
    if (bs->bit_offset > bs->total_bits) {
        bs->bit_offset = bs->total_bits;
    }
}

void bitstream_byte_align(bitstream_reader_t *bs) {
    if (bs == NULL) {
        return;
    }
    uint32_t remainder = bs->bit_offset & 7U;
    if (remainder != 0U) {
        bs->bit_offset += (8U - remainder);
        if (bs->bit_offset > bs->total_bits) {
            bs->bit_offset = bs->total_bits;
        }
    }
}

uint32_t bitstream_get_bits_left(const bitstream_reader_t *bs) {
    if (bs == NULL) {
        return 0U;
    }
    if (bs->bit_offset >= bs->total_bits) {
        return 0U;
    }
    return bs->total_bits - bs->bit_offset;
}

uint32_t bitstream_get_bit_offset(const bitstream_reader_t *bs) {
    if (bs == NULL) {
        return 0U;
    }
    return bs->bit_offset;
}
