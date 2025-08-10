## FastLED Core Library (src/fl)

FastLED has a cross‑platform, embedded‑friendly core that lives under `src/fl/`. This layer provides containers, algorithms, memory tools, math and color utilities, graphics primitives, basic async/concurrency, I/O, and portability shims. It’s designed to work consistently across microcontrollers and host builds without pulling in the C++ standard library on embedded targets.

This guide is for two audiences:
- New FastLED users who want a quick orientation and practical examples
- C++ developers who want to use the `fl::` APIs directly inside this repo

If you’re writing Arduino or PlatformIO sketches, you almost always just `#include <FastLED.h>` and follow the examples in `examples/`. The `fl::` headers power those features under the hood. If you’re building more advanced effects or working on internals, the `fl::` layer gives you a portable STL‑like toolkit.

---

## Quick start

- Use in sketches: include `FastLED.h` and explore `examples/` for complete, working sketches.
- Use inside the repo: include the specific `fl/` headers you need, e.g. `#include "fl/vector.h"`.
- Prefer non‑owning views for APIs: accept `fl::span<T>` instead of raw pointers or containers.
- Prefer `fl::unique_ptr` / `fl::shared_ptr` over raw `new`/`delete` when ownership is needed.
- Prefer `fl::memfill` over `memset` for memory fill operations in this codebase.

Common building blocks:
- Containers and views: `fl::vector<T>`, `fl::deque<T>`, `fl::span<T>`, `fl::slice<T>`
- Strings and streams: `fl::string`, `fl::ostream`, `fl::stringstream`, `fl::printf`
- Optionals and variants: `fl::optional<T>`, `fl::variant<...>`
- Ownership: `fl::unique_ptr<T>`, `fl::shared_ptr<T>`, `fl::weak_ptr<T>`
- Functional: `fl::function<Signature>`, `fl::function_list<Signature>`
- Async primitives: `fl::promise<T>`, `fl::task`
- Math and color: `fl::math`, `fl::sin32`, `fl::random`, `fl::gamma`, `fl::gradient`, `fl::hsv`
- Graphics: `fl::raster`, `fl::screenmap`, `fl::rectangular_draw_buffer`, `fl::downscale`, `fl::supersample`
- JSON: `fl::Json` with the defaulting operator `|`

---

## Small, practical examples

Using containers and views:

```cpp
#include "fl/vector.h"
#include "fl/span.h"

size_t count_nonzero(fl::span<const uint8_t> bytes) {
    size_t count = 0;
    for (uint8_t b : bytes) {
        if (b != 0) { ++count; }
    }
    return count;
}

void example() {
    fl::vector<uint8_t> data;
    data.push_back(1);
    data.push_back(0);
    data.push_back(3);

    size_t n = count_nonzero(fl::span<const uint8_t>(data));
    (void)n;
}
```

JSON with safe defaults:

```cpp
#include "fl/json.h"

void json_example(const fl::string& jsonStr) {
    fl::Json json = fl::Json::parse(jsonStr);
    int brightness = json["config"]["brightness"] | 128;  // default if missing
    bool enabled = json["enabled"] | false;
    (void)brightness; (void)enabled;
}
```

Gamma utility:

```cpp
#include "fl/gamma.h"

uint8_t apply_gamma(uint8_t v) {
    return fl::gamma::correct8(v);
}
```

Multicast callbacks with `function_list`:

```cpp
#include "fl/function_list.h"

void on_event(int code) { /* ... */ }

void register_handlers() {
    fl::function_list<void(int)> handlers;
    handlers.add(on_event);
    handlers(200); // invoke all
}
```

---

## Design principles and idioms

- Cross‑platform: consistent behavior across compilers and targets
- Header‑only where possible: easy to compose and reuse
- Embedded‑aware: minimal dynamic allocation; clear ownership
- Views over copies: prefer `fl::span<T>` for inputs
- Explicit lifetimes: use RAII types (`unique_ptr`, `shared_ptr`) when ownership is required
- Portability shims: use `fl/` headers for compiler/attribute control instead of raw pragmas

---

## What to reach for (by topic)

- Data structures: `vector`, `deque`, `queue`, `priority_queue`, `set`, `map`,
  `unordered_set`, `hash_map`, `hash_set`, `rbtree`, `bitset`, `bitset_dynamic`
- Views and ranges: `span`, `slice`, `range_access`
- Algebraic and utility types: `tuple`, `pair`, `optional`, `variant`, `types`, `type_traits`
- Memory and ownership: `unique_ptr`, `shared_ptr`, `weak_ptr`, `scoped_ptr`, `scoped_array`,
  `allocator`, `memory`, `memfill`, `inplacenew`
- Math, color, DSP: `math`, `math_macros`, `sin32`, `random`, `map_range`, `gamma`, `gradient`,
  `hsv`, `hsv16`, `fill`, `fft`, `fft_impl`, `time`, `time_alpha`, `noise_woryley`, `lut`, `clamp`, `clear`, `splat`, `transform`
- Graphics and geometry: `raster`, `raster_sparse`, `rectangular_draw_buffer`, `screenmap`, `tile2x2`,
  `xmap`, `xymap`, `xypath`, `xypath_impls`, `xypath_renderer`, `traverse_grid`, `grid`, `line_simplification`,
  `geometry`, `point`, `downscale`, `upscale`, `supersample`, `wave_simulation*`, `leds`, `ui`, `ui_impl`
- Concurrency and async: `thread`, `mutex`, `thread_local`, `promise`, `promise_result`, `task`, `async`
- Functional and events: `function`, `function_list`, `functional`, `engine_events`
- Text, I/O, JSON: `string`, `str`, `ostream`, `istream`, `sstream`, `strstream`, `printf`, `json`,
  `bytestream`, `bytestreammemory`, `io`, `file_system`, `fetch`
- Portability and control: `compiler_control`, `force_inline`, `virtual_if_not_avr`,
  `has_define`, `register`, `warn`, `trace`, `dbg`, `assert`, `unused`, `export`, `dll`, `deprecated`, `avr_disallowed`, `bit_cast`, `namespace`, `singleton`, `id_tracker`, `insert_result`, `template_magic`, `utility`, `move`, `initializer_list`, `stdint`, `cstddef`

---

## Where to go next

- Examples: browse `examples/` for complete sketches and effect patterns.
- Headers: open any `src/fl/*.h` to see implementations; most are small and composable.
- Tests: look in `tests/` for usage patterns and edge cases.

---

## Appendix — Full header index

- `algorithm.h`
- `align.h`
- `allocator.h`
- `array.h`
- `assert.h`
- `async.h`
- `atomic.h`
- `audio.h`
- `audio_reactive.h`
- `avr_disallowed.h`
- `bit_cast.h`
- `bitset.h`
- `bitset_dynamic.h`
- `blur.h`
- `bytestream.h`
- `bytestreammemory.h`
- `circular_buffer.h`
- `clamp.h`
- `clear.h`
- `colorutils.h`
- `colorutils_misc.h`
- `comparators.h`
- `compiler_control.h`
- `convert.h`
- `corkscrew.h`
- `cstddef.h`
- `dbg.h`
- `deprecated.h`
- `deque.h`
- `dll.h`
- `downscale.h`
- `draw_mode.h`
- `draw_visitor.h`
- `ease.h`
- `engine_events.h`
- `export.h`
- `fetch.h`
- `fft.h`
- `fft_impl.h`
- `file_system.h`
- `fill.h`
- `five_bit_hd_gamma.h`
- `force_inline.h`
- `function.h`
- `function_list.h`
- `functional.h`
- `gamma.h`
- `geometry.h`
- `gradient.h`
- `grid.h`
- `has_define.h`
- `hash.h`
- `hash_map.h`
- `hash_map_lru.h`
- `hash_set.h`
- `hsv.h`
- `hsv16.h`
- `id_tracker.h`
- `initializer_list.h`
- `inplacenew.h`
- `insert_result.h`
- `int.h`
- `io.h`
- `istream.h`
- `json.h`
- `leds.h`
- `line_simplification.h`
- `lut.h`
- `map.h`
- `map_range.h`
- `math.h`
- `math_macros.h`
- `memfill.h`
- `memory.h`
- `move.h`
- `mutex.h`
- `namespace.h`
- `noise_woryley.h`
- `optional.h`
- `ostream.h`
- `pair.h`
- `point.h`
- `printf.h`
- `promise.h`
- `promise_result.h`
- `priority_queue.h`
- `ptr.h`
- `ptr_impl.h`
- `queue.h`
- `random.h`
- `range_access.h`
- `raster.h`
- `raster_sparse.h`
- `rectangular_draw_buffer.h`
- `referent.h`
- `register.h`
- `rbtree.h`
- `scoped_array.h`
- `scoped_ptr.h`
- `screenmap.h`
- `set.h`
- `sin32.h`
- `singleton.h`
- `slice.h`
- `span.h`
- `splat.h`
- `sstream.h`
- `stdint.h`
- `str.h`
- `string.h`
- `strstream.h`
- `supersample.h`
- `task.h`
- `template_magic.h`
- `thread.h`
- `thread_local.h`
- `tile2x2.h`
- `time.h`
- `time_alpha.h`
- `trace.h`
- `transform.h`
- `traverse_grid.h`
- `tuple.h`
- `type_traits.h`
- `types.h`
- `ui.h`
- `ui_impl.h`
- `unique_ptr.h`
- `unordered_set.h`
- `unused.h`
- `upscale.h`
- `utility.h`
- `variant.h`
- `vector.h`
- `virtual_if_not_avr.h`
- `warn.h`
- `wave_simulation.h`
- `wave_simulation_real.h`
- `weak_ptr.h`
- `xmap.h`
- `xymap.h`
- `xypath.h`
- `xypath_impls.h`
- `xypath_renderer.h`
- `shared_ptr.h`
- `sketch_macros.h`