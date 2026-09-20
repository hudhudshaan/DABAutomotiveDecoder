/**
 * @file    mp2_decoder.c
 * @brief   Standards-Compliant DAB MUSICAM (ISO/IEC 11172-3 Layer II) Audio Unit Decoder
 * @standard MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "mp2_decoder.h"
#include "../core/bitstream_reader.h"
#include "../dsp/dsp_math.h"
#include <string.h>

void mp2_decoder_init(mp2_decoder_state_t *p_state) {
    if (p_state == NULL) {
        return;
    }
    (void)memset(p_state, 0, sizeof(mp2_decoder_state_t));
    p_state->sample_rate_hz = 48000U;
    p_state->num_channels   = 2U;
    p_state->mode           = 0U; /* Stereo */
    p_state->mode_extension = 0U;
    mp2_synth_init(&p_state->synth_state);
}

int32_t mp2_decoder_process_au(
    mp2_decoder_state_t *p_state,
    const uint8_t       *p_au_data,
    uint16_t             au_len,
    int16_t             *pcm_out_interleaved,
    uint16_t            *p_num_samples_per_ch,
    uint8_t              enable_neon
) {
    if ((p_state == NULL) || (p_au_data == NULL) || (pcm_out_interleaved == NULL) || (p_num_samples_per_ch == NULL)) {
        return DAB_ERR_NULL_HANDLE;
    }
    if (au_len < 4U) {
        return DAB_ERR_INVALID_PARAM;
    }

    bitstream_reader_t bs;
    bitstream_init(&bs, p_au_data, (uint32_t)au_len);

    /* Step 1: Parse MPEG-1/2 Layer II Frame Header per ISO 11172-3 */
    uint32_t sync = bitstream_peek_bits(&bs, 12);
    if (sync == 0xFFFU) {
        bitstream_skip_bits(&bs, 12); /* Sync 12 bits */
        uint8_t id = (uint8_t)bitstream_get_bits(&bs, 1);
        uint8_t layer = (uint8_t)bitstream_get_bits(&bs, 2);
        (void)layer;
        uint8_t protection = (uint8_t)bitstream_get_bits(&bs, 1);
        bitstream_skip_bits(&bs, 4);  /* Bitrate index */
        uint8_t sr_idx = (uint8_t)bitstream_get_bits(&bs, 2);
        if (id == 1U) {
            /* MPEG-1 Full-Rate */
            if (sr_idx == 1U) {
                p_state->sample_rate_hz = 48000U;
            } else if (sr_idx == 0U) {
                p_state->sample_rate_hz = 44100U;
            } else {
                p_state->sample_rate_hz = 32000U;
            }
        } else {
            /* MPEG-2 LSF Half-Rate */
            if (sr_idx == 1U) {
                p_state->sample_rate_hz = 24000U;
            } else if (sr_idx == 0U) {
                p_state->sample_rate_hz = 22050U;
            } else {
                p_state->sample_rate_hz = 16000U;
            }
        }

        bitstream_skip_bits(&bs, 1);  /* Padding bit */
        bitstream_skip_bits(&bs, 1);  /* Private bit */
        p_state->mode = (uint8_t)bitstream_get_bits(&bs, 2);
        p_state->mode_extension = (uint8_t)bitstream_get_bits(&bs, 2);
        bitstream_skip_bits(&bs, 1);  /* Copyright */
        bitstream_skip_bits(&bs, 1);  /* Original */
        bitstream_skip_bits(&bs, 2);  /* Emphasis */

        if (protection == 0U) {
            bitstream_skip_bits(&bs, 16); /* Skip 16-bit CRC if present */
        }
    }

    uint8_t num_channels = (p_state->mode == 3U) ? 1U : 2U;
    p_state->num_channels = num_channels;

    /* Joint stereo bound determination (ISO 11172-3 Table 3-B.1) */
    uint8_t jsbound = 32U;
    if (p_state->mode == 1U) {
        switch (p_state->mode_extension) {
            case 0U: jsbound = 4U;  break;
            case 1U: jsbound = 8U;  break;
            case 2U: jsbound = 12U; break;
            case 3U: jsbound = 16U; break;
            default: jsbound = 16U; break;
        }
    }

    const mp2_subband_alloc_t *p_alloc_table = (p_state->sample_rate_hz <= 24000U) ? g_mp2_alloc_table_24k : g_mp2_alloc_table_48k;

    /* Step 2: Unpack Bit Allocation Table */
    for (uint8_t sb = 0U; sb < 32U; sb++) {
        uint8_t nbal = p_alloc_table[sb].nbal;
        if (nbal > 0U) {
            if ((p_state->mode == 1U) && (sb >= jsbound)) {
                /* Joint Stereo: Shared allocation */
                uint8_t alloc = (uint8_t)bitstream_get_bits(&bs, nbal);
                p_state->bit_alloc[0][sb] = alloc;
                p_state->bit_alloc[1][sb] = alloc;
            } else {
                for (uint8_t ch = 0U; ch < num_channels; ch++) {
                    p_state->bit_alloc[ch][sb] = (uint8_t)bitstream_get_bits(&bs, nbal);
                }
            }
        } else {
            for (uint8_t ch = 0U; ch < 2U; ch++) {
                p_state->bit_alloc[ch][sb] = 0U;
            }
        }
    }

    /* Step 3: Unpack 2-Bit Scale Factor Select Information (SCFSI) */
    for (uint8_t sb = 0U; sb < 32U; sb++) {
        for (uint8_t ch = 0U; ch < num_channels; ch++) {
            if (p_state->bit_alloc[ch][sb] != 0U) {
                p_state->scfsi[ch][sb] = (uint8_t)bitstream_get_bits(&bs, 2);
            } else {
                p_state->scfsi[ch][sb] = 0U;
            }
        }
    }

    /* Step 4: Unpack 6-Bit Scale Factors */
    for (uint8_t sb = 0U; sb < 32U; sb++) {
        for (uint8_t ch = 0U; ch < num_channels; ch++) {
            if (p_state->bit_alloc[ch][sb] != 0U) {
                uint8_t scfsi = p_state->scfsi[ch][sb];
                uint8_t idx0 = 0U;
                uint8_t idx1 = 0U;
                uint8_t idx2 = 0U;

                switch (scfsi) {
                    case 0U:
                        idx0 = (uint8_t)bitstream_get_bits(&bs, 6);
                        idx1 = (uint8_t)bitstream_get_bits(&bs, 6);
                        idx2 = (uint8_t)bitstream_get_bits(&bs, 6);
                        break;
                    case 1U:
                        idx0 = (uint8_t)bitstream_get_bits(&bs, 6);
                        idx1 = (uint8_t)bitstream_get_bits(&bs, 6);
                        idx2 = idx1;
                        break;
                    case 2U:
                        idx0 = (uint8_t)bitstream_get_bits(&bs, 6);
                        idx1 = idx0;
                        idx2 = idx0;
                        break;
                    case 3U:
                        idx0 = (uint8_t)bitstream_get_bits(&bs, 6);
                        idx1 = idx0;
                        idx2 = (uint8_t)bitstream_get_bits(&bs, 6);
                        break;
                    default:
                        idx0 = 0U; idx1 = 0U; idx2 = 0U;
                        break;
                }

                p_state->scale_factors[ch][sb][0] = g_mp2_scale_factors[idx0 & 0x3FU];
                p_state->scale_factors[ch][sb][1] = g_mp2_scale_factors[idx1 & 0x3FU];
                p_state->scale_factors[ch][sb][2] = g_mp2_scale_factors[idx2 & 0x3FU];
            } else {
                p_state->scale_factors[ch][sb][0] = 0.0f;
                p_state->scale_factors[ch][sb][1] = 0.0f;
                p_state->scale_factors[ch][sb][2] = 0.0f;
            }
        }
    }

    /* Step 5: Subband Sample Unpacking & De-quantization (12 groups of 3 = 36 blocks) */
    for (uint8_t grp = 0U; grp < 12U; grp++) {
        uint8_t grp_idx = grp / 4U;
        for (uint8_t sb = 0U; sb < 32U; sb++) {
            uint8_t is_joint = ((p_state->mode == 1U) && (sb >= jsbound)) ? 1U : 0U;

            if (is_joint != 0U) {
                /* Joint stereo: one sample decoded, copied with channel-specific scale factors */
                uint8_t alloc = p_state->bit_alloc[0][sb];
                if (alloc != 0U) {
                    uint8_t quant_class = p_alloc_table[sb].quant_idx[alloc];
                    float c_val = g_mp2_c_coeff[quant_class];
                    float d_val = g_mp2_d_coeff[quant_class];
                    float s_deq[3] = {0.0f, 0.0f, 0.0f};

                    if (quant_class == 1U) {
                        uint32_t val = bitstream_get_bits(&bs, 5);
                        uint32_t s0 = val % 3U; val /= 3U;
                        uint32_t s1 = val % 3U;
                        uint32_t s2 = val / 3U;
                        s_deq[0] = c_val * (((float)s0 / 2.0f) + d_val);
                        s_deq[1] = c_val * (((float)s1 / 2.0f) + d_val);
                        s_deq[2] = c_val * (((float)s2 / 2.0f) + d_val);
                    } else if (quant_class == 2U) {
                        uint32_t val = bitstream_get_bits(&bs, 7);
                        uint32_t s0 = val % 5U; val /= 5U;
                        uint32_t s1 = val % 5U;
                        uint32_t s2 = val / 5U;
                        s_deq[0] = c_val * (((float)s0 / 4.0f) + d_val);
                        s_deq[1] = c_val * (((float)s1 / 4.0f) + d_val);
                        s_deq[2] = c_val * (((float)s2 / 4.0f) + d_val);
                    } else if (quant_class == 4U) {
                        uint32_t val = bitstream_get_bits(&bs, 10);
                        uint32_t s0 = val % 9U; val /= 9U;
                        uint32_t s1 = val % 9U;
                        uint32_t s2 = val / 9U;
                        s_deq[0] = c_val * (((float)s0 / 8.0f) + d_val);
                        s_deq[1] = c_val * (((float)s1 / 8.0f) + d_val);
                        s_deq[2] = c_val * (((float)s2 / 8.0f) + d_val);
                    } else {
                        uint8_t bits = g_mp2_bits_per_sample[quant_class];
                        uint32_t max_val = (bits > 0U) ? ((1U << bits) - 1U) : 1U;
                        for (uint8_t s = 0U; s < 3U; s++) {
                            uint32_t sample_raw = bitstream_get_bits(&bs, bits);
                            float s_norm = (float)sample_raw / (float)max_val;
                            s_deq[s] = c_val * (s_norm + d_val);
                        }
                    }

                    for (uint8_t ch = 0U; ch < 2U; ch++) {
                        float scf = p_state->scale_factors[ch][sb][grp_idx];
                        for (uint8_t s = 0U; s < 3U; s++) {
                            p_state->sb_samples[ch][(uint16_t)grp * 3U + s][sb] = s_deq[s] * scf;
                        }
                    }
                } else {
                    for (uint8_t ch = 0U; ch < 2U; ch++) {
                        for (uint8_t s = 0U; s < 3U; s++) {
                            p_state->sb_samples[ch][(uint16_t)grp * 3U + s][sb] = 0.0f;
                        }
                    }
                }
            } else {
                for (uint8_t ch = 0U; ch < num_channels; ch++) {
                    uint8_t alloc = p_state->bit_alloc[ch][sb];
                    float scf = p_state->scale_factors[ch][sb][grp_idx];

                    if (alloc != 0U) {
                        uint8_t quant_class = p_alloc_table[sb].quant_idx[alloc];
                        float c_val = g_mp2_c_coeff[quant_class];
                        float d_val = g_mp2_d_coeff[quant_class];

                        if (quant_class == 1U) {
                            uint32_t val = bitstream_get_bits(&bs, 5);
                            uint32_t s0 = val % 3U; val /= 3U;
                            uint32_t s1 = val % 3U;
                            uint32_t s2 = val / 3U;
                            p_state->sb_samples[ch][(uint16_t)grp * 3U + 0U][sb] = c_val * (((float)s0 / 2.0f) + d_val) * scf;
                            p_state->sb_samples[ch][(uint16_t)grp * 3U + 1U][sb] = c_val * (((float)s1 / 2.0f) + d_val) * scf;
                            p_state->sb_samples[ch][(uint16_t)grp * 3U + 2U][sb] = c_val * (((float)s2 / 2.0f) + d_val) * scf;
                        } else if (quant_class == 2U) {
                            uint32_t val = bitstream_get_bits(&bs, 7);
                            uint32_t s0 = val % 5U; val /= 5U;
                            uint32_t s1 = val % 5U;
                            uint32_t s2 = val / 5U;
                            p_state->sb_samples[ch][(uint16_t)grp * 3U + 0U][sb] = c_val * (((float)s0 / 4.0f) + d_val) * scf;
                            p_state->sb_samples[ch][(uint16_t)grp * 3U + 1U][sb] = c_val * (((float)s1 / 4.0f) + d_val) * scf;
                            p_state->sb_samples[ch][(uint16_t)grp * 3U + 2U][sb] = c_val * (((float)s2 / 4.0f) + d_val) * scf;
                        } else if (quant_class == 4U) {
                            uint32_t val = bitstream_get_bits(&bs, 10);
                            uint32_t s0 = val % 9U; val /= 9U;
                            uint32_t s1 = val % 9U;
                            uint32_t s2 = val / 9U;
                            p_state->sb_samples[ch][(uint16_t)grp * 3U + 0U][sb] = c_val * (((float)s0 / 8.0f) + d_val) * scf;
                            p_state->sb_samples[ch][(uint16_t)grp * 3U + 1U][sb] = c_val * (((float)s1 / 8.0f) + d_val) * scf;
                            p_state->sb_samples[ch][(uint16_t)grp * 3U + 2U][sb] = c_val * (((float)s2 / 8.0f) + d_val) * scf;
                        } else {
                            uint8_t bits = g_mp2_bits_per_sample[quant_class];
                            uint32_t max_val = (bits > 0U) ? ((1U << bits) - 1U) : 1U;
                            for (uint8_t s = 0U; s < 3U; s++) {
                                uint32_t sample_raw = bitstream_get_bits(&bs, bits);
                                float s_norm = (float)sample_raw / (float)max_val;
                                float s_dequant = c_val * (s_norm + d_val);
                                p_state->sb_samples[ch][(uint16_t)grp * 3U + s][sb] = s_dequant * scf;
                            }
                        }
                    } else {
                        for (uint8_t s = 0U; s < 3U; s++) {
                            p_state->sb_samples[ch][(uint16_t)grp * 3U + s][sb] = 0.0f;
                        }
                    }
                }
            }
        }
    }

    /* Step 6: 36 Polyphase Matrix Synthesis Iterations -> 1152 PCM samples/channel */
    uint16_t out_count = (p_state->sample_rate_hz <= 24000U) ? DAB_MP2_SAMPLES_PER_AU_24K : DAB_MP2_SAMPLES_PER_AU_48K;
    uint8_t max_blocks = (p_state->sample_rate_hz <= 24000U) ? 18U : 36U;

    for (uint8_t s = 0U; s < max_blocks; s++) {
        float block_subband[2][32];
        float block_pcm[2][32];

        for (uint8_t ch = 0U; ch < 2U; ch++) {
            uint8_t src_ch = (ch < num_channels) ? ch : 0U;
            for (uint8_t k = 0U; k < 32U; k++) {
                block_subband[ch][k] = p_state->sb_samples[src_ch][s][k];
            }
        }

        mp2_synth_process_subband_block(&p_state->synth_state, block_subband, block_pcm, 2U, enable_neon);

        for (uint8_t j = 0U; j < 32U; j++) {
            uint16_t pcm_idx = (uint16_t)(((uint16_t)s * 32U + j) * 2U);
            float left_f  = block_pcm[0][j] * 32767.0f;
            float right_f = block_pcm[1][j] * 32767.0f;

            pcm_out_interleaved[pcm_idx]      = dsp_clamp16((int32_t)left_f);
            pcm_out_interleaved[pcm_idx + 1U] = dsp_clamp16((int32_t)right_f);
        }
    }

    *p_num_samples_per_ch = out_count;
    return DAB_OK;
}
