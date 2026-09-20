# Real DAB/DAB+ AU Test Integration & Action Plan
### Project Planning & Verification Next Steps

---

## 1. Production-Quality AU Decoding Readiness

The complete production-grade decoding engine for both DAB broadcast formats is fully implemented and compiled into `libdab_decoder.a`:

### 1.1 DAB Classic (MPEG-1 / MPEG-2 Layer II MUSICAM)
* **Standards:** ISO/IEC 11172-3 & ISO/IEC 13818-3.
* **Capabilities:** 
  * 32-subband polyphase synthesis matrixing.
  * Normative C/D dequantization tables and scale factor unpacking.
  * Intensity Joint Stereo (intensity subband boundary `jsbound` 4, 8, 12, 16).
  * Dual Channel (independent channels) and Mono downmix.
  * 48 kHz standard rate (1,152 samples/AU) and 24 kHz half-rate LSF (576 samples/AU).

### 1.2 DAB+ Digital Radio (HE-AAC v2)
* **Standards:** ETSI TS 102 563 & ISO/IEC 14496-3.
* **Capabilities:** 
  * Mandatory **960-sample transform window** (sine and KBD windows).
  * 11 normative Huffman codebooks (CB1–CB11).
  * Bounded escape sequence reader ($N \le 20$) preventing worst-case execution time runaway.
  * Fast Goertzel recursive rotation recurrence IMDCT (sub-millisecond latency).
  * 32-to-64 band QMF upsampling SBR (high-frequency reconstruction).
  * Parametric Stereo (PS) all-pass decorrelator and spatializer.

### 1.3 Automotive Protection & Error Mitigation (MS-3 & MS-4)
* **ETSI EN 300 401 §12.2 CRC-16/CCITT:** Checksum validated on every received AU.
* **4-Stage Concealment FSM:** `NONE` $\to$ `INTERPOLATE` $\to$ `ATTENUATE` $\to$ `MUTED`.
* **Soft Mute Gain Ramping:** Linear, 128-entry Cosine LUT, and Exponential curve profiles.
* **Pop Prevention:** Raised-cosine crossfade on signal return eliminating clicks and pops ($|\Delta S| \le 120$).
* **Asymmetric Audio Quality Index (AQI 0..100):** Fast-Drop to 0 upon loss, Slow-Rise recovery (`+2/frame`).
* **2-Bit Blending Hardware Triggers:**
  * `0b00` (`DAB_TRIGGER_IDLE`): Healthy playback.
  * `0b01` (`DAB_TRIGGER_CONCEAL`): Loss detected; vehicle audio manager prepares for FM/IP blending.
  * `0b10` (`DAB_TRIGGER_UNRECOVERABLE`): Hard mute; vehicle immediately switches audio source to FM/IP.

---

## 2. Expected Changes When Real DAB AU Files Arrive Next Week

### 2.1 The Good News: Zero Core Code Changes
The core decoder engine (`libdab_decoder.a`) requires **ZERO code changes**. 

The engine operates exclusively on memory buffers via its frozen public API:
```c
int32_t DAB_Decoder_DecodeAU(
    DAB_Decoder_Handle       handle,
    const uint8_t           *au_data,        /* Pointer to raw AU payload + 2-byte CRC */
    uint16_t                 au_len,         /* Length in bytes */
    const DAB_SignalStatus  *signal_status,  /* Baseband status or NULL */
    int16_t                 *pcm_out,        /* Output buffer for 16-bit stereo PCM */
    DAB_AudioStatus         *status_out      /* Telemetry: AQI, triggers, mute state */
);
```
It has zero dependencies on disk formats, operating system files, or hardcoded stream names.

---

## 3. How to Minimize Code Changes & Avoid Errors

To avoid human errors, recompilation overhead, or file mismatch issues when ingesting real AU data:

### 3.1 Avoid Hardcoded File Names (Use CLI Arguments)
The test harness CLI (`dab_test_harness.exe`) is designed to take any file path at runtime. **No C code needs to be modified or recompiled**:
```bash
./harness/dab_test_harness.exe \
    --input "test_streams/real_captures/station_name.au" \
    --codec aac \
    --sample-rate 48000 \
    --wav "test_streams/real_captures/station_name.wav" \
    --log "test_streams/real_captures/station_name.log"
```

### 3.2 Confirm AU Container Framing with the RF / Baseband Team
When the baseband hardware team extracts AU bitstream files from the tuner receiver, confirm which container format they are exporting:

#### Format A: Standard Framed AU File (Supported Out-of-the-Box)
Each AU in the `.au` file is preceded by a 4-byte big-endian length prefix:
```text
[4-byte Big-Endian Length N] [N bytes AU payload ending with 2-byte CRC]
[4-byte Big-Endian Length M] [M bytes AU payload ending with 2-byte CRC]
...
```
* **Status:** This is the exact format parsed by `dab_test_harness.exe`. If the baseband team provides this format, **it works immediately with zero modifications**.

#### Format B: Raw Continuous Bitstream (No Length Prefixes)
The file is a raw byte dump where AUs appear back-to-back:
* For MP2: Packets start with sync word `0xFFF` and packet length is derived from the header bitrate/samplerate formula:
  $$\text{Length} = 144 \times \frac{\text{Bitrate}}{\text{SampleRate}} + \text{Padding}$$
* For DAB+: Packets are packaged in ETSI TS 102 563 superframes starting with standard firecode headers.
* **Action if needed:** If the baseband team exports Format B, a lightweight ingest reader can be activated in the harness without touching `libdab_decoder.a`.

---

## 4. Recommended Directory Structure for Next Week's Test Vectors

Create a clean folder dedicated to physical RF captures:
```text
test_streams\real_captures\
├── README.md                           # Metadata log (station name, ensemble, SNR, location)
├── bbc_radio1_dab_plus.au              # Real on-air DAB+ stream (HE-AAC v2 @ 48kHz)
├── classic_fm_dab_plus.au              # Real on-air classical music stream
├── absolute_radio_dab_classic.au       # Real on-air DAB classic stream (MP2 @ 48kHz)
└── tunnel_dropout_scenario.au          # Real RF capture driving into a tunnel (error burst)
```

---

## 5. Step-by-Step Test Procedure for Real Signals

When the files arrive next week, execute the following steps:

1. **Place Incoming Captures:** Copy the extracted `.au` files into `test_streams/real_captures/`.
2. **Execute Ingest & Decoding:**
   * **For DAB+ Broadcasts:**
     ```bash
     ./harness/dab_test_harness.exe \
         --input test_streams/real_captures/bbc_radio1_dab_plus.au \
         --codec aac --sample-rate 48000 \
         --wav test_streams/real_captures/bbc_radio1.wav \
         --log test_streams/real_captures/bbc_radio1.log
     ```
   * **For DAB Classic Broadcasts:**
     ```bash
     ./harness/dab_test_harness.exe \
         --input test_streams/real_captures/absolute_radio_dab_classic.au \
         --codec mp2 --sample-rate 48000 \
         --wav test_streams/real_captures/absolute_radio.wav \
         --log test_streams/real_captures/absolute_radio.log
     ```
3. **Auditory & Telemetry Verification:**
   * Open the generated `.wav` file in VLC or Audacity: Verify clear, studio-quality speech or music playback.
   * Open the generated `.log` file: Verify that on-air RF fading correctly triggers the AQI score drop and blending flags (`0b01` or `0b10`).

---

## 6. Action Items Checklist for Next Week

- [ ] Confirm with the baseband extraction team whether they export **Format A (4-byte length prefix)** or **Format B (raw bitstream)**.
- [ ] Receive sample capture files for DAB Classic (MP2) and DAB+ (HE-AAC v2).
- [ ] Run `dab_test_harness.exe` on sample captures and produce `.wav` and `.log`.
- [ ] Perform listening tests on studio speech and complex music passages.
- [ ] Validate seamless blending triggers against real RF tunnel / multipath fade events.
