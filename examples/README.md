# FastLED Examples (`examples/`)

This document maps the example sketches, shows how to run them on different targets (Arduino/PlatformIO, Teensy, ESP32, WASM), and suggests learning paths. It mirrors the structure and tone of `src/fl/README.md` so you can quickly jump between concepts and runnable code.

## Table of Contents

- [Overview and Quick Start](#overview-and-quick-start)
- [How to Run Examples](#how-to-run-examples)
  - [Arduino IDE (classic workflow)](#arduino-ide-classic-workflow)
  - [PlatformIO (boards and native host)](#platformio-boards-and-native-host)
  - [Teensy/OctoWS2811](#teensyoctows2811)
  - [ESP32 / I2S (parallel output)](#esp32--i2s-parallel-output)
  - [WASM (browser demos + JSON UI)](#wasm-browser-demos--json-ui)
- [Directory Map (by theme)](#directory-map-by-theme)
  - [Basics and core idioms](#basics-and-core-idioms)
  - [Color, palettes, and HSV](#color-palettes-and-hsv)
  - [Classic 1D effects](#classic-1d-effects)
  - [2D, matrices, mapping](#2d-matrices-mapping)
  - [FX engine and higher-level utilities](#fx-engine-and-higher-level-utilities)
  - [Audio and reactive demos](#audio-and-reactive-demos)
  - [Storage, SD card, and data](#storage-sd-card-and-data)
  - [Multiple strips, parallel, and high-density](#multiple-strips-parallel-and-high-density)
  - [ESP/Teensy/SmartMatrix specifics](#espteensysmartmatrix-specifics)
  - [WASM and UI](#wasm-and-ui)
  - [Larger projects and showcases](#larger-projects-and-showcases)
- [Quick Usage Notes](#quick-usage-notes)
- [Choosing an Example](#choosing-an-example)
- [Troubleshooting](#troubleshooting)
- [Guidance for New Users](#guidance-for-new-users)
- [Guidance for C++ Developers](#guidance-for-c-developers)

---

## Overview and Quick Start

The `examples/` directory contains runnable sketches that cover:
- Getting started (blinking, first LED, pin modes)
- Color utilities, palettes, and HSV/CRGB conversions
- Classic 1D effects (Cylon, Fire, Twinkles, DemoReel100)
- 2D matrix helpers, XY mapping, and raster effects
- Advanced pipelines like downscale/upscale and path rendering
- Platform-specific demos (Teensy OctoWS2811, ESP I2S)
- Browser/WASM examples with JSON-driven UI controls

Typical first steps:
- Open an example in the Arduino IDE and change `NUM_LEDS`, chipset, and `DATA_PIN` to match hardware
- For matrices, define width/height and choose serpentine vs. row-major mapping
- For WASM or host exploration, use the WASM examples or the STUB platform (see below)

---

## How to Run Examples

### Arduino IDE (classic workflow)

- Open an `.ino` file directly (e.g., `examples/Blink/Blink.ino`)
- Edit the configuration near the top:
  - LED type: `FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS)`
  - Strip length: `NUM_LEDS`
  - Brightness: `FastLED.setBrightness(…)`
- Select your board and COM/serial port, then Upload

Tips:
- For 2D examples, set `WIDTH`/`HEIGHT` and confirm wiring (serpentine vs. linear)
- If the animation is mirrored or offset, adjust the mapping helper

### PlatformIO (boards and native host)

- Board-based workflows: create a `platformio.ini` that targets your MCU and copy an example sketch into a project `src/` folder
- Host/STUB workflows: use the STUB platform for local testing where appropriate (no hardware); advanced builds hook into `src/platforms/stub/`

The repository includes `ci/native/` and `ci/kitchensink/` PlatformIO configs you can reference for host builds and integration tests.

### Teensy/OctoWS2811

- Examples under `examples/OctoWS2811*` and related Teensy demos show multi-output patterns
- Replace pin/channel configuration and buffer sizes to match your wiring; ensure you select the correct Teensy model in your IDE/toolchain

### ESP32 / I2S (parallel output)

- See `examples/EspI2SDemo/` and `examples/Esp32S3I2SDemo/`
- These demonstrate high-throughput I2S-driven output; choose a board definition matching your dev board and wiring
- On some environments, parallel output requires specific pin sets and PSRAM settings; consult the sketch notes

### WASM (browser demos + JSON UI)

- `examples/wasm/` and related WASM-focused examples run in the browser
- The JSON UI system enables sliders, buttons, and other controls (see `src/platforms/wasm` and `src/fl/ui.h`)
- Typical flow: build to WebAssembly, serve the app, and interact via the browser UI

---

## Directory Map (by theme)

This list highlights commonly used examples. It is not exhaustive—browse the folders for more.

### Basics and core idioms

- `Blink/` — minimal starting point
- `FirstLight/` — walk a single bright pixel along the strip
- `PinMode/` — simple input pin usage
- `RGBSetDemo/` — basic pixel addressing and assignment
- `RGBCalibrate/` — adjust color channel balance

### Color, palettes, and HSV

- `ColorPalette/` — palette usage and transitions
- `ColorTemperature/` — white point and temperature helpers
- `HSVTest/` — HSV types and conversions
- `ColorBoost/` — saturation/luminance shaping for high visual impact

### Classic 1D effects

- `Cylon/`, `FxCylon/` — scanning eye; FX variants use higher-level helpers
- `Fire2012/`, `Fire2012WithPalette/`, `FxFire2012/` — classic fire effect
- `TwinkleFox/`, `FxTwinkleFox/` — twinkling star fields
- `Pride2015/`, `FxPride2015/` — rainbow variants
- `DemoReel100/`, `FxDemoReel100/` — rotating showcase of many patterns
- `Wave/` — 1D wave toolkit

### 2D, matrices, mapping

- `XYMatrix/` — matrix mapping helpers and layouts
- `Wave2d/`, `FxWave2d/` — 2D wavefields
- `Blur2d/` — separable blur across a matrix
- `Downscale/` — render high-res, resample to panel resolution
- `Animartrix/` — animated matrix patterns and helpers
- `SmartMatrix/` — SmartMatrix integration sketch

### FX engine and higher-level utilities

- `FxEngine/` — scaffolding for composing layers and frames
- `FxGfx2Video/` — utilities to pipe graphics into frame/video helpers
- `fx/` under `src/` provides the building blocks used by these examples

### Audio and reactive demos

- `Audio/` — audio input + analysis (simple and advanced variants)
- `Ports/PJRCSpectrumAnalyzer/` — Teensy-centric spectrum analyzer

### Storage, SD card, and data

- `FxSdCard/` — SD-backed media and assets (see `data/` subfolder)

### Multiple strips, parallel, and high-density

- `Multiple/` — organize multiple arrays/segments
- `TeensyParallel/` — multi-output example
- `TeensyMassiveParallel/` — larger multi-output wiring
- `OctoWS2811/`, `OctoWS2811Demo/` — OctoWS2811 multi-channel output

### ESP/Teensy/SmartMatrix specifics

- `EspI2SDemo/`, `Esp32S3I2SDemo/` — ESP32 parallel/I2S output
- `SmartMatrix/` — run on SmartMatrix hardware

### WASM and UI

- `wasm/` — browser-targeted demo
- `WasmScreenCoords/` — UI overlay and coordinate visualization
- `Json/` — JSON-structured sketch example
- `UITest/` — showcase of JSON UI controls and groups

### Larger projects and showcases

- `LuminescentGrand/` — complex, multi-file installation piece
- `Luminova/` — larger effect set
- `Chromancer/` — advanced example with assets and helpers

---

## Quick Usage Notes

- Always set the LED chipset, pin, color order, and `NUM_LEDS` to match your hardware:
  - `FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);`
  - Common CHIPSETs: `WS2812B`, `SK6812`, `APA102` (APA102 also needs `CLK_PIN`)
- Adjust brightness and power: `FastLED.setBrightness(…)` and consider power limits for dense strips
- For matrices, define `WIDTH`/`HEIGHT` and use serpentine or row-major helpers; verify orientation
- Prefer using `fl::Leds` + `XYMap` for 2D logic when the example exposes those hooks
- For high quality on low-res displays, render at higher resolution and `downscale`

---

## Choosing an Example

- New to FastLED: start with `Blink` → `FirstLight` → `DemoReel100`
- Building a palette-based animation: `ColorPalette` and `ColorTemperature`
- Making a 1D animation: `Cylon`, `Fire2012`, `TwinkleFox`
- Driving a panel: `XYMatrix`, then try `Downscale` or `Wave2d`
- Multi-output / high-density: `OctoWS2811Demo`, `TeensyParallel`
- Browser demo / UI: `wasm`, `UITest`, `Json`
- Advanced/experimental: `Corkscrew` (in `src/fl`), `Fx*` examples, and `Chromancer`

---

## Troubleshooting

- Nothing lights up:
  - Re-check `DATA_PIN`, chipset, and `COLOR_ORDER`
  - Confirm `NUM_LEDS` and power are correct; try a low brightness first
- Colors look wrong: try `GRB` vs. `RGB` ordering; some strips invert green/red
- Matrix appears mirrored or wrapped: change serpentine/row-major mapping or flip `WIDTH` and `HEIGHT`
- ESP32 I2S pinning: verify the chosen pins are valid for your board’s I2S peripheral
- Teensy multi-output: confirm channel count and buffer sizes match your wiring

---

## Guidance for New Users

- Include `FastLED.h`, pick your chipset, set `NUM_LEDS`, and get something simple running first
- For matrices, draw into width/height coordinates and let mapping/wiring helpers translate to indices
- Explore the palette and HSV examples for smooth color; try `fill_rainbow` and `CHSV`
- When moving to larger builds, consider splitting configuration and effect code into separate files for clarity

## Guidance for C++ Developers

- Many examples are deliberately small; for more reusable building blocks, see `src/fl/` and `src/fx/`
- Prefer `fl::` containers, views (`fl::span`), and graphics helpers for portability and quality
- For UI/remote control on capable targets, use the JSON UI elements (see `src/fl/ui.h`) and WASM bridge (`src/platforms/wasm`)

---

This README will evolve alongside the examples. Browse subfolders for sketch-specific notes and hardware details. For the core library map and deeper subsystems, see `src/README.md` and `src/fl/README.md`. 
