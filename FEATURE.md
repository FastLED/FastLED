Design Doc — Integrate Helix MP3 (fixed-point) decoder into FastLED (src/third_party/)
1) Objective

Add a small, fast fixed-point MP3 decoder to FastLED so sketches can decode MP3 frames on-device (e.g., from SD/stream) and feed PCM to audio-reactive effects (RMS, FFT, onset/beat, pitch) or to MIDI extractors. The integration should be vendorable, license-compliant, and easy to use from Arduino and ESP-IDF builds.

Chosen decoder: Helix MP3 (fixed-point) — widely used on MCUs, fast, predictable memory, proven on ESP32, RP2040, and Cortex-M. 
GitHub
+2
GitHub
+2

GitHub references:

Primary: chmorgan/libhelix-mp3 (fixed-point decoder) 
GitHub

ESP-IDF component (wrapper): chmorgan/esp-libhelix-mp3 
GitHub
+1

Arduino integration ideas (uses Helix under the hood): earlephilhower/ESP8266Audio 
GitHub

Licensing note: Helix sources carry RealNetworks licenses (RPSL/RCSL) in the fixpt code; ensure compliance when vendoring. (See Silicon Labs + Microchip app notes reminding to include the license files.) 
Silicon Labs
+1

2) Scope & Non-Goals

In scope

Vendor Helix MP3 (fixed-point) into src/third_party/libhelix_mp3/ with minimal patches.

Provide a tiny C++ façade API (fl::audio::Mp3HelixDecoder) exposing init/decode/reset and yielding 16-bit interleaved PCM.

Offer Arduino (ESP32-S3/S2/S3, ESP32, RP2040) and ESP-IDF build paths.

Examples that decode from SD and memory stream, pushing PCM into I2S or analysis hooks.

Out of scope

MP3 encoder, AAC/FLAC, streaming over HTTP (may come later).

Heavy audio stacks (e.g., esp-adf). We stay lightweight. 
ESP Component Registry

3) Target Platforms & Budgets

ESP32 family (Xtensa & RISC-V variants incl. S3): 240 MHz recommended for 44.1 kHz stereo + effects; RAM ≥ 200 KB free.

RP2040: works (Helix via ESP8266Audio shows viability); buffer sizes tuned smaller. 
GitHub
+1

AVR: not targeted (flash/RAM too tight for real-time MP3).

Typical Helix footprint (varies by config/toolchain): small flash (~tens of KB) and modest RAM; used broadly on Cortex-M/PIC32 by vendors. 
Silicon Labs
+1

4) Repository Layout (proposed)
FastLED/
└─ src/
   ├─ third_party/
   │  └─ libhelix_mp3/
   │     ├─ LICENSE/               # include RPSL/RCSL/license files verbatim
   │     ├─ helix/…                # decoder sources (fixpt)
   │     ├─ CMakeLists.txt         # minimal; used by ESP-IDF builds
   │     └─ helix_config.h         # our compile-time toggles
   ├─ audio/
   │  ├─ mp3_helix_decoder.h       # C++ façade (public)
   │  └─ mp3_helix_decoder.cpp     # adapter to Helix C API
   └─ fx/…                         # (existing FastLED)
examples/
└─ Audio/
   ├─ Mp3ToI2S_Minimal/            # decode -> I2S out
   ├─ Mp3ToAnalysis_RMS_FFT/       # decode -> analysis callbacks
   └─ Mp3Stream_SD/                # decode from SD/file

5) Build System Integration
5.1 Arduino (PlatformIO & Arduino IDE)

CMake is not required; compile Helix C sources alongside FastLED.

Add library.properties/library.json updates if needed so Arduino/PIO picks up src/third_party/libhelix_mp3/helix/*.c.

Default flags:

-Os -ffunction-sections -fdata-sections

-DHELIx_FIXEDPOINT=1 (build fixpt only)

Disable unused features in helix_config.h (no CRC, minimal tables).

ESP32: enable I2S; recommend CONFIG_FREERTOS_UNICORE=n (dual-core OK) and 240 MHz for headroom.

5.2 ESP-IDF

Option A (vendor only): compile vendored Helix directly (our CMakeLists.txt).

Option B (optional path): allow using esp-libhelix-mp3 component instead of vendored copy when -DUSE_IDF_HELIX=ON. 
GitHub
+1

Agent task: detect environment → choose A or B via small cmake/ifdef shim.

6) Public API (façade)
// src/audio/mp3_helix_decoder.h
namespace fl::audio {

struct Mp3Frame {
  const int16_t* pcm;   // interleaved L/R
  int samples;          // samples per channel
  int channels;         // 1 or 2
  int sample_rate;      // Hz
};

class Mp3HelixDecoder {
 public:
  // init with an external scratch buffer to avoid mallocs
  bool init(void* workbuf, size_t workbuf_len);

  // Feed raw MP3 bytes; decode zero or more frames.
  // Returns number of PCM frames produced into the callback.
  template <typename Fn>
  int decode(const uint8_t* data, size_t len, Fn on_frame);

  void reset();
};

} // namespace fl::audio


Design notes

No allocations after init(); the agent sizes workbuf using Helix’s MP3GetBuffersize() (or equivalent).

decode() can be called repeatedly with partial MP3 data; handle ID3 safely (skip/continue). 
GitHub

Output format: 16-bit signed PCM, interleaved, ready for I2S or analysis.

7) Data Flow & Hooks

Sources: SD card, SPIFFS/LittleFS, network stream (future).
Pipeline: Read MP3 bytes → Mp3HelixDecoder.decode() → PCM callback(s)
Consumers (callbacks):

I2S sink (optional helper): writes PCM to DAC/I2S.

Analysis sink: RMS/peak, FFT, onset/beat, pitch detector, MIDI extractor (existing/planned FastLED audio tools).

8) Performance & Buffers (starting points)

MP3 input ring: 2–4 KB (tune per source latency).

PCM output block: 1152 samples per channel (max MP3 frame) → 2304 int16 samples total; double-buffer.

Work buffer: per Helix query; allocate once in global/static arena.

ESP32-S3 @ 240 MHz: should sustain 44.1 kHz stereo with room for analysis; keep I2S DMA at ≥ 2x frame depth.

9) Licensing & Compliance

Vendor Helix license files (RPSL/RCSL/LICENSE) with the copied sources and keep any headers intact. 
Silicon Labs
+1

Our façade code under the FastLED license; clearly separate third-party code in src/third_party/libhelix_mp3/.

Document that decoding patents for MP3 were sunset (2017 notice)—still keep license files per upstream guidance. 
Silicon Labs

10) Testing & QA

Unit tests (IDF Unity / Arduino CI):

Frame decode sanity: sample-rate, channels, frame size.

Corrupt frames & ID3 headers don’t crash; decoder resumes. 
GitHub

Buffer size guards: assert if workbuf too small.

Integration tests:

SD → decode → I2S playback (44.1 kHz stereo).

SD → decode → analysis (RMS + 1024-pt FFT) → verify energy bands stable.

Artifacts:

Include 2–3 short MP3 test vectors (public/redistributable).

11) Example Sketches (deliverables)

Mp3ToI2S_Minimal

#include <FastLED.h>
#include "audio/mp3_helix_decoder.h"
#include "i2s_sink.h"   // tiny helper (examples/)

fl::audio::Mp3HelixDecoder dec;
static uint8_t mp3in[3072];
static int16_t pcmout[2304];

void setup() {
  // mount SD, init I2S, allocate helix workbuf...
}

void loop() {
  size_t n = read_mp3_bytes(mp3in, sizeof(mp3in));
  dec.decode(mp3in, n, [&](const fl::audio::Mp3Frame& f){
    i2s_write_pcm(f.pcm, f.samples * f.channels);
    // also: audio_reactive_update(f.pcm, f.samples, f.sample_rate);
  });
}


Mp3ToAnalysis_RMS_FFT — same but routes PCM to analysis callbacks.

Mp3Stream_SD — shows incremental reads + ID3 skipping.

12) Agent Work Plan (step-by-step)

Vendor Helix

Create src/third_party/libhelix_mp3/ and import fixpt decoder sources + RPSL/RCSL/LICENSE files. 
GitHub
+1

Add helix_config.h to disable unused features; keep build consistent across Arduino/IDF.

Build wiring

Arduino/PIO: ensure .c files are compiled; add minimal guards in src/audio/mp3_helix_decoder.cpp.

ESP-IDF: simple CMakeLists.txt to build vendored sources; optional path to use esp-libhelix-mp3 if USE_IDF_HELIX=ON. 
GitHub

Façade API

Implement Mp3HelixDecoder around Helix C API (MP3InitDecoder, MP3Decode, …).

Expose init(workbuf, len), decode(data,len,callback), reset().

I/O helpers

Minimal i2s_sink.h/.cpp in examples (not core) for ESP32.

SD reader helper for chunked reads.

Examples

Add the three sketches above; confirm on ESP32-S3 (240 MHz).

Tests

Add simple frame decode unit test (IDF Unity).

Add ID3-tag regression (feed file with large ID3v2 header). 
GitHub

Docs

docs/audio/mp3.md: how to enable, memory estimates, example wiring, known limitations (e.g., malformed streams).

CI

Build check for Arduino (ESP32) + IDF.

Size report for core + examples.

13) Risks & Mitigations

License mistakes → Keep Helix in isolated folder with upstream license files and attribution; document clearly. 
Silicon Labs
+1

ID3 edge cases → Add simple ID3 skipper and loop MP3Decode() until sync found (document behavior). 
GitHub

Timing under load → Recommend 240 MHz on ESP32-S3 and DMA-backed I2S; provide buffer sizing guidance.

RAM pressure → Require external work buffer; allow user to size via API.

14) Acceptance Criteria

Buildable on Arduino (ESP32) and ESP-IDF without extra dependencies.

Decode and play a 44.1 kHz stereo MP3 from SD with glitch-free audio for ≥ 60 s.

Provide PCM to analysis callbacks at frame boundaries with correct sample rate and channel count.

Vendored code includes upstream license files and clear separation.

15) Nice-to-Haves (later)

Optional minimp3 backend for users preferring CC0 / single-header (trade-offs in speed).

HTTP streaming example and seamless ID3v2 skip.

Built-in RMS/FFT helper nodes wired to FastLED audio-reactive FX.

Appendix: Upstream Links (for code agent)

Helix decoder (fixed-point): https://github.com/chmorgan/libhelix-mp3 
GitHub

ESP-IDF component: https://github.com/chmorgan/esp-libhelix-mp3 (also on Espressif Component Registry) 
GitHub
+1

Arduino reference implementation using Helix: https://github.com/earlephilhower/ESP8266Audio 
GitHub

License guidance / vendor app notes: Silicon Labs AN1112; Microchip AN1367. 
Silicon Labs
+1

If you want, I can generate the initial PR skeleton (folders, façade, CMake/PIO glue, and one minimal example) ready to drop into github.com/fastled/fastled.