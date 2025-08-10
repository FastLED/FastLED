# FastLED Source Tree (`src/`)

This document introduces the FastLED source tree housed under `src/`, first by mapping the major directories and public headers, then providing a practical guide for two audiences:
- First‑time FastLED users who want to know which headers to include and how to use common features
- Experienced C++ developers exploring subsystems and how the library is organized

The `src/` directory contains the classic FastLED public API (`FastLED.h`), the cross‑platform `fl::` foundation, effect/graphics utilities, platform backends, sensors, fonts/assets, and selected third‑party shims.

## Table of Contents

- [Overview and Quick Start](#overview-and-quick-start)
  - [What lives in `src/`](#what-lives-in-src)
  - [Include policy](#include-policy)
- [Directory Map (7 major areas)](#directory-map-7-major-areas)
  - [Public headers and glue](#public-headers-and-glue)
  - [Core foundation: `fl/`](#core-foundation-fl)
  - [Effects and graphics: `fx/`](#effects-and-graphics-fx)
  - [Platforms and HAL: `platforms/`](#platforms-and-hal-platforms)
  - [Sensors and input: `sensors/`](#sensors-and-input-sensors)
  - [Fonts and assets: `fonts/`](#fonts-and-assets-fonts)
  - [Third‑party and shims: `third_party/`](#third-party-and-shims-third_party)
- [Quick Usage Examples](#quick-usage-examples)
  - [Classic strip setup](#classic-strip-setup)
  - [2D matrix with `fl::Leds` + `XYMap`](#2d-matrix-with-flleds--xymap)
  - [Resampling pipeline (downscale/upscale)](#resampling-pipeline-downscaleupscale)
  - [JSON UI (WASM)](#json-ui-wasm)
- [Deep Dives by Area](#deep-dives-by-area)
  - [Public API surface](#public-api-surface)
  - [Core foundation cross‑reference](#core-foundation-cross-reference)
  - [FX engine building blocks](#fx-engine-building-blocks)
  - [Platform layer and stubs](#platform-layer-and-stubs)
  - [WASM specifics](#wasm-specifics)
  - [Testing and compile‑time gates](#testing-and-compile-time-gates)
- [Guidance for New Users](#guidance-for-new-users)
- [Guidance for C++ Developers](#guidance-for-c-developers)

---

## Overview and Quick Start

### What lives in `src/`

- `FastLED.h` and classic public headers for sketches
- `fl/`: cross‑platform foundation (containers, math, graphics primitives, async, JSON)
- `fx/`: effect/graphics utilities and 1D/2D composition helpers
- `platforms/`: hardware abstraction layers (AVR, ARM, ESP, POSIX, STUB, WASM, etc.)
- `sensors/`: basic input components (buttons, digital pins)
- `fonts/`: built‑in bitmap fonts for overlays
- `third_party/`: vendored minimal headers and compatibility glue

If you are writing Arduino‑style sketches, include `FastLED.h`. For advanced/host builds or portable C++, prefer including only what you use from `fl/` and related subsystems.

### Include policy

- Sketches: `#include <FastLED.h>` for the canonical API
- Advanced C++ (host, tests, platforms): include targeted headers (e.g., `"fl/vector.h"`, `"fl/downscale.h"`, `"fx/frame.h"`)
- Prefer `fl::` facilities to keep portability across compilers and embedded targets. See `src/fl/README.md` for the full `fl::` guide.

---

## Directory Map (7 major areas)

### Public headers and glue

- Entry points: `FastLED.h`, color types, core utilities, and historical compatibility headers
- Glue to bridge classic APIs to modern subsystems (e.g., `fl::` drawing/mapping)

### Core foundation: `fl/`

- Embedded‑aware STL‑like utilities, geometry/graphics primitives, color/math, async, JSON, I/O
- Start here for containers, views, memory, rasterization, XY mapping, resampling, and UI glue
- See `src/fl/README.md` for a comprehensive breakdown

### Effects and graphics: `fx/`

- Composition engine pieces: frames, layers, blenders, interpolators, and specialized effect helpers
- 1D/2D utilities and video helpers for higher‑level effect construction

### Platforms and HAL: `platforms/`

- Target backends and shims for AVR, ARM, ESP, POSIX, STUB (for tests), WASM, etc.
- Shared code for JSON UI, timing, and platform integrations

### Sensors and input: `sensors/`

- Minimal input primitives (e.g., `button`, `digital_pin`) intended for demos and portable logic

### Fonts and assets: `fonts/`

- Small bitmap fonts (e.g., `console_font_4x6`) to draw text overlays in demos/tests

### Third‑party and shims: `third_party/`

- Small vendored components such as `arduinojson` headers and platform shims kept intentionally minimal

---

## Quick Usage Examples

The following examples show how to use common capabilities reachable from `src/`.

### Classic strip setup

```cpp
#include <FastLED.h>

constexpr uint16_t NUM_LEDS = 144;
CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<WS2812B, 5, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(160);
}

void loop() {
    fill_rainbow(leds, NUM_LEDS);
    FastLED.show();
}
```

### 2D matrix with `fl::Leds` + `XYMap`

```cpp
#include <FastLED.h>
#include "fl/leds.h"
#include "fl/xymap.h"

constexpr uint16_t WIDTH  = 16;
constexpr uint16_t HEIGHT = 16;
constexpr uint16_t NUM_LEDS = WIDTH * HEIGHT;
CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<WS2812B, 5, GRB>(leds, NUM_LEDS);
}

void loop() {
    auto map   = fl::XYMap::constructSerpentine(WIDTH, HEIGHT);
    fl::Leds s = fl::Leds(leds, map);

    for (uint16_t y = 0; y < HEIGHT; ++y) {
        for (uint16_t x = 0; x < WIDTH; ++x) {
            uint8_t hue = uint8_t((x * 255u) / (WIDTH ? WIDTH : 1));
            s(x, y) = CHSV(hue, 255, 255);
        }
    }

    FastLED.show();
}
```

### Resampling pipeline (downscale/upscale)

```cpp
#include "fl/downscale.h"
#include "fl/upscale.h"
#include "fl/grid.h"

// High‑def buffer drawn elsewhere
fl::Grid<CRGB> srcHD(64, 64);

// Downscale into your physical panel resolution
fl::Grid<CRGB> panel(16, 16);

void render_frame() {
    fl::downscale(srcHD, srcHD.size_xy(), panel, panel.size_xy());
}
```

### JSON UI (WASM)

```cpp
#include <FastLED.h>
#include "fl/ui.h"

UISlider brightness("Brightness", 128, 0, 255);
UICheckbox enabled("Enabled", true);

void setup() {
    brightness.onChanged([](UISlider& s){ FastLED.setBrightness((uint8_t)s); });
}

void loop() {
    FastLED.show();
}
```

See `src/fl/README.md` and `platforms/shared/ui/json/readme.md` for JSON UI lifecycle and platform bridges.

---

## Deep Dives by Area

### Public API surface

- Sketch entry point: `FastLED.h`
- Classic helpers: color conversions, dithering, palettes, HSV/CRGB utilities
- Bridge to newer subsystems (e.g., treat `CRGB*` as an `fl::Leds` surface for 2D rendering)

### Core foundation cross‑reference

- The `fl::` namespace offers:
  - Containers and views (`vector`, `span`, `slice`)
  - Graphics/rasterization (`raster`, `xymap`, `rectangular_draw_buffer`)
  - Resampling (`downscale`, `upscale`, `supersample`)
  - Color/math (`hsv`, `hsv16`, `gamma`, `gradient`, `sin32`, `random`)
  - Async/functional (`task`, `promise`, `function_list`)
  - JSON and I/O (`json`, `ostream`, `printf`)
- Detailed documentation: `src/fl/README.md`

### FX engine building blocks

- `fx/` contains an effect pipeline with frames, layers, and video helpers
- Typical usage: assemble layers, blend or interpolate frames, and map output to LEDs via `fl::Leds`
- Headers to look for: `fx/frame.h`, `fx/detail/*`, `fx/video/*` (exact APIs evolve; see headers)

### Platform layer and stubs

- `platforms/stub/`: host/test builds without hardware; ideal for CI and examples
- `platforms/wasm/`: web builds with JSON UI, Asyncify integration, and JS glue
- `platforms/arm/`, `platforms/esp/`, `platforms/avr/`, `platforms/posix/`: target‑specific shims
- Compile‑time gates and helpers: see `platforms/*/*.h` and `platforms/ui_defs.h`

### WASM specifics

- JSON UI is enabled by default on WASM (`FASTLED_USE_JSON_UI=1`)
- Browser UI is driven via platform glue (`src/platforms/wasm/ui.cpp`, JS modules)
- Async patterns rely on the `fl::task`/engine events integration; see examples in `examples/` under `wasm/` topics

### Testing and compile‑time gates

- The source tree includes a rich test suite under `tests/` and CI utilities under `ci/`
- For portability, prefer `fl::` primitives and compile‑time guards (`compiler_control.h`, `warn.h`, `assert.h`)
- Many platform backends provide “fake” or stubbed facilities for host validation

---

## Guidance for New Users

- If you are writing sketches: include `FastLED.h` and follow examples in `examples/`
- For 2D matrices or layouts: wrap your `CRGB*` with `fl::Leds` and an `fl::XYMap`
- For high‑quality rendering: consider `supersample` + `downscale` to preserve detail
- On WASM: explore the JSON UI to add sliders/buttons without platform‑specific code

## Guidance for C++ Developers

- Treat `fl::` as an embedded‑friendly STL and graphics/math foundation; prefer `fl::span<T>` for parameters
- Use compiler‑portable control macros from `fl/` headers rather than raw pragmas
- Keep platform dependencies behind `platforms/` shims and use the STUB backend for host tests
- Prefer `fl::memfill` over `memset` where applicable to match codebase idioms

---

This README will evolve alongside the codebase. For subsystem details, jump into the directories listed above and consult header comments and `src/fl/README.md` for the foundation layer.
