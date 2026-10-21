# ESP32 memory regression #4870

## Cause and fix

The ESP-DSP FFT backend included the umbrella `esp_dsp.h`. In the legacy
ESP32 SDK it includes `mat.h`, which includes `<iostream>`. Its static
`std::ios_base::Init` constructor makes the audio unity object retain C++
stream and locale support even in an LED-only sketch.

Remove the unused experimental ESP-DSP adapter and call FastLED's existing
optimized `kiss_fftr` real transform directly. The removed dispatcher already
unconditionally called `kiss_fftr`; its additional size argument was ignored.
The tested FFT arithmetic, configured numeric precision, windowing, magnitude
batching, constant-Q processing and binning are unchanged. RMT buffering,
network detection, interrupt handling and recovery are also unchanged.

The initial patch narrowed the SDK include to `dsps_fft2r.h`. Following review,
the experimental backend and its dedicated `AudioFftParity` example are removed
entirely, keeping one production FFT implementation. There is no automatic
SDK-dependent implementation switch or additional FFT work/twiddle cache.

## Controlled Lolin D32 comparison

The unchanged 96-pixel sketch from [the reporter's repository](https://github.com/chemdoc77/CD77_FastLED_2026/blob/1cc4929bec905543d224b225603e269c25381bdc/NEOPIXEL%20New%20Product%20Test%20by%20Chemdoc77/src/Neopixel_New_Product_Test.cpp)
was built with fbuild 2.5.42, `board = lolin_d32`,
`platform = espressif32@6.13.0`, Arduino ESP32 2.0.17 / IDF 4.4.7 and
Xtensa GCC 8.4 patch5. All variants use the same sketch, platform and flags;
the staged library is replaced with the corresponding revision.

| Library | Board flash bytes | Static RAM bytes |
| --- | ---: | ---: |
| 3.10.3 (`20667c3a64`) | 253,022 | 22,216 |
| 3.10.8 (`d7402f1294`) | 513,110 | 30,344 |
| Master control (`528a6fdbeb`) | 513,110 | 30,344 |
| Master with the unused ESP-DSP adapter removed | 272,434 | 23,292 |

The fix saves **240,676 bytes flash and 7,052 bytes static RAM** against the
master control. The remaining difference from 3.10.3 is **19,412 bytes flash
and 1,076 bytes static RAM**. This does not restore full size parity.

These are controlled local measurements, not an exact reproduction of the
reporter's build: their resolved SDK and build flags were not supplied, and
their reported absolute sizes differ. Static RAM excludes runtime heap.
No runtime heap saving or physical-device waveform result is claimed.

## Reproducible repository size check

The existing legacy ESP32 Blink target provides an in-repository regression
control without requiring the external sketch or a new workflow:

```bash
bash bloat esp32dev_idf44 --build --budget tests/data/esp32dev_idf44_bloat_budget.json
```

This target uses Arduino ESP32 2.0.3 / IDF 4.4.1 and GCC 8.4 patch3.
Before/after board flash is 473,598 / 232,250 bytes; static RAM is
24,292 / 17,248 bytes. The bloat report separately measures allocated
`image_flash` at 473,613 / 232,265 bytes and attributed `total_ram` at
33,005 / 21,511 bytes. Attributed RAM includes symbol aliases and must not be
reported as physical static RAM. The budget rejects the saved original
report and accepts the fixed report.

SDK builds of the existing production `Audio` example on legacy
`esp32dev_idf44` and modern `esp32dev` validate actual audio-pipeline callers.
The existing FFT golden tests pass before and after the cleanup. No device
speed comparison against ESP-DSP is claimed or needed to preserve the existing
implementation; the optimized real transform and its callers' math are unchanged.
The examples were not run on hardware. No production functionality or
diagnostics are disabled.

Full `bash lint` passed. `bash test --cpp --debug` passed 321 unit suites and
95 examples with sanitizers. Example inventory tests (18 cases) and the
lint-baseline tests (3 cases) also passed. Earlier quick-mode runs were rejected by the
runner's stale-artifact guard before assertions; the full debug build and
run above completed successfully. Independent repository code review found
no issues.

Local build logs, the measurement harness and saved before/after reports are
retained under `.build/4870-evidence/` and `.build/4870_measure.py`.
