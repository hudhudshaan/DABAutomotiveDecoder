/**
 * @file    automotive_decoder_runner.c
 * @brief   Automotive DAB/DAB+ Stream Runner (C99 Pointer-Arithmetic Form)
 */

#include "dab_decoder.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define LIBAUDIO_MAX_PAYLOAD_SIZE 4096U
#define LIBAUDIO_WAV_HEADER_SIZE   44U
#define LIBAUDIO_HEADER_TOKEN_SIZE  4U

static uint64_t g_decoder_mem[(DAB_DECODER_AAC_HANDLE_SIZE + 7U) / 8U];
static int16_t  g_pcm_output[DAB_MAX_PCM_SAMPLES_PER_AU * 2U];
static uint8_t  g_bitstream_payload[LIBAUDIO_MAX_PAYLOAD_SIZE];
static uint8_t  g_header_buffer[LIBAUDIO_HEADER_TOKEN_SIZE];

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: ./automotive_decoder_runner <input_file.au> <output_file.wav>\n");
        return 1;
    }

    const char *input_path  = *(argv + 1);
    const char *output_path = *(argv + 2);

    FILE *in_file = fopen(input_path, "rb");
    if (!in_file) {
        printf("Fatal: Unable to open ODR stream file: %s\n", input_path);
        return 1;
    }

    /* Inspect file size */
    fseek(in_file, 0, SEEK_END);
    long file_size = ftell(in_file);
    fseek(in_file, 0, SEEK_SET);

    FILE *out_file = fopen(output_path, "wb");
    if (!out_file) {
        printf("Fatal: Unable to create target WAV file: %s\n", output_path);
        fclose(in_file);
        return 1;
    }

    fseek(out_file, (long)LIBAUDIO_WAV_HEADER_SIZE, SEEK_SET);

    /* Peek at first 6 bytes to identify stream container format */
    uint8_t peek[6];
    size_t peek_read = fread(peek, 1U, 6U, in_file);
    fseek(in_file, 0, SEEK_SET);

    int is_ts102563_superframe = 0;
    uint32_t sample_rt = 48000U;
    uint8_t sbr_active_flag = 1U;

    if (peek_read == 6U) {
        /* Format A: 4-byte big-endian AU length prefix (< 2048 bytes per AU) */
        if ((peek[0] == 0U) && (peek[1] == 0U) && (peek[2] < 8U)) {
            is_ts102563_superframe = 0;
        } else {
            /* Format B: ETSI TS 102 563 DAB+ Superframe stream */
            is_ts102563_superframe = 1;
            uint8_t cfg = peek[2];
            int dac_rate = (cfg >> 6) & 1;
            int sbr_flag = (cfg >> 5) & 1;
            sample_rt = (dac_rate == 1) ? 48000U : 32000U;
            sbr_active_flag = (uint8_t)sbr_flag;
        }
    }

    DAB_Decoder_Handle dec_handle = DAB_Decoder_InitWithMem(
        (void *)g_decoder_mem, 
        sizeof(g_decoder_mem), 
        DAB_CODEC_AAC, 
        sample_rt
    );

    if (!dec_handle) {
        printf("Fatal: Entrance initialization failed.\n");
        fclose(in_file);
        fclose(out_file);
        return 1;
    }
    DAB_Decoder_SetSbrActive(dec_handle, sbr_active_flag);

    printf("--> Decoder framework initialization success (Output rate: %u Hz, SBR: %u).\n",
           sample_rt, sbr_active_flag);

    uint32_t total_samples = 0U;
    uint32_t frame_count   = 0U;

    if (is_ts102563_superframe) {
        /* Determine superframe size based on standard DAB+ bitrates */
        uint32_t sf_size = 1200U; /* Default: 80 kbps */
        const uint32_t candidate_sizes[] = {480U, 720U, 960U, 1200U, 1440U, 1920U};
        for (size_t c = 0U; c < sizeof(candidate_sizes)/sizeof(candidate_sizes[0]); c++) {
            uint32_t c_sz = candidate_sizes[c];
            if ((file_size > 0) && ((uint32_t)file_size % c_sz == 0U)) {
                if ((size_t)file_size >= (c_sz + 3U)) {
                    fseek(in_file, (long)(c_sz + 2U), SEEK_SET);
                    uint8_t next_cfg = 0U;
                    if (fread(&next_cfg, 1U, 1U, in_file) == 1U) {
                        if (next_cfg == peek[2]) {
                            sf_size = c_sz;
                            fseek(in_file, 0, SEEK_SET);
                            break;
                        }
                    }
                    fseek(in_file, 0, SEEK_SET);
                } else {
                    sf_size = c_sz;
                    break;
                }
            }
        }

        uint32_t rs_parity = (sf_size / 120U) * 10U;
        uint32_t audio_data_end = sf_size - rs_parity;

        printf("--> Detected ETSI TS 102 563 DAB+ Superframe stream (superframe size: %u bytes, audio: %u bytes).\n",
               sf_size, audio_data_end);
        printf("--> Processing stream chunks...\n");

        static uint8_t sf_buf[2048];
        while (fread(sf_buf, 1U, sf_size, in_file) == sf_size) {
            uint8_t cfg = sf_buf[2];
            int dac_rate = (cfg >> 6) & 1;
            int sbr_flag = (cfg >> 5) & 1;
            int num_aus = (sbr_flag == 1) ? ((dac_rate == 1) ? 3 : 2) : ((dac_rate == 1) ? 6 : 4);

            const uint8_t *au_ptrs[6];
            uint16_t au_lens[6];

            if (num_aus == 2) {
                /* Core 16 kHz + SBR (32 kHz output): 2 AUs (60ms each) */
                uint16_t au1 = ((uint16_t)sf_buf[3] << 4U) | ((uint16_t)(sf_buf[4] >> 4U) & 0x0FU);
                if ((au1 > 5U) && (au1 < audio_data_end)) {
                    au_ptrs[0] = &sf_buf[5];
                    au_lens[0] = au1 - 5U;
                    au_ptrs[1] = &sf_buf[au1];
                    au_lens[1] = (uint16_t)(audio_data_end - au1);
                } else {
                    num_aus = 0;
                }
            } else if (num_aus == 3) {
                /* Core 24 kHz + SBR (48 kHz output): 3 AUs (40ms each) */
                uint16_t au1 = ((uint16_t)sf_buf[3] << 4U) | ((uint16_t)(sf_buf[4] >> 4U) & 0x0FU);
                uint16_t au2 = (((uint16_t)sf_buf[4] & 0x0FU) << 8U) | (uint16_t)sf_buf[5];

                if ((au1 > 6U) && (au2 > au1) && (au2 < audio_data_end)) {
                    au_ptrs[0] = &sf_buf[6];
                    au_lens[0] = au1 - 6U;
                    au_ptrs[1] = &sf_buf[au1];
                    au_lens[1] = au2 - au1;
                    au_ptrs[2] = &sf_buf[au2];
                    au_lens[2] = (uint16_t)(audio_data_end - au2);
                } else {
                    num_aus = 0;
                }
            } else if (num_aus == 4) {
                /* Core 32 kHz standalone AAC-LC (32 kHz output): 4 AUs (30ms each) */
                uint16_t au1 = ((uint16_t)sf_buf[3] << 4U) | ((uint16_t)(sf_buf[4] >> 4U) & 0x0FU);
                uint16_t au2 = (((uint16_t)sf_buf[4] & 0x0FU) << 8U) | (uint16_t)sf_buf[5];
                uint16_t au3 = ((uint16_t)sf_buf[6] << 4U) | ((uint16_t)(sf_buf[7] >> 4U) & 0x0FU);

                if ((au1 > 8U) && (au2 > au1) && (au3 > au2) && (au3 < audio_data_end)) {
                    au_ptrs[0] = &sf_buf[8];
                    au_lens[0] = au1 - 8U;
                    au_ptrs[1] = &sf_buf[au1];
                    au_lens[1] = au2 - au1;
                    au_ptrs[2] = &sf_buf[au2];
                    au_lens[2] = au3 - au2;
                    au_ptrs[3] = &sf_buf[au3];
                    au_lens[3] = (uint16_t)(audio_data_end - au3);
                } else {
                    num_aus = 0;
                }
            } else if (num_aus == 6) {
                /* Core 48 kHz standalone AAC-LC (48 kHz output): 6 AUs (20ms each) */
                uint16_t au1 = ((uint16_t)sf_buf[3] << 4U) | ((uint16_t)(sf_buf[4] >> 4U) & 0x0FU);
                uint16_t au2 = (((uint16_t)sf_buf[4] & 0x0FU) << 8U) | (uint16_t)sf_buf[5];
                uint16_t au3 = ((uint16_t)sf_buf[6] << 4U) | ((uint16_t)(sf_buf[7] >> 4U) & 0x0FU);
                uint16_t au4 = (((uint16_t)sf_buf[7] & 0x0FU) << 8U) | (uint16_t)sf_buf[8];
                uint16_t au5 = ((uint16_t)sf_buf[9] << 4U) | ((uint16_t)(sf_buf[10] >> 4U) & 0x0FU);

                if ((au1 > 11U) && (au2 > au1) && (au3 > au2) && (au4 > au3) && (au5 > au4) && (au5 < audio_data_end)) {
                    au_ptrs[0] = &sf_buf[11];
                    au_lens[0] = au1 - 11U;
                    au_ptrs[1] = &sf_buf[au1];
                    au_lens[1] = au2 - au1;
                    au_ptrs[2] = &sf_buf[au2];
                    au_lens[2] = au3 - au2;
                    au_ptrs[3] = &sf_buf[au3];
                    au_lens[3] = au4 - au3;
                    au_ptrs[4] = &sf_buf[au4];
                    au_lens[4] = au5 - au4;
                    au_ptrs[5] = &sf_buf[au5];
                    au_lens[5] = (uint16_t)(audio_data_end - au5);
                } else {
                    num_aus = 0;
                }
            } else {
                num_aus = 0;
            }

            for (int a = 0; a < num_aus; a++) {
                DAB_AudioStatus runtime_status;
                memset(&runtime_status, 0, sizeof(DAB_AudioStatus));

                DAB_SignalStatus sig_status;
                memset(&sig_status, 0, sizeof(DAB_SignalStatus));
                sig_status.au_crc_pass = 1U;

                int32_t metrics = DAB_Decoder_DecodeAU(
                    dec_handle,
                    au_ptrs[a],
                    au_lens[a],
                    &sig_status,
                    g_pcm_output,
                    &runtime_status
                );

                if (metrics > 0) {
                    uint32_t write_bytes = (uint32_t)metrics * 2U * (uint32_t)sizeof(int16_t);
                    fwrite(g_pcm_output, 1U, write_bytes, out_file);
                    total_samples += (uint32_t)metrics;
                }
                frame_count++;
            }
        }
    } else {
        printf("--> Detected length-prefixed AU stream.\n");
        printf("--> Processing stream chunks...\n");

        while (feof(in_file) == 0) {
            if (fread(g_header_buffer, 1U, LIBAUDIO_HEADER_TOKEN_SIZE, in_file) != LIBAUDIO_HEADER_TOKEN_SIZE) {
                break; 
            }

            uint32_t current_au_length = ((uint32_t)(*(g_header_buffer + 0)) << 24U) |
                                         ((uint32_t)(*(g_header_buffer + 1)) << 16U) |
                                         ((uint32_t)(*(g_header_buffer + 2)) << 8U)  |
                                         ((uint32_t)(*(g_header_buffer + 3)));

            if ((current_au_length == 0U) || (current_au_length > LIBAUDIO_MAX_PAYLOAD_SIZE)) {
                printf("Warning: Invalid or oversized AU frame detected at packet %u. Skipping.\n", frame_count);
                break;
            }

            if (fread(g_bitstream_payload, 1U, current_au_length, in_file) != current_au_length) {
                break;
            }

            DAB_AudioStatus runtime_status;
            memset(&runtime_status, 0, sizeof(DAB_AudioStatus));
            
            int32_t metrics = DAB_Decoder_DecodeAU(
                dec_handle, 
                g_bitstream_payload, 
                (uint16_t)current_au_length, 
                NULL, 
                g_pcm_output, 
                &runtime_status
            );

            if (metrics > 0) {
                uint32_t write_bytes = (uint32_t)metrics * 2U * (uint32_t)sizeof(int16_t);
                fwrite(g_pcm_output, 1U, write_bytes, out_file);
                total_samples += (uint32_t)metrics;
            }

            frame_count++;
        }
    }

    uint32_t raw_pcm_bytes  = total_samples * 2U * (uint32_t)sizeof(int16_t);
    uint32_t riff_chunk_sz  = raw_pcm_bytes + 36U;
    uint32_t byte_rate_calc = sample_rt * 2U * 2U;
    uint32_t sub_chunk_sz   = 16U;
    uint16_t format_tag     = 1U; 
    uint16_t channel_cnt    = 2U;
    uint16_t block_align    = 4U;
    uint16_t bits_per_smpl  = 16U;

    fseek(out_file, 0, SEEK_SET);
    fwrite("RIFF", 1U, 4U, out_file);
    fwrite(&riff_chunk_sz, 4U, 1U, out_file);
    fwrite("WAVEfmt ", 1U, 8U, out_file);
    fwrite(&sub_chunk_sz, 4U, 1U, out_file);
    fwrite(&format_tag, 2U, 1U, out_file);
    fwrite(&channel_cnt, 2U, 1U, out_file);
    fwrite(&sample_rt, 4U, 1U, out_file);
    fwrite(&byte_rate_calc, 4U, 1U, out_file);
    fwrite(&block_align, 2U, 1U, out_file);
    fwrite(&bits_per_smpl, 2U, 1U, out_file);
    fwrite("data", 1U, 4U, out_file);
    fwrite(&raw_pcm_bytes, 4U, 1U, out_file);

    fclose(in_file);
    fclose(out_file);

    printf("--> Pipeline stream processing completed.\n");
    printf("--> Successfully parsed %u frames. Decoded %u audio samples.\n", frame_count, total_samples);

    return 0;
}
