# LOOP2: Fixed-Point Library Investigation

## Goal

Determine the best strategy for fixed-point arithmetic in FastLED, driven by the concrete use case in `src/fl/fx/2d/chasing_spirals.hpp` and the current implementation in `src/fl/fixed_point/q16.hpp`.

## Context

FastLED currently has a hand-rolled `fl::Q16` struct (~140 lines) providing:
- Q16.16 arithmetic: `from_float`, `mul`, `div`
- Trigonometry: `sincos` (wrapping FastLED's `sin32`/`cos32`)
- LUT-accelerated 2D Perlin noise: `pnoise2d` with Q8.24 internals

The Q16 code is tightly optimized for the Chasing_Spirals animation (3.8x faster than float on x86, critical for ESP32). The question is whether to keep rolling our own, adopt a standard-track design, or pull in a third-party library.

## Evaluation Criteria (apply to every option)

1. **API fit**: Can it express Q16.16 naturally? Does it support mixed-precision (Q8.24 internal)?
2. **Code size**: Flash/RAM overhead on ESP32 (target: <2KB flash for the fixed-point layer)
3. **Performance**: Must not regress the 3.8x speedup. No virtual dispatch, no heap allocation.
4. **Portability**: Must work on ESP32, ESP32-S3, AVR (atmega328p), ARM (STM32), Teensy, x86.
5. **C++ standard**: Must compile as C++11 (project requirement).
6. **Header-only**: Strongly preferred (matches FastLED's architecture).
7. **License**: Must be compatible with MIT.
8. **Maintenance burden**: Will we need to patch it? How active is the upstream?
9. **Integration effort**: How much of `chasing_spirals.hpp` needs rewriting?

---

## Options to Investigate

### Option A: Keep custom `fl/fixed_point/` (status quo)
**Research file**: `research_option_a_custom.md`

Tasks:
1. Audit current `q16.hpp` for correctness edge cases (overflow in `mul`, `div` by zero, negative floor_frac).
2. Assess what additional fixed-point functionality FastLED might need beyond Chasing_Spirals (other animartrix effects, audio FFT, color math).
3. Evaluate naming: `Q16` vs `Q16n16` vs `Fixed16_16` vs `FixedPoint<16,16>` template.
4. Estimate ongoing maintenance cost — what happens when we need Q8.8 or Q1.15 for other effects?
5. Document pros/cons of a bespoke solution vs a general-purpose library.

### Option B: Adopt P0037/CNL design as `fl/stl/fixed_point.h`
**Research file**: `research_option_b_p0037_cnl.md`

Tasks:
1. Clone/read [CNL](https://github.com/johnmcfarlane/cnl) and evaluate its actual API surface for Q16.16 use.
2. Check C++ standard requirement — CNL targets C++17/20. Determine if it can be backported to C++11.
3. Measure code size: include CNL headers and check template instantiation bloat on a representative ESP32 build.
4. Check if CNL's `scaled_integer` can express mixed-precision (Q16.16 input → Q8.24 intermediate → Q16.16 output).
5. Evaluate if adopting CNL's design pattern (without the full library) for `fl::fixed_point<Rep, Exponent>` is feasible.
6. Assess the `fl/stl/` convention: does a failed proposal belong in `fl/stl/`? Or is `fl/fixed_point/` more honest?

### Option C: fpm (Fixed Point Math library)
**Research file**: `research_option_c_fpm.md`

Tasks:
1. Clone/read [fpm](https://github.com/MikeLankamp/fpm) — header-only, MIT license.
2. Check C++ standard: fpm targets C++11. Confirm it compiles on ESP32/AVR/ARM.
3. Evaluate API: can `fpm::fixed<int32_t, int64_t, 16>` replace `fl::Q16`? Does it support custom fractional bits?
4. Benchmark: rewrite `Chasing_Spirals_Q31` inner loop using fpm types and compare performance.
5. Check if fpm supports combined sincos, LUT-based Perlin noise, or if we'd still need custom code on top.
6. Evaluate fpm's operator overloading approach vs our explicit `Q16::mul()` — which is clearer for the codebase?
7. Assess integration: what goes in `third_party/fpm/`, what wrapping is needed?

### Option D: libfixmath
**Research file**: `research_option_d_libfixmath.md`

Tasks:
1. Read [libfixmath](https://github.com/PetteriAimworthy/libfixmath) (formerly `libfixmath` by flatmush).
2. Check license (MIT) and activity level.
3. Evaluate: libfixmath is Q16.16 specifically (`fix16_t`). Compare API to our `Q16`.
4. Check portability: libfixmath is C, which is good for AVR. But does it integrate cleanly with C++ code?
5. Evaluate trig functions: libfixmath has `fix16_sin`, `fix16_cos`. Compare precision and speed to our `sin32`-based approach.
6. Check if libfixmath has Perlin noise or if we'd need our own on top.
7. Assess code size on ESP32.

### Option E: Compositional template approach (`fl::FixedPoint<IntBits, FracBits>`)
**Research file**: `research_option_e_template.md`

Tasks:
1. Design a minimal template class `fl::FixedPoint<int IntBits, int FracBits>` that auto-selects `int16_t`, `int32_t`, or `int64_t` as backing type.
2. Evaluate if template approach adds value over concrete types — do we realistically need Q8.8, Q1.15, Q8.24 as distinct types?
3. Prototype the template and check if the compiler generates identical code to hand-rolled `Q16` (no abstraction penalty).
4. Assess compile-time cost on AVR (templates can bloat flash on 8-bit).
5. Determine if this can live in `fl/fixed_point/` as a project-specific utility.

---

## Execution Order

1. Execute research tasks A through E in parallel. Each writes its findings to the corresponding `research_option_*.md` file.
2. After all research files are complete, read all five reports.
3. Write `FINAL_RECOMMENDATION.md` with:
   - Summary comparison table (criteria x options)
   - Recommended approach with rationale
   - Migration plan if switching from current `fl::Q16`
   - Naming recommendation for the chosen type
