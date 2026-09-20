# Test Streams & Real Broadcast Audio Guide

### Automotive DAB / DAB+ Audio Decoder (Milestones MS-1 to MS-4)

---

## 1. Overview of DAB / DAB+ Test Streams

In the digital radio industry (ETSI EN 300 401 and ETSI TS 102 563), the audio signal received by the automotive tuner is delivered as a sequence of **pure Audio Units (AUs)**:

- **DAB Classic (MPEG-1 / MPEG-2 Layer II):** Each AU contains the MPEG audio header, subband bit allocation, scale factors, and subband samples, ending with a 2-byte ETSI CRC-16/CCITT.
- **DAB+ (HE-AAC v2):** Each AU is packaged with a 960-sample transform window core, SBR envelope data, Parametric Stereo parameters, and a 2-byte CRC.

---



## 2. Pre-Generated Reference Streams (In `test_streams/`)

The delivery package already includes **6 standards-compliant reference** `.au` **bitstream files** designed for automated CI/CD and verification:


| Stream File Name               | Codec Engine    | Sample Rate | Frames | Test Purpose                                                         |
| ------------------------------ | --------------- | ----------- | ------ | -------------------------------------------------------------------- |
| `clean_mp2_48k_stereo.au`      | MPEG-1 Layer II | 48,000 Hz   | 1,000  | Baseline continuous DAB Classic stereo decoding (~24 sec)            |
| `clean_mp2_24k_mono.au`        | MPEG-2 LSF      | 24,000 Hz   | 500    | Half-rate mono decoding (576 samples/AU)                             |
| `clean_aac_48k_stereo.au`      | HE-AAC v2       | 48,000 Hz   | 500    | Baseline DAB+ 960-window core with SBR and PS                        |
| `error_injected_single_crc.au` | MPEG-1 Layer II | 48,000 Hz   | 300    | Isolated 1-frame CRC failure (verifies `INTERPOLATE` mode)           |
| `error_injected_burst5.au`     | MPEG-1 Layer II | 48,000 Hz   | 400    | 5 consecutive lost frames (verifies `ATTENUATE` mode)                |
| `scenario_channel_switch.au`   | MPEG-1 Layer II | 48,000 Hz   | 200    | Dynamic automotive error mitigation (Good $\to$ Mute $\to$ Pop-Free) |


---



## 3. Where Real On-Air DAB / DAB+ Broadcast Streams Are Available

In commercial broadcasting, radio stations transmit full multiplex ensembles. The raw transmissions are captured either as **ETI** (Ensemble Transport Interface) files or raw **SDR I/Q baseband recordings** (`.raw` / `.iq`), from which the Audio Units (`.au`) are extracted.

### 3.1 WorldDAB Official Test Stream Library

- **Official Website:** [WorldDAB ETI & Test Library](https://www.worlddab.org/)
- **Languages Available:** **English** (BBC, Commercial UK ensembles), **German** (Bayerischer Rundfunk), French, Scandinavian networks.
- **Stream Profiles:**
  - DAB Classic: 192 kbps / 128 kbps stereo @ 48 kHz.
  - DAB+: 32 kbps to 96 kbps HE-AAC v2 @ 48 kHz (with SBR and Parametric Stereo).
  - Automotive dynamic scenarios: Bitrate switching, dynamic label plus, announcement switching, and severe RF fading events.



### 3.2 OpenDigitalRadio (ODR) Community Archives

- **Repositories:** [OpenDigitalRadio GitHub](https://github.com/Opendigitalradio) & [ODR Wiki](https://wiki.opendigitalradio.org/)
- **Key Software & Captures:**
  - `dab-scripts` **&** `mmbtools`**:** Open multiplex definitions and reference files.
  - `etisnoop`**:** Terminal utility to inspect, parse, and extract AU frames from `.eti` recordings.
  - `dablin`**:** Open-source digital radio receiver for demuxing and playing raw DAB/DAB+ streams.



### 3.3 All India Radio (AIR / Prasar Bharati) Live Broadcasts (Hindi & Telugu)

- **On-Air Operations in India:** All India Radio (Prasar Bharati) operates high-power digital radio transmitters across major metropolitan regions broadcasting in **Hindi** and **Telugu**:
  - **Delhi & Northern India (Hindi):** AIR National, Vividh Bharati, AIR Gold, AIR Rainbow, FM Rainbow.
  - **Hyderabad & Telangana / Andhra Pradesh (Telugu):** AIR Hyderabad, Vividh Bharati Telugu, regional news services.
  - **Bangalore, Chennai, Mumbai:** Regional state services + Hindi + English services.
- **How Teams Capture Real On-Air AU Streams in India:**
Using an inexpensive **RTL-SDR USB receiver** (~₹2,000 / $25) connected to a laptop:
  1. Connect a VHF antenna tuned to Band III (Channels 11B/12B, 218–225 MHz).
  2. Launch the open-source receiver software **[welle.io](https://www.welle.io/)** or `rtl_sdr`.
  3. Select the desired station (e.g. Vividh Bharati in Hindi or AIR Hyderabad in Telugu).
  4. Click **"Record Stream / Dump Service"** to capture the live broadcast directly to a local `.au` or `.eti` file.

---



## 4. How to Generate Compliant `.au` Files from Any Audio (English, Hindi, Telugu)

If you have existing WAV or MP3 files (e.g. a Hindi Bollywood song, Telugu news broadcast, or English interview), you can convert them into standard DAB+ `.au` files immediately using `ODR-AudioEnc` (the reference broadcast encoder maintained by OpenDigitalRadio).

### Step 1: Install `odr-audioenc`

- Available for Windows and Linux from [OpenDigitalRadio ODR-AudioEnc GitHub](https://github.com/Opendigitalradio/ODR-AudioEnc).

Commands on WSL terminal are as follows:

Open WSL as an administrator and run below commands

```
git clone [https://github.com/Opendigitalradio/ODR-AudioEnc.git](https://github.com/Opendigitalradio/ODR-AudioEnc.git) 

cd ODR-AudioEnc 

./bootstrap 

./configure --enable-alsa --enable-vlc # Tweak backends as needed 

make 

sudo make install # remember the password
```

Once you run `sudo make install`, the compiled `odr-audioenc` binary is automatically copied into the default Linux system binary directory: /usr/local/bin/odr-audioenc

Reconfirm using below commands alon with version

```
which odr-audioenc

odr-audioenc --version

```

Now, **Install ffmpeg** in WSL if you don't have it:

```
sudo apt update && sudo apt install -y ffmpeg # remember the password
```

**Record a 30-second clip** from the live BBC World Service stream:

```
ffmpeg -i "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service" -t 30 -ar 48000 -ac 2 bbc_english.wav

```

### Step 2: Encode Audio to Standards-Compliant DAB+ AU Bitstreams

```bash

odr-audioenc -i "bbc_english.wav" -o "english_dab_plus.au" -b 80 -r 48000 -c 2

# Encode a Hindi audio clip into a 48kHz DAB+ AU stream:
odr-audioenc -i "hindi_vividh_bharati.wav" -o "test_streams/hindi_dab_plus.au" -b 48 -r 48000 -c 2

# Encode a Telugu speech/music clip:
odr-audioenc -i "telugu_broadcast.wav" -o "test_streams/telugu_dab_plus.au" -b 64 -r 48000 -c 2

# Encode an English speech/news clip:
odr-audioenc -i "bbc_english_news.wav" -o "test_streams/english_dab_plus.au" -b 80 -r 48000 -c 2
```



### Parameter Reference:

- `-i`: Path to input 16-bit PCM WAV file.
- `-o`: Path to output `.au` bitstream file.
- `-b`: Target DAB+ bit rate in kbps (32, 48, 64, 80, 96).
- `-r`: Audio sampling rate (`48000` or `32000`).
- `-c`: Channels (`2` for stereo, `1` for mono).

---



## 5. Decoding Real `.au` Files with the Test Harness CLI

Once you have acquired or encoded a real `.au` file, process it through the test harness:

### 5.1 Decoding a Real DAB+ Stream (Hindi / Telugu / English):

```bash
./harness/dab_test_harness.exe \
    --input "test_streams/hindi_dab_plus.au" \
    --codec aac \
    --sample-rate 48000 \
    --wav "test_streams/out_hindi.wav" \
    --log "test_streams/out_hindi.log"
```



### 5.2 Decoding a Real DAB Classic (MP2) Stream:

```bash
./harness/dab_test_harness.exe \
    --input "test_streams/english_classic.au" \
    --codec mp2 \
    --sample-rate 48000 \
    --wav "test_streams/out_english.wav" \
    --log "test_streams/out_english.log"
```



### 5.3 What You Will Experience:

1. **Auditory Output (**`.wav`**):** Open `out_hindi.wav` in VLC Media Player to hear the crystal-clear broadcast speech and music.
2. **Telemetry Log (**`.log`**):** Open `out_hindi.log` to inspect real on-air signal quality, frame status (`GOOD`), AQI score (rising smoothly to 100), and hardware blending triggers (`0b00`).

---



## 6. Summary Resource Table


| Resource                   | Description                             | Supported Languages                | Format                      | Link / Acquisition                                                                           |
| -------------------------- | --------------------------------------- | ---------------------------------- | --------------------------- | -------------------------------------------------------------------------------------------- |
| **WorldDAB Library**       | Official European reference captures    | English, German, French            | `.eti` / `.au`              | [worlddab.org](https://www.worlddab.org/)                                                    |
| **AIR Live Transmissions** | Prasar Bharati live on-air transmitters | Hindi, Telugu, English             | Live RF Band III            | Capture with RTL-SDR + `welle.io`                                                            |
| **ODR-AudioEnc**           | Self-encode any WAV/MP3 clip            | Any language (Hindi, Telugu, etc.) | `.au`                       | [github.com/Opendigitalradio/ODR-AudioEnc](https://github.com/Opendigitalradio/ODR-AudioEnc) |
| **Test Harness CLI**       | Automotive decoder executable           | All standards-compliant AUs        | `.au` $\to$ `.wav` + `.log` | `harness/dab_test_harness.exe`                                                               |


