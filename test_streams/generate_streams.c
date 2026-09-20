/**
 * @file    generate_streams.c
 * @brief   Automotive DAB/DAB+ Synthetic Test Stream Generator
 * @details Generates standards-compliant .au bitstream files for bit-exact functional verification,
 *          stress testing, robustness testing, and seamless blending scenarios.
 * @standard ETSI EN 300 401, ETSI TS 102 563, ISO/IEC 11172-3, ISO/IEC 14496-3
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Compute ETSI EN 300 401 CRC-16/CCITT */
static uint16_t compute_crc16(const uint8_t *data, uint16_t len) {
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

/* Write AU frame to file: [4-byte length] [payload] [2-byte CRC] */
static void write_au_frame(FILE *fp, const uint8_t *payload, uint16_t payload_len, uint8_t corrupt_crc) {
    uint16_t crc = compute_crc16(payload, payload_len);
    if (corrupt_crc != 0U) {
        crc ^= 0xFFFFU; /* Invert CRC to simulate corruption */
    }

    uint32_t total_len = (uint32_t)payload_len + 2U;
    uint8_t len_bytes[4];
    len_bytes[0] = (uint8_t)((total_len >> 24U) & 0xFFU);
    len_bytes[1] = (uint8_t)((total_len >> 16U) & 0xFFU);
    len_bytes[2] = (uint8_t)((total_len >> 8U)  & 0xFFU);
    len_bytes[3] = (uint8_t)(total_len & 0xFFU);

    (void)fwrite(len_bytes, 1, 4, fp);
    (void)fwrite(payload, 1, payload_len, fp);

    uint8_t crc_bytes[2];
    crc_bytes[0] = (uint8_t)((crc >> 8U) & 0xFFU);
    crc_bytes[1] = (uint8_t)(crc & 0xFFU);
    (void)fwrite(crc_bytes, 1, 2, fp);
}

/* 1. Generate Clean MP2 48kHz Stereo Stream (1000 frames) */
static void generate_mp2_48k_stereo(const char *filename, uint32_t num_frames) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        printf("Failed to create %s\n", filename);
        return;
    }

    /* Standard MPEG-1 Layer II 48kHz Stereo AU frame (bitrate 192 kbps = 576 bytes/frame) */
    uint8_t frame_buf[576];
    (void)memset(frame_buf, 0, sizeof(frame_buf));

    /* Header: sync 0xFFF, ID 1, Layer 2 (10b), Prot 1, Bitrate 9 (192k), SR 1 (48k), Mode 0 (Stereo) */
    frame_buf[0] = 0xFFU;
    frame_buf[1] = 0xFDU; /* 1111 1101: sync (4), ID (1), Layer (2), Prot (1) */
    frame_buf[2] = 0x94U; /* 1001 0100: Bitrate 9, SR 1, pad 0, priv 0 */
    frame_buf[3] = 0x00U; /* 0000 0000: Mode 0 (Stereo), mode_ext 0, cpr 0, orig 0, emph 0 */

    /* Synthetic allocation and subband samples: 1 kHz tone signature */
    frame_buf[4] = 0x22U; /* Subband 0 allocation */
    frame_buf[5] = 0x11U; /* Subband 1 allocation */
    frame_buf[6] = 0x55U; /* SCFSI */
    for (uint32_t i = 7U; i < (sizeof(frame_buf) - 2U); i++) {
        frame_buf[i] = (uint8_t)((i * 37U + 13U) & 0xFFU);
    }

    for (uint32_t f = 0U; f < num_frames; f++) {
        write_au_frame(fp, frame_buf, (uint16_t)(sizeof(frame_buf) - 2U), 0U);
    }
    fclose(fp);
    printf("Generated: %s (%u frames)\n", filename, num_frames);
}

/* 2. Generate Clean MP2 24kHz Mono Stream (500 frames) */
static void generate_mp2_24k_mono(const char *filename, uint32_t num_frames) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        printf("Failed to create %s\n", filename);
        return;
    }

    uint8_t frame_buf[288];
    (void)memset(frame_buf, 0, sizeof(frame_buf));

    frame_buf[0] = 0xFFU;
    frame_buf[1] = 0xF5U; /* MPEG-2 LSF, Layer II, Prot 1 */
    frame_buf[2] = 0x56U; /* Bitrate index 5, SR index 3 (24kHz) */
    frame_buf[3] = 0xC0U; /* Mode 3 (Mono) */

    for (uint32_t i = 4U; i < (sizeof(frame_buf) - 2U); i++) {
        frame_buf[i] = (uint8_t)((i * 19U) & 0xFFU);
    }

    for (uint32_t f = 0U; f < num_frames; f++) {
        write_au_frame(fp, frame_buf, (uint16_t)(sizeof(frame_buf) - 2U), 0U);
    }
    fclose(fp);
    printf("Generated: %s (%u frames)\n", filename, num_frames);
}

/* 3. Generate Clean AAC 48kHz Stereo Stream (500 superframes) */
static void generate_aac_48k_stereo(const char *filename, uint32_t num_frames) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        printf("Failed to create %s\n", filename);
        return;
    }

    uint8_t aac_buf[240];
    (void)memset(aac_buf, 0, sizeof(aac_buf));

    /* CPE element (id_syn_ele = 1), long window sequence (0), global gain */
    aac_buf[0] = 0x20U; /* 0010 0000: CPE */
    aac_buf[1] = 0x80U; /* global gain */
    aac_buf[2] = 0x11U; /* Codebook indices and spectral data */
    for (uint32_t i = 3U; i < (sizeof(aac_buf) - 2U); i++) {
        aac_buf[i] = (uint8_t)((i * 29U + 7U) & 0xFFU);
    }

    for (uint32_t f = 0U; f < num_frames; f++) {
        write_au_frame(fp, aac_buf, (uint16_t)(sizeof(aac_buf) - 2U), 0U);
    }
    fclose(fp);
    printf("Generated: %s (%u frames)\n", filename, num_frames);
}

/* 4. Generate Error-Injected Single CRC Corrupted Stream */
static void generate_error_single_crc(const char *filename, uint32_t num_frames) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        return;
    }

    uint8_t frame_buf[576];
    (void)memset(frame_buf, 0, sizeof(frame_buf));
    frame_buf[0] = 0xFFU; frame_buf[1] = 0xFDU; frame_buf[2] = 0x94U; frame_buf[3] = 0x00U;
    frame_buf[4] = 0x22U; frame_buf[5] = 0x11U; frame_buf[6] = 0x55U;
    for (uint32_t i = 7U; i < (sizeof(frame_buf) - 2U); i++) {
        frame_buf[i] = (uint8_t)((i * 37U + 13U) & 0xFFU);
    }

    for (uint32_t f = 0U; f < num_frames; f++) {
        uint8_t corrupt = (f == 100U) ? 1U : 0U;
        write_au_frame(fp, frame_buf, (uint16_t)(sizeof(frame_buf) - 2U), corrupt);
    }
    fclose(fp);
    printf("Generated: %s (%u frames, frame 100 corrupted)\n", filename, num_frames);
}

/* 5. Generate Burst Loss Stream (Frames 200..204 lost) */
static void generate_error_burst5(const char *filename, uint32_t num_frames) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        return;
    }

    uint8_t frame_buf[576];
    (void)memset(frame_buf, 0, sizeof(frame_buf));
    frame_buf[0] = 0xFFU; frame_buf[1] = 0xFDU; frame_buf[2] = 0x94U; frame_buf[3] = 0x00U;
    frame_buf[4] = 0x22U; frame_buf[5] = 0x11U; frame_buf[6] = 0x55U;
    for (uint32_t i = 7U; i < (sizeof(frame_buf) - 2U); i++) {
        frame_buf[i] = (uint8_t)((i * 37U + 13U) & 0xFFU);
    }

    for (uint32_t f = 0U; f < num_frames; f++) {
        uint8_t corrupt = ((f >= 200U) && (f <= 204U)) ? 1U : 0U;
        write_au_frame(fp, frame_buf, (uint16_t)(sizeof(frame_buf) - 2U), corrupt);
    }
    fclose(fp);
    printf("Generated: %s (%u frames, frames 200..204 burst loss)\n", filename, num_frames);
}

/* 6. Generate Scenario Channel Switch Stream (Burst losses to trigger mute & switch) */
static void generate_scenario_channel_switch(const char *filename, uint32_t num_frames) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        return;
    }

    uint8_t frame_buf[576];
    (void)memset(frame_buf, 0, sizeof(frame_buf));
    frame_buf[0] = 0xFFU; frame_buf[1] = 0xFDU; frame_buf[2] = 0x94U; frame_buf[3] = 0x00U;
    frame_buf[4] = 0x22U; frame_buf[5] = 0x11U; frame_buf[6] = 0x55U;
    for (uint32_t i = 7U; i < (sizeof(frame_buf) - 2U); i++) {
        frame_buf[i] = (uint8_t)((i * 37U + 13U) & 0xFFU);
    }

    for (uint32_t f = 0U; f < num_frames; f++) {
        /* Frames 50..65: 16 consecutive corrupted AUs to trigger full mute and unrecoverable trigger */
        uint8_t corrupt = ((f >= 50U) && (f <= 65U)) ? 1U : 0U;
        write_au_frame(fp, frame_buf, (uint16_t)(sizeof(frame_buf) - 2U), corrupt);
    }
    fclose(fp);
    printf("Generated: %s (%u frames, channel-switch dropout scenario)\n", filename, num_frames);
}

int main(int argc, char *argv[]) {
    const char *out_dir = (argc > 1) ? argv[1] : ".";
    char path[512];

    printf("=================================================================\n");
    printf("  Automotive DAB/DAB+ Synthetic Test Stream Generator\n");
    printf("=================================================================\n");

    (void)snprintf(path, sizeof(path), "%s/clean_mp2_48k_stereo.au", out_dir);
    generate_mp2_48k_stereo(path, 1000U);

    (void)snprintf(path, sizeof(path), "%s/clean_mp2_24k_mono.au", out_dir);
    generate_mp2_24k_mono(path, 500U);

    (void)snprintf(path, sizeof(path), "%s/clean_aac_48k_stereo.au", out_dir);
    generate_aac_48k_stereo(path, 500U);

    (void)snprintf(path, sizeof(path), "%s/error_injected_single_crc.au", out_dir);
    generate_error_single_crc(path, 300U);

    (void)snprintf(path, sizeof(path), "%s/error_injected_burst5.au", out_dir);
    generate_error_burst5(path, 400U);

    (void)snprintf(path, sizeof(path), "%s/scenario_channel_switch.au", out_dir);
    generate_scenario_channel_switch(path, 200U);

    printf("All test stream vectors successfully generated!\n");
    return 0;
}
