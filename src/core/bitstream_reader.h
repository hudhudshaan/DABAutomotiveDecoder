/**
 * @file    bitstream_reader.h
 * @brief   Safe, Bounds-Checked Bitstream Reader for AU Demuxing & Codec Parsing
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#ifndef BITSTREAM_READER_H
#define BITSTREAM_READER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const uint8_t *p_data;      /**< Pointer to bitstream byte buffer */
    uint32_t       total_bytes; /**< Buffer size in bytes */
    uint32_t       total_bits;  /**< Total bits available */
    uint32_t       bit_offset;  /**< Current bit read position */
} bitstream_reader_t;

void     bitstream_init(bitstream_reader_t *bs, const uint8_t *p_buffer, uint32_t size_bytes);
uint32_t bitstream_get_bits(bitstream_reader_t *bs, uint8_t num_bits);
uint32_t bitstream_peek_bits(const bitstream_reader_t *bs, uint8_t num_bits);
void     bitstream_skip_bits(bitstream_reader_t *bs, uint32_t num_bits);
void     bitstream_byte_align(bitstream_reader_t *bs);
uint32_t bitstream_get_bits_left(const bitstream_reader_t *bs);
uint32_t bitstream_get_bit_offset(const bitstream_reader_t *bs);

#ifdef __cplusplus
}
#endif

#endif /* BITSTREAM_READER_H */
