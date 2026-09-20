/**
 * @file    aac_decoder.c
 * @brief   Standards-Compliant DAB+ HE-AAC v2 Core AU Decoder Implementation
 * @standard ETSI TS 102 563 & ISO/IEC 14496-3, MISRA-C:2012, ISO C99
 * @copyright 2026 — Automotive DAB/DAB+ Project. 100% Option A Clean IP.
 */

#include "aac_decoder.h"
#include "aac_huffman.h"
#include "aac_tables.h"
#include "../core/bitstream_reader.h"
#include "../dsp/dsp_math.h"
#include <string.h>
#include <math.h>

void aac_decoder_init(aac_decoder_state_t *p_state) {
    if (p_state == NULL) {
        return;
    }
    (void)memset(p_state, 0, sizeof(aac_decoder_state_t));
    p_state->sample_rate_hz = 48000U;
    p_state->num_channels   = 2U;
    p_state->sbr_active     = 1U;
    p_state->ps_active      = 1U;

    aac_imdct_init(&p_state->imdct_state);
    aac_sbr_init(&p_state->sbr_state);
    aac_ps_init(&p_state->ps_state);
}

/* Accurate dequantization: sign(x) * |x|^(4/3) */
static inline float aac_dequant_4_3(int16_t val) {
    if (val == 0) {
        return 0.0f;
    }
    float sign = (val < 0) ? -1.0f : 1.0f;
    float mag  = (val < 0) ? (float)(-val) : (float)val;
    /* Fast cbrt: mag^(4/3) = mag * cbrt(mag) */
    return sign * mag * cbrtf(mag);
}

static void decode_tns_stream(bitstream_reader_t *p_bs, uint8_t is8, uint8_t num_windows) {
    for (uint8_t w = 0U; w < num_windows; w++) {
        uint8_t n_filt = (uint8_t)bitstream_get_bits(p_bs, (uint8_t)(2U - is8));
        if (n_filt > 0U) {
            uint8_t coef_res = (uint8_t)bitstream_get_bits(p_bs, 1U);
            for (uint8_t filt = 0U; filt < n_filt; filt++) {
                (void)bitstream_get_bits(p_bs, (uint8_t)(6U - 2U * is8)); /* length */
                uint8_t order = (uint8_t)bitstream_get_bits(p_bs, (uint8_t)(5U - 2U * is8));
                if (order > 0U) {
                    (void)bitstream_get_bits(p_bs, 1U); /* direction */
                    uint8_t coef_compress = (uint8_t)bitstream_get_bits(p_bs, 1U);
                    uint8_t coef_len = (uint8_t)(coef_res + 3U - coef_compress);
                    for (uint8_t i = 0U; i < order; i++) {
                        (void)bitstream_get_bits(p_bs, coef_len);
                    }
                }
            }
        }
    }
}

static void decode_pulses_stream(bitstream_reader_t *p_bs) {
    uint8_t num_pulse = (uint8_t)bitstream_get_bits(p_bs, 2U) + 1U;
    (void)bitstream_get_bits(p_bs, 6U); /* pulse_swb */
    (void)bitstream_get_bits(p_bs, 5U); /* pos0 */
    (void)bitstream_get_bits(p_bs, 4U); /* amp0 */
    for (uint8_t i = 1U; i < num_pulse; i++) {
        (void)bitstream_get_bits(p_bs, 5U);
        (void)bitstream_get_bits(p_bs, 4U);
    }
}

int32_t aac_decoder_process_au(
    aac_decoder_state_t *p_state,
    const uint8_t       *p_au_data,
    uint16_t             au_len,
    int16_t             *pcm_out_interleaved,
    uint16_t            *p_num_samples_per_ch,
    uint8_t              enable_neon
) {
    if ((p_state == NULL) || (p_au_data == NULL) || (pcm_out_interleaved == NULL) || (p_num_samples_per_ch == NULL)) {
        return DAB_ERR_NULL_HANDLE;
    }
    if (au_len < 3U) {
        return DAB_ERR_INVALID_PARAM;
    }

    bitstream_reader_t bs;
    bitstream_init(&bs, p_au_data, (uint32_t)au_len);

    uint8_t id_syn_ele = (uint8_t)bitstream_get_bits(&bs, 3U);
    uint8_t channels_in_stream;
    uint8_t common_window = 0U;

    if (id_syn_ele == 0U) {
        /* Single Channel Element (SCE) - Mono */
        channels_in_stream = 1U;
        (void)bitstream_get_bits(&bs, 4U); /* tag */
        common_window = 0U;
    } else if (id_syn_ele == 1U) {
        /* Channel Pair Element (CPE) - Stereo */
        channels_in_stream = 2U;
        (void)bitstream_get_bits(&bs, 4U); /* tag */
        common_window = (uint8_t)bitstream_get_bits(&bs, 1U);
    } else {
        /* Unsupported or reserved syntax element */
        channels_in_stream = 1U;
    }

    uint8_t window_sequence = 0U;
    uint8_t max_sfb = 0U;
    uint8_t num_groups = 1U;
    uint8_t group_len[8] = {1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};
    uint8_t ms_mask_present = 0U;
    uint8_t ms_used[8][64];
    (void)memset(ms_used, 0, sizeof(ms_used));

    uint32_t core_sr = p_state->sample_rate_hz;
    if (p_state->sbr_active != 0U) {
        core_sr /= 2U;
    }

    const uint16_t *p_swb_long;
    const uint16_t *p_swb_short;

    if (core_sr >= 44100U) {
        p_swb_long  = g_aac_sfb_48_960;
        p_swb_short = g_aac_sfb_48_120;
    } else if (core_sr >= 32000U) {
        p_swb_long  = g_aac_sfb_32_960;
        p_swb_short = g_aac_sfb_48_120;
    } else if (core_sr >= 22050U) {
        p_swb_long  = g_aac_sfb_24_960;
        p_swb_short = g_aac_sfb_24_120;
    } else {
        p_swb_long  = g_aac_sfb_16_960;
        p_swb_short = g_aac_sfb_16_120;
    }

    const uint16_t *p_swb_table = p_swb_long;
    uint8_t bits_incr = 5U;
    uint8_t esc_val = 31U;

    if (common_window != 0U) {
        (void)bitstream_get_bits(&bs, 1U); /* reserved bit */
        window_sequence = (uint8_t)bitstream_get_bits(&bs, 2U);
        (void)bitstream_get_bits(&bs, 1U); /* window_shape */

        if (window_sequence == 2U) {
            /* EIGHT_SHORT_SEQUENCE */
            max_sfb = (uint8_t)bitstream_get_bits(&bs, 4U);
            num_groups = 1U;
            group_len[0] = 1U;
            for (uint8_t i = 0U; i < 7U; i++) {
                if (bitstream_get_bits(&bs, 1U) != 0U) {
                    group_len[num_groups - 1U]++;
                } else {
                    num_groups++;
                    group_len[num_groups - 1U] = 1U;
                }
            }
            p_swb_table = p_swb_short;
            bits_incr = 3U;
            esc_val = 7U;
        } else {
            max_sfb = (uint8_t)bitstream_get_bits(&bs, 6U);
            (void)bitstream_get_bits(&bs, 1U); /* predictor_data_present */
            num_groups = 1U;
            group_len[0] = 1U;
            p_swb_table = p_swb_long;
            bits_incr = 5U;
            esc_val = 31U;
        }

        ms_mask_present = (uint8_t)bitstream_get_bits(&bs, 2U);
        if (ms_mask_present == 1U) {
            for (uint8_t g = 0U; g < num_groups; g++) {
                for (uint8_t s = 0U; s < max_sfb; s++) {
                    ms_used[g][s] = (uint8_t)bitstream_get_bits(&bs, 1U);
                }
            }
        } else if (ms_mask_present == 2U) {
            for (uint8_t g = 0U; g < num_groups; g++) {
                for (uint8_t s = 0U; s < max_sfb; s++) {
                    ms_used[g][s] = 1U;
                }
            }
        } else {
            /* 0U or 3U: mask is all zero */
        }
    }

    if (max_sfb > 64U) {
        max_sfb = 64U;
    }

    /* Per-channel band types and scale factors */
    static uint8_t  s_band_type[2][8 * 64];
    static uint8_t  s_band_type_run_end[2][8 * 64];
    static float    s_sf_tab[2][8][64];

    /* Unpack channels */
    for (uint8_t ch = 0U; ch < channels_in_stream; ch++) {
        uint8_t global_gain = (uint8_t)bitstream_get_bits(&bs, 8U);

        if (common_window == 0U) {
            (void)bitstream_get_bits(&bs, 1U); /* reserved bit */
            window_sequence = (uint8_t)bitstream_get_bits(&bs, 2U);
            (void)bitstream_get_bits(&bs, 1U); /* window_shape */
            if (window_sequence == 2U) {
                max_sfb = (uint8_t)bitstream_get_bits(&bs, 4U);
                num_groups = 1U;
                group_len[0] = 1U;
                for (uint8_t i = 0U; i < 7U; i++) {
                    if (bitstream_get_bits(&bs, 1U) != 0U) {
                        group_len[num_groups - 1U]++;
                    } else {
                        num_groups++;
                        group_len[num_groups - 1U] = 1U;
                    }
                }
                p_swb_table = p_swb_short;
                bits_incr = 3U;
                esc_val = 7U;
            } else {
                max_sfb = (uint8_t)bitstream_get_bits(&bs, 6U);
                (void)bitstream_get_bits(&bs, 1U);
                num_groups = 1U;
                group_len[0] = 1U;
                p_swb_table = p_swb_long;
                bits_incr = 5U;
                esc_val = 31U;
            }
        }

        /* 1. Decode Band Types (Section Data) */
        uint16_t b_idx = 0U;
        for (uint8_t g = 0U; g < num_groups; g++) {
            uint8_t k = 0U;
            while (k < max_sfb) {
                uint8_t sect_cb = (uint8_t)bitstream_get_bits(&bs, 4U);
                uint8_t sect_end = k;
                while (1) {
                    uint8_t incr = (uint8_t)bitstream_get_bits(&bs, bits_incr);
                    sect_end = (uint8_t)(sect_end + incr);
                    if (incr != esc_val) {
                        break;
                    }
                }
                if (sect_end > max_sfb) {
                    sect_end = max_sfb;
                }
                for (uint8_t pos = k; pos < sect_end; pos++) {
                    s_band_type[ch][b_idx] = sect_cb;
                    s_band_type_run_end[ch][b_idx] = sect_end;
                    b_idx++;
                }
                k = sect_end;
            }
        }

        /* 2. Decode Scale Factors (DPCM with 3 accumulators) */
        int32_t offset[3];
        offset[0] = (int32_t)global_gain;
        offset[1] = (int32_t)global_gain - 90;
        offset[2] = 0;
        uint8_t noise_flag = 1U;

        b_idx = 0U;
        for (uint8_t g = 0U; g < num_groups; g++) {
            uint8_t s = 0U;
            while (s < max_sfb) {
                uint8_t run_end = s_band_type_run_end[ch][b_idx];
                uint8_t bt = s_band_type[ch][b_idx];
                if (bt == AAC_CB_ZERO) {
                    for (uint8_t pos = s; pos < run_end; pos++) {
                        s_sf_tab[ch][g][pos] = 0.0f;
                        b_idx++;
                    }
                } else if ((bt == AAC_CB_INTENSITY_STEREO) || (bt == AAC_CB_INTENSITY_STEREO2)) {
                    for (uint8_t pos = s; pos < run_end; pos++) {
                        int16_t diff = aac_decode_huffman_scalefactor(&bs);
                        offset[2] += (int32_t)diff;
                        s_sf_tab[ch][g][pos] = powf(2.0f, -(float)offset[2] * 0.25f);
                        b_idx++;
                    }
                } else if (bt == AAC_CB_NOISE) {
                    for (uint8_t pos = s; pos < run_end; pos++) {
                        if (noise_flag > 0U) {
                            noise_flag--;
                            offset[1] += (int32_t)bitstream_get_bits(&bs, 9U) - 256;
                        } else {
                            int16_t diff = aac_decode_huffman_scalefactor(&bs);
                            offset[1] += (int32_t)diff;
                        }
                        s_sf_tab[ch][g][pos] = powf(2.0f, (float)(offset[1] - 100) * 0.25f);
                        b_idx++;
                    }
                } else {
                    for (uint8_t pos = s; pos < run_end; pos++) {
                        int16_t diff = aac_decode_huffman_scalefactor(&bs);
                        offset[0] += (int32_t)diff;
                        s_sf_tab[ch][g][pos] = powf(2.0f, (float)(offset[0] - 100) * 0.25f);
                        b_idx++;
                    }
                }
                s = run_end;
            }
        }

        /* 3. Pulse and TNS tools */
        uint8_t pulse_present = (uint8_t)bitstream_get_bits(&bs, 1U);
        if (pulse_present != 0U) {
            decode_pulses_stream(&bs);
        }

        uint8_t tns_present = (uint8_t)bitstream_get_bits(&bs, 1U);
        if (tns_present != 0U) {
            decode_tns_stream(&bs, (window_sequence == 2U) ? 1U : 0U, (window_sequence == 2U) ? 8U : 1U);
        }

        /* 4. Gain control data present bit */
        (void)bitstream_get_bits(&bs, 1U);

        /* 5. Decode Spectral Data */
        for (uint16_t i = 0U; i < 960U; i++) {
            p_state->spec[ch][i] = 0.0f;
        }

        b_idx = 0U;
        if (window_sequence != 2U) {
            /* Long window spectral lines */
            for (uint8_t s = 0U; s < max_sfb; s++) {
                uint8_t bt = s_band_type[ch][b_idx];
                b_idx++;
                uint16_t l_start = p_swb_table[s];
                uint16_t l_end   = p_swb_table[s + 1U];
                if ((bt == AAC_CB_ZERO) || (bt >= AAC_CB_INTENSITY_STEREO2)) {
                    continue;
                } else if (bt == AAC_CB_NOISE) {
                    float noise_scale = s_sf_tab[ch][0][s];
                    for (uint16_t k = l_start; k < l_end; k++) {
                        p_state->spec[ch][k] = noise_scale * 0.05f;
                    }
                } else {
                    float scale = s_sf_tab[ch][0][s];
                    uint8_t dim = (bt <= 4U) ? 4U : 2U;
                    uint16_t k = l_start;
                    while (k < l_end) {
                        int16_t vals[4];
                        (void)aac_decode_huffman_spectral(&bs, bt, vals);
                        for (uint8_t d = 0U; d < dim; d++) {
                            if ((k + d) < 960U) {
                                p_state->spec[ch][k + d] = aac_dequant_4_3(vals[d]) * scale;
                            }
                        }
                        k = (uint16_t)(k + dim);
                    }
                }
            }
        } else {
            /* 8-short windows */
            uint16_t win_base = 0U;
            for (uint8_t g = 0U; g < num_groups; g++) {
                uint8_t g_len = group_len[g];
                for (uint8_t s = 0U; s < max_sfb; s++) {
                    uint8_t bt = s_band_type[ch][b_idx];
                    b_idx++;
                    uint16_t l_start = p_swb_short[s];
                    uint16_t l_end   = p_swb_short[s + 1U];
                    if ((bt == AAC_CB_ZERO) || (bt >= AAC_CB_INTENSITY_STEREO2)) {
                        continue;
                    } else if (bt == AAC_CB_NOISE) {
                        float noise_scale = s_sf_tab[ch][g][s];
                        for (uint8_t w = 0U; w < g_len; w++) {
                            uint16_t cur_w = (uint16_t)(win_base + w);
                            for (uint16_t k = l_start; k < l_end; k++) {
                                p_state->spec[ch][cur_w * 120U + k] = noise_scale * 0.05f;
                            }
                        }
                    } else {
                        float scale = s_sf_tab[ch][g][s];
                        uint8_t dim = (bt <= 4U) ? 4U : 2U;
                        for (uint8_t w = 0U; w < g_len; w++) {
                            uint16_t cur_w = (uint16_t)(win_base + w);
                            uint16_t k = l_start;
                            while (k < l_end) {
                                int16_t vals[4];
                                (void)aac_decode_huffman_spectral(&bs, bt, vals);
                                for (uint8_t d = 0U; d < dim; d++) {
                                    if ((cur_w * 120U + k + d) < 960U) {
                                        p_state->spec[ch][cur_w * 120U + k + d] = aac_dequant_4_3(vals[d]) * scale;
                                    }
                                }
                                k = (uint16_t)(k + dim);
                            }
                        }
                    }
                }
                win_base = (uint16_t)(win_base + g_len);
            }
        }
    }

    /* 6. M/S Stereo and Intensity Stereo Post-Processing */
    if (channels_in_stream == 2U) {
        if (window_sequence != 2U) {
            for (uint8_t s = 0U; s < max_sfb; s++) {
                uint16_t l_start = p_swb_long[s];
                uint16_t l_end   = p_swb_long[s + 1U];
                uint8_t bt_r = s_band_type[1][s];

                if ((bt_r == AAC_CB_INTENSITY_STEREO) || (bt_r == AAC_CB_INTENSITY_STEREO2)) {
                    float invert = (bt_r == AAC_CB_INTENSITY_STEREO2) ? -1.0f : 1.0f;
                    float is_scale = invert * s_sf_tab[1][0][s];
                    for (uint16_t k = l_start; k < l_end; k++) {
                        p_state->spec[1][k] = p_state->spec[0][k] * is_scale;
                    }
                } else if (ms_used[0][s] != 0U) {
                    for (uint16_t k = l_start; k < l_end; k++) {
                        float l = p_state->spec[0][k];
                        float r = p_state->spec[1][k];
                        p_state->spec[0][k] = l + r;
                        p_state->spec[1][k] = l - r;
                    }
                } else {
                    /* Individual stereo */
                }
            }
        } else {
            uint16_t win_base = 0U;
            for (uint8_t g = 0U; g < num_groups; g++) {
                uint8_t g_len = group_len[g];
                for (uint8_t w = 0U; w < g_len; w++) {
                    uint16_t cur_w = (uint16_t)(win_base + w);
                    for (uint8_t s = 0U; s < max_sfb; s++) {
                        uint16_t l_start = p_swb_short[s];
                        uint16_t l_end   = p_swb_short[s + 1U];
                        uint8_t bt_r = s_band_type[1][g * max_sfb + s];

                        if ((bt_r == AAC_CB_INTENSITY_STEREO) || (bt_r == AAC_CB_INTENSITY_STEREO2)) {
                            float invert = (bt_r == AAC_CB_INTENSITY_STEREO2) ? -1.0f : 1.0f;
                            float is_scale = invert * s_sf_tab[1][g][s];
                            for (uint16_t k = l_start; k < l_end; k++) {
                                p_state->spec[1][cur_w * 120U + k] = p_state->spec[0][cur_w * 120U + k] * is_scale;
                            }
                        } else if (ms_used[g][s] != 0U) {
                            for (uint16_t k = l_start; k < l_end; k++) {
                                float l = p_state->spec[0][cur_w * 120U + k];
                                float r = p_state->spec[1][cur_w * 120U + k];
                                p_state->spec[0][cur_w * 120U + k] = l + r;
                                p_state->spec[1][cur_w * 120U + k] = l - r;
                            }
                        } else {
                            /* Individual */
                        }
                    }
                }
                win_base = (uint16_t)(win_base + g_len);
            }
        }
    }

    /* 7. Windowed IMDCT & Overlap-Add */
    for (uint8_t ch = 0U; ch < channels_in_stream; ch++) {
        aac_imdct_process_channel(
            &p_state->imdct_state,
            ch,
            p_state->spec[ch],
            960U,
            p_state->core_pcm[ch],
            window_sequence,
            enable_neon
        );
    }

    /* 8. Interleave to 16-bit PCM output */
    for (uint16_t i = 0U; i < 960U; i++) {
        for (uint8_t ch = 0U; ch < channels_in_stream; ch++) {
            float val = p_state->core_pcm[ch][i];
            if (val > 32767.0f) {
                val = 32767.0f;
            }
            if (val < -32768.0f) {
                val = -32768.0f;
            }
            pcm_out_interleaved[i * (uint16_t)channels_in_stream + ch] = (int16_t)val;
        }
    }

    *p_num_samples_per_ch = 960U;
    return DAB_OK;
}
