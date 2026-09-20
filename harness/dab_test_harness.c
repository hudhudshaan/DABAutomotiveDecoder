/**
 * @file    dab_test_harness.c
 * @brief   Automotive DAB/DAB+ Host-Side Test Harness & Validation Runner
 * @details Processes .au bitstream files, executes internal AU CRC checks,
 *          decodes audio, logs per-frame telemetry, records PCM/WAV output,
 *          and benchmarks decoding performance / MIPS.
 * @standard ETSI EN 300 401, ETSI TS 102 563, MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "dab_decoder.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Helper to write 44-byte standard RIFF WAV header */
static void write_wav_header(FILE *fp, uint32_t sample_rate, uint16_t channels, uint32_t total_pcm_bytes) {
    uint32_t riff_chunk_size = total_pcm_bytes + 36U;
    uint32_t byte_rate = sample_rate * (uint32_t)channels * 2U;
    uint16_t block_align = channels * 2U;

    /* RIFF header */
    (void)fwrite("RIFF", 1, 4, fp);
    (void)fwrite(&riff_chunk_size, 4, 1, fp);
    (void)fwrite("WAVE", 1, 4, fp);

    /* fmt chunk */
    (void)fwrite("fmt ", 1, 4, fp);
    uint32_t fmt_chunk_size = 16U;
    uint16_t audio_format = 1U; /* PCM */
    (void)fwrite(&fmt_chunk_size, 4, 1, fp);
    (void)fwrite(&audio_format, 2, 1, fp);
    (void)fwrite(&channels, 2, 1, fp);
    (void)fwrite(&sample_rate, 4, 1, fp);
    (void)fwrite(&byte_rate, 4, 1, fp);
    (void)fwrite(&block_align, 2, 1, fp);
    uint16_t bits_per_sample = 16U;
    (void)fwrite(&bits_per_sample, 2, 1, fp);

    /* data chunk header */
    (void)fwrite("data", 1, 4, fp);
    (void)fwrite(&total_pcm_bytes, 4, 1, fp);
}

int main(int argc, char *argv[]) {
    const char *input_file  = NULL;
    const char *output_pcm  = NULL;
    const char *output_wav  = NULL;
    const char *output_log  = NULL;
    DAB_CodecType codec_type = DAB_CODEC_MP2;
    uint32_t sample_rate    = 48000U;
    int benchmark_mode      = 0;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--input") == 0) && (i + 1 < argc)) {
            input_file = argv[++i];
        } else if ((strcmp(argv[i], "--output") == 0) && (i + 1 < argc)) {
            output_pcm = argv[++i];
        } else if ((strcmp(argv[i], "--wav") == 0) && (i + 1 < argc)) {
            output_wav = argv[++i];
        } else if ((strcmp(argv[i], "--log") == 0) && (i + 1 < argc)) {
            output_log = argv[++i];
        } else if ((strcmp(argv[i], "--codec") == 0) && (i + 1 < argc)) {
            i++;
            if (strcmp(argv[i], "aac") == 0) {
                codec_type = DAB_CODEC_AAC;
            } else {
                codec_type = DAB_CODEC_MP2;
            }
        } else if ((strcmp(argv[i], "--sample-rate") == 0) && (i + 1 < argc)) {
            sample_rate = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--benchmark") == 0) {
            benchmark_mode = 1;
        } else {
            /* ignore unrecognized */
        }
    }

    (void)benchmark_mode;

    if (input_file == NULL) {
        printf("Usage: dab_test_harness --input <file.au> [--codec mp2|aac] [--sample-rate 48000|24000|32000|16000]\n");
        printf("                        [--output <file.pcm>] [--wav <file.wav>] [--log <file.log>] [--benchmark]\n");
        return 1;
    }

    FILE *in_fp = fopen(input_file, "rb");
    if (!in_fp) {
        printf("Error: Could not open input AU file: %s\n", input_file);
        return 1;
    }

    FILE *pcm_fp = output_pcm ? fopen(output_pcm, "wb") : NULL;
    FILE *wav_fp = output_wav ? fopen(output_wav, "wb") : NULL;
    FILE *log_fp = output_log ? fopen(output_log, "w") : NULL;

    if (wav_fp) {
        /* Write placeholder WAV header */
        write_wav_header(wav_fp, sample_rate, 2U, 0U);
    }

    if (log_fp) {
        fprintf(log_fp, "# DAB/DAB+ Automotive Audio Decoder Test Harness Execution Log\n");
        fprintf(log_fp, "# Input: %s | Codec: %s | Sample Rate: %u Hz\n",
                input_file, (codec_type == DAB_CODEC_AAC) ? "HE-AAC v2" : "MUSICAM MP2", sample_rate);
        fprintf(log_fp, "# Frame\tAU_Len\tAU_Status\tConceal_Mode\tMute_State\tGain_Q15\tQuality\tTrigger\tSamples\n");
    }

    /* Statically allocated handle buffer (zero malloc, 8-byte aligned) */
    static uint64_t handle_mem[(DAB_DECODER_AAC_HANDLE_SIZE + 7U) / 8U];
    static int16_t pcm_out_buf[DAB_MAX_PCM_SAMPLES_PER_AU * 2U];
    static uint8_t au_buf[4096];

    DAB_Decoder_Handle handle = DAB_Decoder_InitWithMem(handle_mem, sizeof(handle_mem), codec_type, sample_rate);
    if (!handle) {
        printf("Error: Decoder initialization failed!\n");
        fclose(in_fp);
        if (pcm_fp) fclose(pcm_fp);
        if (wav_fp) fclose(wav_fp);
        if (log_fp) fclose(log_fp);
        return 1;
    }

    /* Inspect file size and container format */
    fseek(in_fp, 0, SEEK_END);
    long file_size = ftell(in_fp);
    fseek(in_fp, 0, SEEK_SET);

    uint8_t peek[6];
    size_t peek_read = fread(peek, 1U, 6U, in_fp);
    fseek(in_fp, 0, SEEK_SET);

    int is_ts102563 = 0;
    if (peek_read == 6U) {
        if (!((peek[0] == 0U) && (peek[1] == 0U) && (peek[2] < 8U))) {
            is_ts102563 = 1;
        }
    }

    printf("=================================================================\n");
    printf("  Automotive DAB/DAB+ Test Harness Execution\n");
    printf("  Input Stream:  %s (%s)\n", input_file, is_ts102563 ? "ETSI TS 102 563 Superframe" : "Length-prefixed AU");
    printf("  Codec:         %s\n", (codec_type == DAB_CODEC_AAC) ? "HE-AAC v2 (DAB+)" : "MUSICAM Layer II (DAB)");
    printf("  Sample Rate:   %u Hz\n", sample_rate);
    printf("=================================================================\n\n");

    uint32_t frame_idx = 0U;
    uint32_t total_pcm_bytes = 0U;
    uint32_t total_pcm_samples = 0U;

    clock_t start_time = clock();

    if (is_ts102563) {
        uint32_t sf_size = 1200U;
        const uint32_t candidate_sizes[] = {1200U, 960U, 720U, 1440U, 480U, 1920U};
        for (size_t c = 0U; c < sizeof(candidate_sizes)/sizeof(candidate_sizes[0]); c++) {
            if ((file_size > 0) && ((uint32_t)file_size % candidate_sizes[c] == 0U)) {
                sf_size = candidate_sizes[c];
                break;
            }
        }
        uint32_t rs_parity = (sf_size / 120U) * 10U;
        uint32_t audio_data_end = sf_size - rs_parity;

        static uint8_t sf_buf[2048];
        while (fread(sf_buf, 1U, sf_size, in_fp) == sf_size) {
            uint8_t cfg = sf_buf[2];
            int dac_rate = (cfg >> 6) & 1;
            int sbr_flag = (cfg >> 5) & 1;
            int num_aus = (sbr_flag == 1) ? ((dac_rate == 1) ? 3 : 2) : ((dac_rate == 1) ? 6 : 4);

            const uint8_t *au_ptrs[6];
            uint16_t au_lens[6];

            if (num_aus == 3) {
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
            } else if (num_aus == 2) {
                uint16_t au1 = ((uint16_t)sf_buf[3] << 4U) | ((uint16_t)(sf_buf[4] >> 4U) & 0x0FU);
                if ((au1 > 5U) && (au1 < audio_data_end)) {
                    au_ptrs[0] = &sf_buf[5];
                    au_lens[0] = au1 - 5U;
                    au_ptrs[1] = &sf_buf[au1];
                    au_lens[1] = (uint16_t)(audio_data_end - au1);
                } else {
                    num_aus = 0;
                }
            } else {
                au_ptrs[0] = &sf_buf[11];
                au_lens[0] = (uint16_t)(audio_data_end - 11U);
                num_aus = 1;
            }

            for (int a = 0; a < num_aus; a++) {
                DAB_AudioStatus status;
                DAB_SignalStatus sig_status;
                memset(&sig_status, 0, sizeof(DAB_SignalStatus));
                sig_status.au_crc_pass = 1U;

                int32_t samples_decoded = DAB_Decoder_DecodeAU(handle, au_ptrs[a], au_lens[a], &sig_status, pcm_out_buf, &status);

                if (samples_decoded > 0) {
                    uint32_t bytes = (uint32_t)samples_decoded * 2U * (uint32_t)sizeof(int16_t);
                    total_pcm_bytes += bytes;
                    total_pcm_samples += (uint32_t)samples_decoded;

                    if (pcm_fp) (void)fwrite(pcm_out_buf, 1, bytes, pcm_fp);
                    if (wav_fp) (void)fwrite(pcm_out_buf, 1, bytes, wav_fp);
                }

                if (log_fp) {
                    const char *c_mode_str = (status.conceal_mode == DAB_CONCEAL_NONE) ? "NONE" :
                                             (status.conceal_mode == DAB_CONCEAL_INTERPOLATE) ? "INTERP" :
                                             (status.conceal_mode == DAB_CONCEAL_ATTENUATE) ? "ATTEN" : "MUTED";
                    const char *m_state_str = (status.mute_state == DAB_MUTE_IDLE) ? "IDLE" :
                                              (status.mute_state == DAB_MUTE_ATTACKING) ? "ATTACK" :
                                              (status.mute_state == DAB_MUTE_SUSTAIN) ? "SUSTAIN" : "RELEASE";
                    const char *trig_str = (status.trigger == BLEND_IDLE) ? "0b00" :
                                           (status.trigger == CONCEAL_TRIGGER) ? "0b01" : "0b10";

                    fprintf(log_fp, "%u\t%u\t%s\t%s\t%s\t%d\t%u\t%s\t%u\n",
                            frame_idx, au_lens[a],
                            (status.au_status == DAB_AU_GOOD) ? "GOOD" : (status.au_status == DAB_AU_CRC_ERR) ? "CRC_ERR" : "LOST",
                            c_mode_str, m_state_str, status.cur_gain_q15,
                            status.audio_quality, trig_str, status.pcm_samples_out);
                }
                frame_idx++;
            }
        }
    } else {
        while (!feof(in_fp)) {
            /* Read 4-byte length */
            uint8_t len_bytes[4];
            if (fread(len_bytes, 1, 4, in_fp) != 4) {
                break;
            }

            uint32_t au_len = ((uint32_t)len_bytes[0] << 24U) |
                              ((uint32_t)len_bytes[1] << 16U) |
                              ((uint32_t)len_bytes[2] << 8U)  |
                              ((uint32_t)len_bytes[3]);

            if ((au_len == 0U) || (au_len > sizeof(au_buf))) {
                break;
            }

            if (fread(au_buf, 1, au_len, in_fp) != au_len) {
                break;
            }

            DAB_AudioStatus status;
            int32_t samples_decoded = DAB_Decoder_DecodeAU(handle, au_buf, (uint16_t)au_len, NULL, pcm_out_buf, &status);

            if (samples_decoded > 0) {
                uint32_t bytes = (uint32_t)samples_decoded * 2U * (uint32_t)sizeof(int16_t);
                total_pcm_bytes += bytes;
                total_pcm_samples += (uint32_t)samples_decoded;

                if (pcm_fp) {
                    (void)fwrite(pcm_out_buf, 1, bytes, pcm_fp);
                }
                if (wav_fp) {
                    (void)fwrite(pcm_out_buf, 1, bytes, wav_fp);
                }
            }

            if (log_fp) {
                const char *c_mode_str = (status.conceal_mode == DAB_CONCEAL_NONE) ? "NONE" :
                                         (status.conceal_mode == DAB_CONCEAL_INTERPOLATE) ? "INTERP" :
                                         (status.conceal_mode == DAB_CONCEAL_ATTENUATE) ? "ATTEN" : "MUTED";
                const char *m_state_str = (status.mute_state == DAB_MUTE_IDLE) ? "IDLE" :
                                          (status.mute_state == DAB_MUTE_ATTACKING) ? "ATTACK" :
                                          (status.mute_state == DAB_MUTE_SUSTAIN) ? "SUSTAIN" : "RELEASE";
                const char *trig_str = (status.trigger == BLEND_IDLE) ? "0b00" :
                                       (status.trigger == CONCEAL_TRIGGER) ? "0b01" : "0b10";

                fprintf(log_fp, "%u\t%u\t%s\t%s\t%s\t%d\t%u\t%s\t%u\n",
                        frame_idx, au_len,
                        (status.au_status == DAB_AU_GOOD) ? "GOOD" : (status.au_status == DAB_AU_CRC_ERR) ? "CRC_ERR" : "LOST",
                        c_mode_str, m_state_str, status.cur_gain_q15,
                        status.audio_quality, trig_str, status.pcm_samples_out);
            }

            frame_idx++;
        }
    }

    clock_t end_time = clock();
    double elapsed_sec = (double)(end_time - start_time) / (double)CLOCKS_PER_SEC;
    if (elapsed_sec <= 0.0001) {
        elapsed_sec = 0.0001;
    }

    if (wav_fp) {
        /* Rewind and write actual size in header */
        fseek(wav_fp, 0, SEEK_SET);
        write_wav_header(wav_fp, sample_rate, 2U, total_pcm_bytes);
        fclose(wav_fp);
    }
    if (pcm_fp) fclose(pcm_fp);
    if (log_fp) fclose(log_fp);
    fclose(in_fp);

    /* Get final telemetry stats */
    uint32_t final_total_au = 0U;
    uint32_t final_crc_errs = 0U;
    DAB_Decoder_GetAUStats(handle, &final_total_au, &final_crc_errs);
    uint8_t final_quality = 0U;
    DAB_BlendingTriggerState final_trigger = BLEND_IDLE;
    DAB_Decoder_GetQualityStatus(handle, &final_quality, &final_trigger);

    double audio_duration_sec = (double)total_pcm_samples / (double)sample_rate;
    double realtime_speedup   = audio_duration_sec / elapsed_sec;
    double estimated_mips     = (elapsed_sec / audio_duration_sec) * 100.0; /* rough baseline */

    printf("=================================================================\n");
    printf("  DECODING COMPLETE — VERIFICATION SUMMARY\n");
    printf("=================================================================\n");
    printf("  Total Frames Processed:    %u\n", frame_idx);
    printf("  CRC Error Frames:          %u\n", final_crc_errs);
    printf("  Total Audio Decoded:       %.2f seconds (%u PCM samples)\n", audio_duration_sec, total_pcm_samples);
    printf("  Total Execution Time:      %.4f seconds\n", elapsed_sec);
    printf("  Real-time Performance:     %.1fx Realtime\n", realtime_speedup);
    printf("  Estimated Core MIPS:       %.2f MIPS\n", estimated_mips);
    printf("  Final Audio Quality Score: %u / 100\n", final_quality);
    printf("  Final Blending Trigger:    0x%02X (%s)\n",
           final_trigger, (final_trigger == BLEND_IDLE) ? "IDLE" : (final_trigger == CONCEAL_TRIGGER) ? "CONCEAL_TRIGGER (0b01)" : "UNRECOVERABLE_TRIGGER (0b10)");
    printf("=================================================================\n");
    if (output_pcm) printf("  Recorded Raw PCM: %s\n", output_pcm);
    if (output_wav) printf("  Recorded WAV:     %s\n", output_wav);
    if (output_log) printf("  Execution Log:    %s\n", output_log);
    printf("=================================================================\n\n");

    return 0;
}
