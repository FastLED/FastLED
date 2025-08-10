# FastLED Core Library (`src/fl`)

This document introduces the FastLED core library housed under `src/fl/`, first by listing its headers, then by progressively expanding into an educational guide for two audiences:
- First‑time FastLED users who want to understand what lives below `FastLED.h` and how to use common utilities
- Experienced C++ developers exploring the `fl::` API as a cross‑platform, STL‑free foundation

FastLED avoids direct dependencies on the C++ standard library in embedded contexts and offers its own STL‑like building blocks in the `fl::` namespace.

## Table of Contents

- [Overview and Quick Start](#overview-and-quick-start)
- [Header Groups (5 major areas)](#header-groups-5-major-areas)
  - [STL‑like data structures and core utilities](#1-stl-like-data-structures-and-core-utilities)
  - [Graphics, geometry, and rendering](#2-graphics-geometry-and-rendering)
  - [Color, math, and signal processing](#3-color-math-and-signal-processing)
  - [Concurrency, async, and functional](#4-concurrency-async-and-functional)
  - [I/O, JSON, and text/formatting](#5-io-json-and-textformatting)
- [Comprehensive Module Breakdown](#comprehensive-module-breakdown)
- [Guidance for New Users](#guidance-for-new-users)
- [Guidance for C++ Developers](#guidance-for-c-developers)

---

## Overview and Quick Start

### What is `fl::`?

`fl::` is FastLED’s cross‑platform foundation layer. It provides containers, algorithms, memory utilities, math, graphics primitives, async/concurrency, I/O, and platform shims designed to work consistently across embedded targets and host builds. It replaces common `std::` facilities with equivalents tailored for embedded constraints and portability.

Key properties:
- Cross‑platform, embedded‑friendly primitives
- Minimal dynamic allocation where possible; clear ownership semantics
- Consistent naming and behavior across compilers/toolchains
- Prefer composable, header‑driven utilities

### Goals and design constraints

- Avoid fragile dependencies on `std::` in embedded builds; prefer `fl::` types
- Emphasize deterministic behavior and low overhead
- Provide familiar concepts (vector, span, optional, variant, function) with embedded‑aware implementations
- Offer safe RAII ownership types and moveable wrappers for resource management
- Keep APIs flexible by preferring non‑owning views (`fl::span`) as function parameters

### Naming and idioms

- Names live in the `fl::` namespace
- Prefer `fl::span<T>` as input parameters over owning containers
- Use `fl::shared_ptr<T>` / `fl::unique_ptr<T>` instead of raw pointers
- Favor explicit ownership and lifetimes; avoid manual `new`/`delete`

### Getting started: common building blocks

Include the top‑level header you need, or just include `FastLED.h` in sketches. When writing platform‑agnostic C++ within this repo, include the specific `fl/` headers you use.

Common types to reach for:
- Containers and views: `fl::vector<T>`, `fl::deque<T>`, `fl::span<T>`, `fl::slice<T>`
- Strings and streams: `fl::string`, `fl::ostream`, `fl::sstream`, `fl::printf`
- Optionals and variants: `fl::optional<T>`, `fl::variant<...>`
- Memory/ownership: `fl::unique_ptr<T>`, `fl::shared_ptr<T>`, `fl::weak_ptr<T>`
- Functional: `fl::function<Signature>`, `fl::function_list<Signature>`
- Concurrency: `fl::thread`, `fl::mutex`, `fl::thread_local`
- Async: `fl::promise<T>`, `fl::task`
- Math: `fl::math`, `fl::sin32`, `fl::random`, `fl::gamma`, `fl::gradient`
- Graphics: `fl::raster`, `fl::screenmap`, `fl::rectangular_draw_buffer`, `fl::downscale`, `fl::supersample`
- Color: `fl::hsv`, `fl::hsv16`, `fl::colorutils`
- JSON: `fl::Json` with safe defaults and ergonomic access

Example: using containers, views, and ownership

```cpp
#include "fl/vector.h"
#include "fl/span.h"
#include "fl/memory.h"   // for fl::make_unique

void process(fl::span<const int> values) {
    // Non-owning view over contiguous data
}

void example() {
    fl::vector<int> data;
    data.push_back(1);
    data.push_back(2);
    data.push_back(3);

    process(fl::span<const int>(data));

    auto ptr = fl::make_unique<int>(42);
    // Exclusive ownership; automatically freed when leaving scope
}
```

Example: JSON with safe default access

```cpp
#include "fl/json.h"

void json_example(const fl::string& jsonStr) {
    fl::Json json = fl::Json::parse(jsonStr);
    int brightness = json["config"]["brightness"] | 128;  // default if missing
    bool enabled = json["enabled"] | false;
}
```

---

## Header Groups (5 major areas)

### 1) STL‑like data structures and core utilities

Containers, views, algorithms, compile‑time utilities, memory/ownership, portability helpers.

- Containers: `vector.h`, `deque.h`, `queue.h`, `priority_queue.h`, `set.h`, `map.h`, `unordered_set.h`, `hash_map.h`, `hash_set.h`, `rbtree.h`, `bitset.h`, `bitset_dynamic.h`
- Views and ranges: `span.h`, `slice.h`, `range_access.h`
- Tuples and algebraic types: `tuple.h`, `pair.h`, `optional.h`, `variant.h`
- Algorithms and helpers: `algorithm.h`, `transform.h`, `comparators.h`, `range_access.h`
- Types and traits: `types.h`, `type_traits.h`, `initializer_list.h`, `utility.h`, `move.h`, `template_magic.h`, `stdint.h`, `cstddef.h`, `namespace.h`
- Memory/ownership: `unique_ptr.h`, `shared_ptr.h`, `weak_ptr.h`, `scoped_ptr.h`, `scoped_array.h`, `ptr.h`, `ptr_impl.h`, `referent.h`, `allocator.h`, `memory.h`, `memfill.h`, `inplacenew.h`
- Portability and compiler control: `compiler_control.h`, `force_inline.h`, `virtual_if_not_avr.h`, `has_define.h`, `register.h`, `warn.h`, `trace.h`, `dbg.h`, `assert.h`, `unused.h`, `export.h`, `dll.h`, `deprecated.h`, `avr_disallowed.h`, `bit_cast.h`, `id_tracker.h`, `insert_result.h`, `singleton.h`

Per‑header quick descriptions:

- `vector.h`: Dynamically sized contiguous container with embedded‑friendly API.
- `deque.h`: Double‑ended queue for efficient front/back operations.
- `queue.h`: FIFO adapter providing push/pop semantics over an underlying container.
- `priority_queue.h`: Heap‑based ordered queue for highest‑priority retrieval.
- `set.h`: Ordered unique collection with deterministic iteration.
- `map.h`: Ordered key‑value associative container.
- `unordered_set.h`: Hash‑based unique set for average O(1) lookups.
- `hash_map.h`: Hash‑based key‑value container tuned for embedded use.
- `hash_set.h`: Hash set implementation complementing `hash_map.h`.
- `rbtree.h`: Balanced tree primitive used by ordered containers.
- `bitset.h`: Fixed‑size compile‑time bitset operations.
- `bitset_dynamic.h`: Runtime‑sized bitset for flexible masks.
- `span.h`: Non‑owning view over contiguous memory (preferred function parameter).
- `slice.h`: Strided or sub‑range view utilities for buffers.
- `range_access.h`: Helpers to unify begin/end access over custom ranges.
- `tuple.h`: Heterogeneous fixed‑size aggregate with structured access.
- `pair.h`: Two‑value aggregate type for simple key/value or coordinate pairs.
- `optional.h`: Presence/absence wrapper to avoid sentinel values.
- `variant.h`: Type‑safe tagged union for sum types without heap allocation.
- `algorithm.h`: Core algorithms (search, sort helpers, transforms) adapted to `fl::` containers.
- `transform.h`: Functional style element‑wise transformations with spans/ranges.
- `comparators.h`: Reusable comparator utilities for ordering operations.
- `types.h`: Canonical type aliases and shared type definitions.
- `type_traits.h`: Compile‑time type inspection and enable_if‑style utilities.
- `initializer_list.h`: Lightweight initializer list support for container construction.
- `utility.h`: Miscellaneous helpers (swap, forward, etc.) suitable for embedded builds.
- `move.h`: Move/forward utilities mirroring standard semantics.
- `template_magic.h`: Metaprogramming helpers to simplify template code.
- `stdint.h`: Fixed‑width integer definitions for cross‑compiler consistency.
- `cstddef.h`: Size/ptrdiff and nullptr utilities for portability.
- `namespace.h`: Internal macros/utilities for managing `fl::` namespaces safely.
- `unique_ptr.h`: Exclusive ownership smart pointer with RAII semantics.
- `shared_ptr.h`: Reference‑counted shared ownership smart pointer.
- `weak_ptr.h`: Non‑owning reference to `shared_ptr`‑managed objects.
- `scoped_ptr.h`: Scope‑bound ownership (no move) for simple RAII cleanup.
- `scoped_array.h`: RAII wrapper for array allocations.
- `ptr.h`/`ptr_impl.h`: Pointer abstractions and shared machinery for smart pointers.
- `referent.h`: Base support for referent/observer relationships.
- `allocator.h`: Custom allocators tailored for embedded constraints.
- `memory.h`: Low‑level memory helpers (construct/destroy, address utilities).
- `memfill.h`: Zero‑cost fill utilities (prefer over `memset` in codebase).
- `inplacenew.h`: Placement new helpers for manual lifetime management.
- `compiler_control.h`: Unified compiler warning/pragma control macros.
- `force_inline.h`: Portable always‑inline control macros.
- `virtual_if_not_avr.h`: Virtual specifier abstraction for AVR compatibility.
- `has_define.h`: Preprocessor feature checks and conditional compilation helpers.
- `register.h`: Register annotation shims for portability.
- `warn.h`/`trace.h`/`dbg.h`: Logging, tracing, and diagnostics helpers.
- `assert.h`: Assertions suited for embedded/testing builds.
- `unused.h`: Intentional unused variable/function annotations.
- `export.h`/`dll.h`: Visibility/export macros for shared library boundaries.
- `deprecated.h`: Cross‑compiler deprecation annotations.
- `avr_disallowed.h`: Guardrails to prevent unsupported usage on AVR.
- `bit_cast.h`: Safe bit reinterpretation where supported, with fallbacks.
- `id_tracker.h`: ID generation/tracking utility for object registries.
- `insert_result.h`: Standardized result type for associative container inserts.
- `singleton.h`: Simple singleton helper for cases requiring global access.

### 2) Graphics, geometry, and rendering

Rasterization, coordinate mappings, paths, grids, resampling, draw buffers, and related glue.

- Raster and buffers: `raster.h`, `raster_sparse.h`, `rectangular_draw_buffer.h`
- Screen and tiles: `screenmap.h`, `tile2x2.h`
- Coordinates and mappings: `xmap.h`, `xymap.h`, `screenmap.h`
- Paths and traversal: `xypath.h`, `xypath_impls.h`, `xypath_renderer.h`, `traverse_grid.h`, `grid.h`, `line_simplification.h`
- Geometry primitives: `geometry.h`, `point.h`
- Resampling/scaling: `downscale.h`, `upscale.h`, `supersample.h`
- Glue and UI: `leds.h`, `ui.h`, `ui_impl.h`, `rectangular_draw_buffer.h`
- Specialized: `corkscrew.h`, `wave_simulation.h`, `wave_simulation_real.h`, `tile2x2.h`, `screenmap.h`

Per‑header quick descriptions:

- `raster.h`: Core raster interface and operations for pixel buffers.
- `raster_sparse.h`: Sparse/partial raster representation for memory‑efficient updates.
- `rectangular_draw_buffer.h`: Double‑buffered rectangular draw surface helpers.
- `screenmap.h`: Maps logical coordinates to physical LED indices/layouts.
- `tile2x2.h`: Simple 2×2 tiling utilities for composing larger surfaces.
- `xmap.h`: General coordinate mapping utilities.
- `xymap.h`: XY coordinate to index mapping helpers for matrices and panels.
- `xypath.h`: Path representation in XY space for drawing and effects.
- `xypath_impls.h`: Implementations and algorithms supporting `xypath.h`.
- `xypath_renderer.h`: Renders paths into rasters with configurable styles.
- `traverse_grid.h`: Grid traversal algorithms for curves/lines/fills.
- `grid.h`: Grid data structure and iteration helpers.
- `line_simplification.h`: Path simplification (e.g., Douglas‑Peucker‑style) for fewer segments.
- `geometry.h`: Basic geometric computations (distances, intersections, etc.).
- `point.h`: Small coordinate/vector primitive type.
- `downscale.h`: Resampling utilities to reduce resolution while preserving features.
- `upscale.h`: Upsampling utilities for enlarging frames.
- `supersample.h`: Anti‑aliasing via multi‑sample accumulation.
- `leds.h`: Integration helpers bridging LED buffers to rendering utilities.
- `ui.h` / `ui_impl.h`: Minimal UI adapter hooks for demos/tests.
- `corkscrew.h`: Experimental path/trajectory utilities for visual effects.
- `wave_simulation.h` / `wave_simulation_real.h`: Simulated wave dynamics for organic effects.

### 3) Color, math, and signal processing

Color models, gradients, gamma, math helpers, random, noise, mapping, and basic DSP.

- Color and palettes: `colorutils.h`, `colorutils_misc.h`, `hsv.h`, `hsv16.h`, `gradient.h`, `fill.h`, `five_bit_hd_gamma.h`, `gamma.h`
- Math and mapping: `math.h`, `math_macros.h`, `sin32.h`, `map_range.h`, `random.h`, `lut.h`, `clamp.h`, `clear.h`, `splat.h`, `transform.h`
- Noise and waves: `noise_woryley.h`, `wave_simulation.h`, `wave_simulation_real.h`
- DSP and audio: `fft.h`, `fft_impl.h`, `audio.h`, `audio_reactive.h`
- Time utilities: `time.h`, `time_alpha.h`

Per‑header quick descriptions:

- `colorutils.h`: High‑level color operations (blend, scale, lerp) for LED pixels.
- `colorutils_misc.h`: Additional helpers and niche color operations.
- `hsv.h` / `hsv16.h`: HSV color types and conversions (8‑bit and 16‑bit variants).
- `gradient.h`: Gradient construction, sampling, and palette utilities.
- `fill.h`: Efficient buffer/palette filling operations for pixel arrays.
- `five_bit_hd_gamma.h`: Gamma correction tables tuned for high‑definition 5‑bit channels.
- `gamma.h`: Gamma correction functions and LUT helpers.
- `math.h` / `math_macros.h`: Core math primitives/macros for consistent numerics.
- `sin32.h`: Fast fixed‑point sine approximations for animations.
- `map_range.h`: Linear mapping and clamping between numeric ranges.
- `random.h`: Pseudorandom utilities for effects and dithering.
- `lut.h`: Lookup table helpers for precomputed transforms.
- `clamp.h`: Bounds enforcement for numeric types.
- `clear.h`: Clear/zero helpers for buffers with type awareness.
- `splat.h`: Vectorized repeat/write helpers for bulk operations.
- `transform.h`: Element transforms (listed here as it is often used for pixel ops too).
- `noise_woryley.h`: Worley/cellular noise generation utilities.
- `wave_simulation*.h`: Wavefield simulation (also referenced in graphics).
- `fft.h` / `fft_impl.h`: Fast Fourier Transform interfaces and backends.
- `audio.h`: Audio input/stream abstractions for host/platforms that support it.
- `audio_reactive.h`: Utilities to drive visuals from audio features.
- `time.h`: Timekeeping helpers (millis/micros abstractions when available).
- `time_alpha.h`: Smoothed/exponential time‑based interpolation helpers.

### 4) Concurrency, async, and functional

Threads, synchronization, async primitives, eventing, and callable utilities.

- Threads and sync: `thread.h`, `mutex.h`, `thread_local.h`
- Async primitives: `promise.h`, `promise_result.h`, `task.h`, `async.h`
- Functional: `function.h`, `function_list.h`, `functional.h`
- Events and engine hooks: `engine_events.h`

Per‑header quick descriptions:

- `thread.h`: Portable threading abstraction for supported hosts.
- `mutex.h`: Mutual exclusion primitive compatible with `fl::thread`. Almost all platforms these are fake implementations.
- `thread_local.h`: Thread‑local storage shim for supported compilers.
- `promise.h`: Moveable wrapper around asynchronous result delivery.
- `promise_result.h`: Result type accompanying promises/futures.
- `task.h`: Lightweight async task primitive for orchestration.
- `async.h`: Helpers for async composition and coordination.
- `function.h`: Type‑erased callable wrapper analogous to `std::function`.
- `function_list.h`: Multicast list of callables with simple invoke semantics.
- `functional.h`: Adapters, binders, and predicates for composing callables.
- `engine_events.h`: Event channel definitions for engine‑style systems.

### 5) I/O, JSON, and text/formatting

Streams, strings, formatted output, bytestreams, filesystem, JSON.

- Text and streams: `string.h`, `str.h`, `ostream.h`, `istream.h`, `sstream.h`, `strstream.h`, `printf.h`
- JSON: `json.h`
- Bytestreams and I/O: `bytestream.h`, `bytestreammemory.h`, `io.h`, `file_system.h`, `fetch.h`

Per‑header quick descriptions:

- `string.h` / `str.h`: String types and helpers without pulling in `std::string`.
- `ostream.h` / `istream.h`: Output/input stream interfaces for host builds.
- `sstream.h` / `strstream.h`: String‑backed stream buffers and helpers.
- `printf.h`: Small, portable formatted print utilities.
- `json.h`: Safe, ergonomic `fl::Json` API with defaulting operator (`|`).
- `bytestream.h`: Sequential byte I/O abstraction for buffers/streams.
- `bytestreammemory.h`: In‑memory byte stream implementation.
- `io.h`: General I/O helpers for files/streams where available.
- `file_system.h`: Minimal filesystem adapter for host platforms.
- `fetch.h`: Basic fetch/request helpers for network‑capable hosts.
## Comprehensive Module Breakdown

This section groups headers by domain, explains their role, and shows minimal usage snippets. Names shown are representative; see the header list above for the full inventory.

### Containers and Views

- Sequence and associative containers: `vector.h`, `deque.h`, `queue.h`, `priority_queue.h`, `set.h`, `map.h`, `unordered_set.h`, `hash_map.h`, `hash_set.h`, `rbtree.h`
- Non‑owning and slicing: `span.h`, `slice.h`, `range_access.h`

Why: Embedded‑aware containers with predictable behavior across platforms. Prefer passing `fl::span<T>` to functions.

```cpp
#include "fl/vector.h"
#include "fl/span.h"

size_t count_nonzero(fl::span<const uint8_t> bytes) {
    size_t count = 0;
    for (uint8_t b : bytes) { if (b != 0) { ++count; } }
    return count;
}
```

### Strings and Streams

- Text types and streaming: `string.h`, `str.h`, `ostream.h`, `istream.h`, `sstream.h`, `strstream.h`, `printf.h`

Why: Consistent string/stream facilities without pulling in the standard streams.

```cpp
#include "fl/string.h"
#include "fl/sstream.h"

fl::string greet(const fl::string& name) {
    fl::sstream ss;
    ss << "Hello, " << name << "!";
    return ss.str();
}
```

### Memory and Ownership

- Smart pointers and utilities: `unique_ptr.h`, `shared_ptr.h`, `weak_ptr.h`, `scoped_ptr.h`, `scoped_array.h`, `allocator.h`, `memory.h`, `memfill.h`

Why: RAII ownership with explicit semantics. Prefer `fl::make_shared<T>()`/`fl::make_unique<T>()` patterns where available, or direct constructors provided by these headers.

```cpp
#include "fl/shared_ptr.h"

struct Widget { int value; };

void ownership_example() {
    fl::shared_ptr<Widget> w(new Widget{123});
    auto w2 = w; // shared ownership
}
```

### Functional Utilities

- Callables and lists: `function.h`, `function_list.h`, `functional.h`

Why: Store callbacks and multicast them safely.

```cpp
#include "fl/function_list.h"

void on_event(int code) { /* ... */ }

void register_handlers() {
    fl::function_list<void(int)> handlers;
    handlers.add(on_event);
    handlers(200); // invoke all
}
```

### Concurrency and Async

- Threads and synchronization: `thread.h`, `mutex.h`, `thread_local.h`
- Async primitives: `promise.h`, `promise_result.h`, `task.h`

Why: Lightweight foundations for parallel work or async orchestration where supported.

```cpp
#include "fl/promise.h"

fl::promise<int> compute_async(); // returns a moveable wrapper around a future-like result
```

### Math, Random, and DSP

- Core math and helpers: `math.h`, `math_macros.h`, `sin32.h`, `random.h`, `map_range.h`
- Color math: `gamma.h`, `gradient.h`, `colorutils.h`, `colorutils_misc.h`, `hsv.h`, `hsv16.h`, `fill.h`
- FFT and analysis: `fft.h`, `fft_impl.h`

Why: Efficient numeric operations for LED effects, audio reactivity, and transforms.

```cpp
#include "fl/gamma.h"

uint8_t apply_gamma(uint8_t v) {
    return fl::gamma::correct8(v);
}
```

### Geometry and Grids

- Basic geometry: `point.h`, `geometry.h`
- Grid traversal and simplification: `grid.h`, `traverse_grid.h`, `line_simplification.h`

Why: Building blocks for 2D/3D layouts and path operations.

### Graphics, Rasterization, and Resampling

- Raster and buffers: `raster.h`, `raster_sparse.h`, `rectangular_draw_buffer.h`
- Screen mapping and tiling: `screenmap.h`, `tile2x2.h`, `xmap.h`, `xymap.h`
- Paths and renderers: `xypath.h`, `xypath_impls.h`, `xypath_renderer.h`, `traverse_grid.h`
- Resampling and effects: `downscale.h`, `upscale.h`, `supersample.h`

Why: Efficient frame manipulation for LED matrices and coordinate spaces.

```cpp
#include "fl/downscale.h"

// Downscale a high‑res buffer to a target raster (API varies by adapter)
```

#### Graphics Deep Dive

This section explains how the major graphics utilities fit together and how to use them effectively for high‑quality, high‑performance rendering on LED strips, matrices, and complex shapes.

- **Wave simulation (1D/2D)**
  - Headers: `wave_simulation.h`, `wave_simulation_real.h`
  - Concepts:
    - Super‑sampling for quality: choose `SuperSample` factors to run an internal high‑resolution simulation and downsample for display.
    - Consistent speed at higher quality: call `setExtraFrames(u8)` to update the simulation multiple times per frame to maintain perceived speed when super‑sampling.
    - Output accessors: `getf`, `geti16`, `getu8` return float/fixed/byte values; 2D version offers cylindrical wrapping via `setXCylindrical(true)`.
  - Tips for “faster” updates:
    - Use `setExtraFrames()` to advance multiple internal steps per visual frame without changing your outer timing.
    - Prefer `getu8(...)` when feeding color functions or gradients on constrained devices.

- **fl::Leds – array and mapped matrix**
  - Header: `leds.h`; mapping: `xymap.h`
  - `fl::Leds` wraps a `CRGB*` so you can treat it as:
    - A plain `CRGB*` via implicit conversion for classic FastLED APIs.
    - A 2D surface via `operator()(x, y)` that respects an `XYMap` (serpentine, line‑by‑line, LUT, or custom function).
  - Construction:
    - `Leds(CRGB* leds, u16 width, u16 height)` for quick serpentine/rectangular usage.
    - `Leds(CRGB* leds, const XYMap& xymap)` for full control (serpentine, rectangular, user function, or LUT).
  - Access and safety:
    - `at(x, y)`/`operator()(x, y)` map to the correct LED index; out‑of‑bounds is safe and returns a sentinel.
    - `operator[]` exposes row‑major access when the map is serpentine or line‑by‑line.

- **Matrix mapping (XYMap)**
  - Header: `xymap.h`
  - Create maps for common layouts:
    - `XYMap::constructSerpentine(w, h)` for typical pre‑wired panels.
    - `XYMap::constructRectangularGrid(w, h)` for row‑major matrices.
    - `XYMap::constructWithUserFunction(w, h, XYFunction)` for custom wiring.
    - `XYMap::constructWithLookUpTable(...)` for arbitrary wiring via LUT.
  - Utilities:
    - `mapToIndex(x, y)` maps coordinates to strip index.
    - `has(x, y)` tests bounds; `toScreenMap()` converts to a float UI mapping.

- **XY paths and path rendering (xypath)**
  - Headers: `xypath.h`, `xypath_impls.h`, `xypath_renderer.h`, helpers in `tile2x2.h`, `transform.h`.
  - Purpose: parameterized paths in 
    \([0,1] \to (x,y)\) for drawing lines/curves/shapes with subpixel precision.
  - Ready‑made paths: point, line, circle, heart, Archimedean spiral, rose curves, phyllotaxis, Gielis superformula, and Catmull‑Rom splines with editable control points.
  - Rendering:
    - Subpixel sampling via `Tile2x2_u8` enables high quality on low‑res matrices.
    - Use `XYPath::drawColor` or `drawGradient` to rasterize into an `fl::Leds` surface.
    - `XYPath::rasterize` writes into a sparse raster for advanced composition.
  - Transforms and bounds:
    - `setDrawBounds(w, h)` and `setTransform(TransformFloat)` control framing and animation transforms.

- **Subpixel “splat” rendering (Tile2x2)**
  - Header: `tile2x2.h`
  - Concept: represent a subpixel footprint as a 2×2 tile of coverage values (u8 alphas). When a subpixel position moves between LEDs, neighboring LEDs get proportional contributions.
  - Use cases:
    - `Tile2x2_u8::draw(color, xymap, out)` to composite with color per‑pixel.
    - Custom blending: `tile.draw(xymap, visitor)` to apply your own alpha/compositing.
    - Wrapped tiles: `Tile2x2_u8_wrap` supports cylindrical wrap with interpolation for continuous effects.

- **Downscale**
  - Header: `downscale.h`
  - Purpose: resample from a higher‑resolution buffer to a smaller target, preserving features.
  - APIs:
    - `downscale(src, srcXY, dst, dstXY)` general case.
    - `downscaleHalf(...)` optimized 2× reduction (square or mapped) used automatically when sizes match.

- **Upscale**
  - Header: `upscale.h`
  - Purpose: bilinear upsampling from a low‑res buffer to a larger target.
  - APIs:
    - `upscale(input, output, inW, inH, xyMap)` auto‑selects optimized paths.
    - `upscaleRectangular` and `upscaleRectangularPowerOf2` bypass XY mapping for straight row‑major layouts.
    - Float reference versions exist for validation.

- **Corkscrew (cylindrical projection)**
  - Header: `corkscrew.h`
  - Goal: draw into a rectangular buffer and project onto a tightly wrapped helical/cylindrical LED layout.
  - Inputs and sizing:
    - `CorkscrewInput{ totalTurns, numLeds, Gap, invert }` defines geometry; helper `calculateWidth()/calculateHeight()` provide rectangle dimensions for buffer allocation.
  - Mapping and iteration:
    - `Corkscrew::at_exact(i)` returns the exact position for LED i; `at_wrap(float)` returns a wrapped `Tile2x2_u8_wrap` footprint for subpixel‑accurate sampling.
    - Use `toScreenMap(diameter)` to produce a `ScreenMap` for UI overlays or browser visualization.
  - Rectangular buffer integration:
    - `getBuffer()/data()` provide a lazily‑initialized rectangle; `fillBuffer/clearBuffer` manage it.
    - `readFrom(source_grid, use_multi_sampling)` projects from a high‑def source grid to the corkscrew using multi‑sampling for quality.

  - Examples

    1) Manual per‑frame draw (push every frame)

    ```cpp
    #include <FastLED.h>
    #include "fl/corkscrew.h"
    #include "fl/grid.h"
    #include "fl/time.h"

    // Your physical LED buffer
    constexpr uint16_t NUM_LEDS = 144;
    CRGB leds[NUM_LEDS];

    // Define a corkscrew geometry (e.g., 19 turns tightly wrapped)
    fl::Corkscrew::Input input(/*totalTurns=*/19.0f, /*numLeds=*/NUM_LEDS);
    fl::Corkscrew cork(input);

    // A simple rectangular source grid to draw into (unwrapped cylinder)
    // Match the corkscrew's recommended rectangle
    const uint16_t W = cork.cylinder_width();
    const uint16_t H = cork.cylinder_height();
    fl::Grid<CRGB> rect(W, H);

    void draw_pattern(uint32_t t_ms) {
        // Example: time‑animated gradient on the unwrapped cylinder
        for (uint16_t y = 0; y < H; ++y) {
            for (uint16_t x = 0; x < W; ++x) {
                uint8_t hue = uint8_t((x * 255u) / (W ? W : 1)) + uint8_t((t_ms / 10) & 0xFF);
                rect(x, y) = CHSV(hue, 255, 255);
            }
        }
    }

    void project_to_strip() {
        // For each LED index i on the physical strip, sample the unwrapped
        // rectangle using the subpixel footprint from at_wrap(i)
        for (uint16_t i = 0; i < NUM_LEDS; ++i) {
            auto tile = cork.at_wrap(float(i));
            // tile.at(u,v) gives {absolute_pos, alpha}
            // Blend the 2x2 neighborhood from the rectangular buffer
            uint16_t r = 0, g = 0, b = 0, a_sum = 0;
            for (uint16_t vy = 0; vy < 2; ++vy) {
                for (uint16_t vx = 0; vx < 2; ++vx) {
                    auto entry = tile.at(vx, vy); // pair<vec2i16, u8>
                    const auto pos = entry.first; // absolute cylinder coords
                    const uint8_t a = entry.second; // alpha 0..255
                    if (pos.x >= 0 && pos.x < int(W) && pos.y >= 0 && pos.y < int(H) && a > 0) {
                        const CRGB& c = rect(uint16_t(pos.x), uint16_t(pos.y));
                        r += uint16_t(c.r) * a;
                        g += uint16_t(c.g) * a;
                        b += uint16_t(c.b) * a;
                        a_sum += a;
                    }
                }
            }
            if (a_sum == 0) {
                leds[i] = CRGB::Black;
            } else {
                leds[i].r = uint8_t(r / a_sum);
                leds[i].g = uint8_t(g / a_sum);
                leds[i].b = uint8_t(b / a_sum);
            }
        }
    }

    void setup() {
        FastLED.addLeds<WS2812B, 5, GRB>(leds, NUM_LEDS);
    }

    void loop() {
        uint32_t t = fl::time();
        draw_pattern(t);
        project_to_strip();
        FastLED.show();
    }
    ```

    2) Automate with task::before_frame (draw right before render)

    Use the per‑frame task API; no direct EngineEvents binding is needed. Per‑frame tasks are scheduled via `fl::task` and integrated with the frame lifecycle by the async system.

    ```cpp
    #include <FastLED.h>
    #include "fl/task.h"
    #include "fl/async.h"
    #include "fl/corkscrew.h"
    #include "fl/grid.h"

    constexpr uint16_t NUM_LEDS = 144;
    CRGB leds[NUM_LEDS];

    fl::Corkscrew cork(fl::Corkscrew::Input(19.0f, NUM_LEDS));
    const uint16_t W = cork.cylinder_width();
    const uint16_t H = cork.cylinder_height();
    fl::Grid<CRGB> rect(W, H);

    static void draw_pattern(uint32_t t_ms, fl::Grid<CRGB>& dst);
    static void project_to_strip(const fl::Corkscrew& c, const fl::Grid<CRGB>& src, CRGB* out, uint16_t n);

    void setup() {
        FastLED.addLeds<WS2812B, 5, GRB>(leds, NUM_LEDS);

        // Register a before_frame task that runs immediately before each render
        fl::task::before_frame().then([&](){
            uint32_t t = fl::time();
            draw_pattern(t, rect);
            project_to_strip(cork, rect, leds, NUM_LEDS);
        });
    }

    void loop() {
        // The before_frame task is invoked automatically at the right time
        FastLED.show();
        // Optionally pump other async work
        fl::async_yield();
    }
    ```

    - The manual approach gives explicit control each frame.
    - The `task::before_frame()` approach schedules work just‑in‑time before rendering without manual event wiring. Use `task::after_frame()` for post‑render work.

- **High‑definition HSV16**
  - Headers: `hsv16.h`, implementation in `hsv16.cpp`
  - `fl::HSV16` stores 16‑bit H/S/V for high‑precision conversion to `CRGB` without banding; construct from `CRGB` or manually, and convert via `ToRGB()` or implicit cast.
  - Color boost:
    - `HSV16::colorBoost(EaseType saturation_function, EaseType luminance_function)` applies a saturation‑space boost similar to gamma, tuned separately for saturation and luminance to counter LED gamut/compression (e.g., WS2812).

- **Easing functions (accurate 8/16‑bit)**
  - Header: `ease.h`
  - Accurate ease‑in/out functions with 8‑ and 16‑bit variants for quad/cubic/sine families, plus dispatchers: `ease8(type, ...)`, `ease16(type, ...)`.
  - Use to shape animation curves, palette traversal, or time alpha outputs.

- **Time alpha**
  - Header: `time_alpha.h`
  - Helpers for time‑based interpolation: `time_alpha8/16/f` compute progress in a window `[start, end]`.
  - Stateful helpers:
    - `TimeRamp(rise, latch, fall)` for a full rise‑hold‑fall cycle.
    - `TimeClampedTransition(duration)` for a clamped one‑shot.
  - Use cases: envelope control for brightness, effect blending, or gating simulation energy over time.

### JSON Utilities

- Safe JSON access: `json.h`

Why: Ergonomic, crash‑resistant access with defaulting operator (`|`). Prefer the `fl::Json` API for new code.

```cpp
fl::Json cfg = fl::Json::parse("{\"enabled\":true}");
bool enabled = cfg["enabled"] | false;
```

### I/O and Filesystem

- Basic I/O and streams: `io.h`, `ostream.h`, `istream.h`, `sstream.h`, `printf.h`
- Filesystem adapters: `file_system.h`

Why: Cross‑platform text formatting, buffered I/O, and optional file access on host builds.

### Algorithms and Utilities

- Algorithms and transforms: `algorithm.h`, `transform.h`, `range_access.h`
- Compile‑time utilities: `type_traits.h`, `tuple.h`, `variant.h`, `optional.h`, `utility.h`, `initializer_list.h`, `template_magic.h`, `types.h`, `stdint.h`, `namespace.h`
- Platform shims and control: `compiler_control.h`, `force_inline.h`, `virtual_if_not_avr.h`, `has_define.h`, `register.h`, `warn.h`, `trace.h`, `dbg.h`, `assert.h`, `unused.h`, `export.h`, `dll.h`

Why: Familiar patterns with embedded‑appropriate implementations and compiler‑portable controls.

### Audio and Reactive Systems

### UI System (JSON UI)

FastLED includes a JSON‑driven UI layer that can expose controls (sliders, buttons, checkboxes, number fields, dropdowns, titles, descriptions, help, audio visualizers) for interactive demos and remote control.

- **Availability by platform**
  - **AVR and other low‑memory chipsets**: disabled by default. The UI is not compiled in on constrained targets.
  - **WASM build**: enabled. The web UI is available and connects to the running sketch.
  - **Other platforms**: can be enabled if the platform supplies a bridge. See `platforms/ui_defs.h` for the compile‑time gate and platform includes.

- **Key headers and switches**
  - `platforms/ui_defs.h`: controls `FASTLED_USE_JSON_UI` (defaults to 1 on WASM, 0 elsewhere).
  - `fl/ui.h`: C++ UI element classes (UISlider, UIButton, UICheckbox, UINumberField, UIDropdown, UITitle, UIDescription, UIHelp, UIAudio, UIGroup).
  - `platforms/shared/ui/json/ui_manager.h`: platform‑agnostic JSON UI manager that integrates with `EngineEvents`.
  - `platforms/shared/ui/json/readme.md`: implementation guide and JSON protocol.

- **Lifecycle and data flow**
  - The UI system uses an update manager (`JsonUiManager`) and is integrated with `EngineEvents`:
    - On frame lifecycle events, new UI elements are exported as JSON; inbound updates are processed after frames.
    - This enables remote control: UI state can be driven locally (browser) or by remote senders issuing JSON updates.
  - Send (sketch → UI): when components are added or changed, `JsonUiManager` emits JSON to the platform bridge.
  - Receive (UI → sketch): the platform calls `JsonUiManager::updateUiComponents(const char*)` with a JSON object; changes are applied on `onEndFrame()`.

- **Registering handlers to send/receive JSON**
  - Platform bridge constructs the manager with a function that forwards JSON to the UI:
    - `JsonUiManager(Callback updateJs)` where `updateJs(const char*)` transports JSON to the front‑end (WASM example uses JS bindings in `src/platforms/wasm/ui.cpp`).
  - To receive UI updates from the front‑end, call:
    - `MyPlatformUiManager::instance().updateUiComponents(json_str)` to queue changes; `onEndFrame()` applies them.

- **Sketch‑side usage examples**

  Basic setup with controls and groups:

  ```cpp
  #include <FastLED.h>
  #include "fl/ui.h"

  UISlider brightness("Brightness", 128, 0, 255);
  UICheckbox enabled("Enabled", true);
  UIButton reset("Reset");
  UIDropdown mode("Mode", {fl::string("Rainbow"), fl::string("Waves"), fl::string("Solid")});
  UITitle title("Demo Controls");
  UIDescription desc("Adjust parameters in real time");
  UIHelp help("# Help\nUse the controls to tweak the effect.");

  void setup() {
      // Group controls visually in the UI
      UIGroup group("Main", title, desc, brightness, enabled, mode, reset, help);

      // Callbacks are pumped via EngineEvents; they trigger when values change
      brightness.onChanged([](UISlider& s){ FastLED.setBrightness((uint8_t)s); });
      enabled.onChanged([](UICheckbox& c){ /* toggle effect */ });
      mode.onChanged([](UIDropdown& d){ /* change program by d.as_int() */ });
      reset.onClicked([](){ /* reset animation state */ });
  }
  ```

  Help component (see `examples/UITest/UITest.ino`):
  ```cpp
  UIHelp helpMarkdown(
      R"(# FastLED UI\nThis area supports markdown, code blocks, and links.)");
  helpMarkdown.setGroup("Documentation");
  ```

  Audio input UI (WASM‑enabled platforms):
  ```cpp
  UIAudio audio("Mic");
  void loop() {
      while (audio.hasNext()) {
          auto sample = audio.next();
          // Use sample data for visualizations
      }
      FastLED.show();
  }
  ```

- **Platform bridge: sending and receiving JSON**
  - Sending from sketch to UI: `JsonUiManager` invokes the `updateJs(const char*)` callback with JSON representing either:
    - A JSON array of element definitions (on first export)
    - A JSON object of state updates (on changes)
  - Receiving from UI to sketch: the platform calls
    - `JsonUiManager::updateUiComponents(const char* jsonStr)` where `jsonStr` is a JSON object like:
      `{ "id_123": {"value": 200}, "id_456": {"pressed": true } }`
  - The manager defers application until the end of the current frame via `onEndFrame()` to ensure consistency.

- **WASM specifics**
  - WASM builds provide the web UI and JS glue (see `src/platforms/wasm/ui.cpp` and `src/platforms/wasm/compiler/modules/ui_manager.js`).
  - Element definitions may be routed directly to the browser UI manager to avoid feedback loops; updates are logged to an inspector, and inbound messages drive `updateUiComponents`.

- **Enabling on other platforms**
  - Implement a platform UI manager using `JsonUiManager` and provide the two weak functions the UI elements call:
    - `void addJsonUiComponent(fl::weak_ptr<JsonUiInternal>)`
    - `void removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>)`
  - Forward platform UI events into `updateUiComponents(jsonStr)`.
  - Ensure `EngineEvents` are active so UI updates are exported and applied on frame boundaries.

- Audio adapters and analysis: `audio.h`, `audio_reactive.h`
- Engine hooks: `engine_events.h`

Why: Build effects that respond to input signals.

### Miscellaneous and Specialized

- Wave simulation: `wave_simulation.h`, `wave_simulation_real.h`
- Screen and LED glue: `leds.h`, `screenmap.h`
- Path/visual experimentation: `corkscrew.h`

---

## Guidance for New Users

- If you are writing sketches: include `FastLED.h` and follow examples in `examples/`. The `fl::` headers power those features under the hood.
- If you are extending FastLED internals or building advanced effects: prefer `fl::` containers and `fl::span` over STL equivalents to maintain portability.
- Favor smart pointers and moveable wrappers for resource management. Avoid raw pointers and manual `delete`.
- Use `fl::Json` for robust JSON handling with safe defaults.

## Guidance for C++ Developers

- Treat `fl::` as an embedded‑friendly STL. Many concepts mirror the standard library but are tuned for constrained targets and consistent compiler support.
- When designing APIs, prefer non‑owning views (`fl::span<T>`) for input parameters and explicit ownership types for storage.
- Use `compiler_control.h` macros for warning control and portability instead of raw pragmas.

---

This README will evolve alongside the codebase. If you are exploring a specific subsystem, start from the relevant headers listed above and follow includes to supporting modules.
