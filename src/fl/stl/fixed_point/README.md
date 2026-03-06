# Fixed-Point Arithmetic Library

Fast, integer-only fixed-point arithmetic. All operations use pure integer math—no floating-point overhead.

## What is Fixed-Point? (5-Minute Intro)

Fixed-point numbers store decimals using only **integers**. Think of it like storing money in cents instead of dollars:
- `$1.50` → store as `150` cents, then divide by 100 when needed
- `1.50` in fixed-point → store as a scaled integer, then divide by scale factor

**Example: How Fixed-Point Works**
```
Float: 2.5 (may be imprecise internally)
Fixed-Point s16x16: 2.5 stored as 163,840 (= 2.5 × 65,536)
           └─ 65,536 is 2^16 (the "scale factor")

Operations use only integers:
  163,840 + 65,536 = 229,376  → convert back to 3.5
  163,840 × 2      = 327,680  → convert back to 5.0
```

**Why use it?**
- ✅ **Fast**: Integer math is 5-100x faster than floating-point on embedded systems
- ✅ **Predictable**: No rounding errors; results are exact and reproducible
- ✅ **Small**: 2-4 bytes per value (vs 4-8 bytes for floats)
- ❌ **Trade-off**: Limited range and precision (you choose the split)

**Float vs Fixed-Point (Quick Comparison)**

| Aspect | Float | Fixed-Point |
|--------|-------|-------------|
| **Speed** | Slow (50-100x) — no FPU on embedded | ⭐ Fast (native integer) |
| **Precision** | ✅ Auto-adjusts per value | Must choose in advance |
| **Range** | Large | You pick (via template) |
| **Rounding errors** | May accumulate in loops | ❌ None — results exact |
| **Size** | 4 bytes (float), 8 bytes (double) | 2-4 bytes (your choice) |
| **Setup** | Just use `float` | Pick template parameters |

**→ Use fixed-point for FastLED because embedded systems have NO hardware float support.**

---

## The Two Main APIs: `sfixed_integer` and `ufixed_integer`

FastLED provides **two templated types** that are the primary entry point:

### Signed Fixed-Point Integer
```cpp
#include "fl/stl/fixed_point.h"

// sfixed_integer<IntBits, FracBits>
// Stores signed values (can be negative)

fl::sfixed_integer<16, 16> angle(3.14159f);  // ±32k range, 1/65k precision
fl::sfixed_integer<8, 24> smooth(0.5f);      // ±128 range, 1/16M precision
fl::sfixed_integer<24, 8> large(100000.5f);  // ±8M range, 1/256 precision
```

### Unsigned Fixed-Point Integer
```cpp
// ufixed_integer<IntBits, FracBits>
// Stores unsigned values (0 and positive only)

fl::ufixed_integer<8, 8> brightness(200.0f);  // 0-256 range, 1/256 precision
fl::ufixed_integer<16, 16> ratio(0.75f);      // 0-65k range, 1/65k precision
```

**Template parameters explained:**

| Parameter | What It Controls | Example |
|-----------|-----------------|---------|
| **IntBits** | How many bits for numbers before decimal point | `sfixed_integer<16, 16>`: stores up to ±32,768 |
| **FracBits** | How many bits for precision after decimal point | `sfixed_integer<16, 16>`: precision of 1/65,536 |

**The Formula:**
- **Range** (signed) = `[-2^IntBits, 2^IntBits)` (e.g., 16 bits → ±65,536)
- **Range** (unsigned) = `[0, 2^IntBits)` (e.g., 8 bits → 0 to 256)
- **Precision** = `1/2^FracBits` (e.g., 16 bits → 0.0000153 steps)
- **Total storage** = `(IntBits + FracBits)` rounded up to 8, 16, 32, or 64 bits

---

## Quick Examples (Start Here)

### Example 1: Fade LED Brightness
```cpp
#include "fl/stl/fixed_point.h"

// u8x8 = unsigned, 8-bit integer, 8-bit fraction
// Range: [0, 256), Precision: 1/256
fl::ufixed_integer<8, 8> brightness(200.0f);  // Start at 200

// Multiply brightness by 0.5 (50% fade)
// All math stays in fixed-point (no slow floats!)
fl::ufixed_integer<8, 8> half = fl::ufixed_integer<8, 8>(0.5f);
fl::ufixed_integer<8, 8> faded = brightness * half;

uint8_t output = faded.to_int();  // Convert back to int: 100
// ✅ Result: 200 × 0.5 = 100 (fast integer-only math)
```

### Example 2: Smooth Animation
```cpp
// Animate from 0 to 1 with easing
fl::sfixed_integer<16, 16> t(0.3f);  // Time: 0.0 to 1.0

// Smooth step (hermite interpolation)
auto eased = fl::smoothstep(
    fl::sfixed_integer<16, 16>(0.0f),
    fl::sfixed_integer<16, 16>(1.0f),
    t);

uint8_t hue = (eased * 255.0f).to_int();
```

### Example 3: Rotate a Point
```cpp
struct Point {
    fl::sfixed_integer<16, 16> x, y;
};

Point rotate(const Point& p, fl::sfixed_integer<16, 16> angle) {
    fl::sfixed_integer<16, 16> sin_a, cos_a;
    fl::sincos(angle, sin_a, cos_a);

    return {
        p.x * cos_a - p.y * sin_a,
        p.x * sin_a + p.y * cos_a
    };
}
```

### Example 4: Perlin Noise (Animartrix)
```cpp
// Used in 2D effects
fl::sfixed_integer<16, 16> x(0.5f);
fl::sfixed_integer<16, 16> y(0.5f);

// Noise returns [-1, 1]
auto pattern = perlin_s16x16::pnoise2d(x, y, ...);

uint8_t led = ((pattern + 1.0f) * 128.0f).to_int();
```

---

## Understanding Range vs Precision (Key Concept)

Fixed-point forces a **trade-off**: You have a fixed number of bits (typically 32 or 64). You split them between integer and fractional parts. More bits for integers = larger range but coarser precision.

**Visual example (32-bit fixed-point):**

```
s16x16 (16 bits integer, 16 bits fraction)
  ├─ Range: ±32,767
  └─ Precision: 1/65,536 ≈ 0.0000153

s8x24 (8 bits integer, 24 bits fraction)
  ├─ Range: ±127
  └─ Precision: 1/16,777,216 ≈ 0.000000059

s24x8 (24 bits integer, 8 bits fraction)
  ├─ Range: ±8,388,607
  └─ Precision: 1/256 ≈ 0.0039
```

**When to choose each:**
- **Need values > 1000?** → Use more IntBits (`s24x8`)
- **Need precision < 0.001?** → Use more FracBits (`s8x24`)
- **Middle ground?** → **Stick with `s16x16`** — best for most use cases

**Rarely need to think about this** — `s16x16` handles angles, brightness, coordinates, and most animation parameters just fine.

---

## Creating and Converting Values

### From Floats (Most Common)
```cpp
// Initialize from float — compiler does the scaling automatically
fl::sfixed_integer<16, 16> angle(3.14159f);      // 3.14159
fl::ufixed_integer<8, 8> brightness(200.0f);     // 200.0
fl::sfixed_integer<8, 24> precise(0.1f);         // 0.1 (high precision)
```

### Converting Back
```cpp
auto value = fl::sfixed_integer<16, 16>(1.5f);

float f = value.to_float();       // → 1.5f (exact)
int i = value.to_int();           // → 1 (truncated, loses 0.5)
auto raw = value.raw();           // → 98304 (internal scaled integer)
                                  //   (1.5 × 65536)
```

### From Raw Values (Advanced/Rare)
```cpp
// If you need to pack/unpack binary data:
auto value = fl::sfixed_integer<16, 16>::from_raw(0x10000);  // 1.0
// 0x10000 (65,536) = 1.0 in s16x16 because 2^16 is the scale
```

### Auto-Promotion (Smaller → Larger, Automatic)
```cpp
fl::ufixed_integer<8, 8> small(1.5f);       // 8+8 = 16 bits
fl::ufixed_integer<16, 16> large = small;   // ✅ Auto-promotes!
// Compiler shifts bits to expand range and precision

// Promotion rules (must ALL be true):
// ✅ u8x8 → u16x16  (more int bits + more frac bits)
// ✅ s8x24 → s16x16 (more int bits, same or fewer frac bits)
// ❌ u8x8 → s16x16  (sign mismatch: unsigned → signed)
// ❌ s16x16 → s8x24 (int bits shrink: 16 → 8)
```

---

## Math API: Free-Function Style (Like std::cmath)

### Rounding & Decomposition
```cpp
auto x = fl::sfixed_integer<16, 16>(2.7f);

fl::floor(x);   // 2.0
fl::ceil(x);    // 3.0
fl::fract(x);   // 0.7
fl::abs(x);     // Absolute value
```

### Interpolation & Clamping
```cpp
auto a = fl::sfixed_integer<16, 16>(0.0f);
auto b = fl::sfixed_integer<16, 16>(10.0f);
auto t = fl::sfixed_integer<16, 16>(0.3f);

fl::lerp(a, b, t);           // Linear interpolation: 3.0
fl::clamp(fl::sfixed_integer<16, 16>(15.0f), a, b);  // → 10.0
fl::smoothstep(a, b, t);     // Smooth: 0→1
fl::step(a, t);              // Step function
```

### Trigonometry (Ultra-Fast Lookup Tables)
```cpp
auto angle = fl::sfixed_integer<16, 16>(1.5708f);  // π/2

// **Use sincos() instead of separate calls (30% faster)**
auto sin_val = fl::sfixed_integer<16, 16>(0.0f);
auto cos_val = fl::sfixed_integer<16, 16>(0.0f);
fl::sincos(angle, sin_val, cos_val);

// Individual functions
fl::sin(angle);      // ~1.0
fl::cos(angle);      // ~0.0
fl::atan(0.7f);
fl::atan2(1.0f, 0.7f);  // Angle from (x, y)
fl::asin(0.7f);
fl::acos(0.7f);
```

### Roots & Powers
```cpp
auto x = fl::sfixed_integer<16, 16>(4.0f);

fl::sqrt(x);         // ~2.0
fl::rsqrt(x);        // ~0.707 (1/√x)
fl::pow(fl::sfixed_integer<16, 16>(2.0f), x);  // 2^4 = 16.0
```

### Arithmetic Operators
```cpp
auto a = fl::sfixed_integer<16, 16>(2.0f);
auto b = fl::sfixed_integer<16, 16>(0.5f);

a + b;          // 2.5
a - b;          // 1.5
a * b;          // 1.0 (fixed-point multiply)
a / b;          // 4.0 (fixed-point divide)
-a;             // -2.0

// Fast power-of-2 operations
a >> 1;         // Divide by 2
a << 2;         // Multiply by 4

// Comparisons
a < b;
a == b;
a >= b;
```

### Modulo
```cpp
auto a = fl::sfixed_integer<16, 16>(7.5f);
auto b = fl::sfixed_integer<16, 16>(2.0f);

fl::mod(a, b);  // ≈ 1.5
```

---

## Member Method API (Alternative)

You can also call methods directly on values (both styles work):

```cpp
auto x = fl::sfixed_integer<16, 16>(2.7f);

x.floor();
x.ceil();
x.fract();
x.abs();
x.sqrt();
x.sin();
x.cos();
```

Use whichever is more natural for your code—free-function (`fl::sin(x)`) or member (`x.sin()`).

---

## Choosing a Template Configuration

**Decision tree (read top to bottom):**

```
1. Is your value small (|x| < 128)?
   ├─ Need very high precision? → s8x24  (precision 1/16M, fits in 4 bytes)
   └─ Precision is OK? → s8x8 or u8x8   (precision 1/256, fits in 2 bytes)

2. Is your value medium (|x| < 32k)?  ⭐ MOST COMMON
   └─ → s16x16 or u16x16  (precision 1/65,536, fits in 4 bytes)

3. Is your value large (|x| < 8M)?
   └─ → s24x8  (precision 1/256, fits in 4 bytes)

4. Sign question: Can your value be negative?
   ├─ Yes → Use sfixed_integer (signed)
   └─ No  → Use ufixed_integer (unsigned, saves 1 bit range)
```

**Quick lookup by use case:**

| Use Case | Type | Range | Precision | Storage |
|----------|------|-------|-----------|---------|
| Brightness (0-255) | `u8x8` | 0-256 | 1/256 | 2 bytes |
| Angles (0-2π radians) | `s16x16` | ±32k | 1/65k | 4 bytes |
| Smooth fade (0.0-1.0) | `s16x16` | ±32k | 1/65k | 4 bytes |
| High precision small (±1.0) | `s8x24` | ±128 | 1/16M | 4 bytes |
| Spatial coordinates | `s16x16` | ±32k | 1/65k | 4 bytes |
| Large numbers (±1M) | `s24x8` | ±8M | 1/256 | 4 bytes |

---

## Convenience Type Aliases

If the full template syntax is too verbose, short aliases are available:

| Alias | Expands To | Use Case |
|-------|-----------|----------|
| `s16x16` | `sfixed_integer<16, 16>` | General signed |
| `u8x8` | `ufixed_integer<8, 8>` | Brightness |
| `s8x24` | `sfixed_integer<8, 24>` | High precision, small range |
| `s24x8` | `sfixed_integer<24, 8>` | Large values, low precision |

**Full list:** `s0x32`, `s4x12`, `s8x8`, `s8x24`, `s12x4`, `s16x16`, `s24x8`, `u0x32`, `u4x12`, `u8x8`, `u8x24`, `u12x4`, `u16x16`, `u24x8`

---

## Performance Characteristics

**On embedded systems (ARM Cortex-M), fixed-point is 5-100x faster:**
- **Addition**: 5-10x faster (native integer)
- **Multiplication**: 10-50x faster (no FPU pipeline stalls)
- **sin/cos**: 50-100x faster (lookup table vs approximation)
- **sqrt**: 5-10x faster (integer algorithm)

**Why?** Embedded CPUs lack FPU, so floats use slow software emulation. Fixed-point uses native integer operations.

**Memory:** Highly efficient—`s16x16` is 4 bytes, `u8x8` is 2 bytes.

---

## Common Pitfalls and Solutions

| Problem | What Happens | Solution |
|---------|--------------|----------|
| **Overflow** | Value exceeds range (e.g., 50,000 in `s16x16` which maxes at ±32,767) | Use wider template: `s24x8` instead of `s16x16` |
| **Division by zero** | Undefined behavior / crash | Always check: `if (divisor != 0) { result = a / b; }` |
| **Wrong precision** | Result is too coarse (e.g., 0.004 steps instead of needed 0.001) | Choose FracBits carefully. `s16x16` gives 0.0000153 precision; `s8x24` gives 0.00000006 |
| **Type mismatch** | Won't compile: `u8x8 + s16x16` (unsigned + signed) | Cast smaller to larger: `s16x16(small) + large` (or use `auto` to promote) |
| **Precision loss** | `.to_int()` truncates decimals: `2.7.to_int()` → `2` | Use `.to_float()` if you need decimals; or just keep as fixed-point |
| **Slow operations** | `x / 2` is slower than it needs to be for powers of 2 | Use bit shifts: `x >> 1` (÷2), `x << 2` (×4) — **5-10x faster** |
| **Lost decimals** | Assigning to `uint8_t` directly: `uint8_t val = faded;` (no conversion) | Use `.to_int()`: `uint8_t val = faded.to_int();` |

---

## Real FastLED Usage

### Easing Functions (`fl/ease.h`)
Smooth animations using fixed-point internally for speed:
```cpp
auto t = fl::sfixed_integer<16, 16>(0.5f);  // Time parameter

auto eased = fl::smoothstep(
    fl::sfixed_integer<16, 16>(0.0f),
    fl::sfixed_integer<16, 16>(1.0f),
    t);

uint8_t brightness = (eased * 255.0f).to_int();
```

### Animartrix 2D Effects (`src/fl/fx/2d/animartrix_detail/`)
Stores geometry as fixed-point for fast operations:
```cpp
struct Geometry {
    fl::sfixed_integer<16, 16> distance;   // Polar distance
    fl::sfixed_integer<16, 16> angle;      // Polar angle (radians)
    fl::sfixed_integer<16, 16> x, y;       // Cartesian coordinates
};

// Fast rotation with precomputed sin/cos
fl::sfixed_integer<16, 16> sin_a, cos_a;
fl::sincos(angle, sin_a, cos_a);

auto rotated_x = cos_a * x - sin_a * y;
auto rotated_y = sin_a * x + cos_a * y;
```

### Perlin Noise (2D Patterns)
Smooth terrain-like patterns for animations:
```cpp
auto noise = perlin_s16x16::pnoise2d(x, y, fade_lut, perm);
// Returns [-1, 1], converted to LED brightness
```

---

## SIMD Versions (4x Parallel Processing)

For vectorized loops processing 4 values at once:

```cpp
#include "fl/stl/fixed_point.h"

// 4-wide SIMD versions (4x parallelism)
// s16x16x4, u8x8x4, s8x24x4, etc.

auto angles = fl::sfixed_integer<16, 16>::x4(1.0f);  // Broadcast to 4 lanes

fl::sin(angles);   // sin on all 4 lanes
fl::cos(angles);   // cos on all 4 lanes
```

---

## API Quick Reference

### Creation
```cpp
fl::sfixed_integer<16, 16> x(3.14f);
fl::ufixed_integer<8, 8> y(200.0f);

// From raw
auto z = fl::sfixed_integer<16, 16>::from_raw(0x10000);
```

### Conversion
```cpp
float f = x.to_float();
int i = x.to_int();
auto raw = x.raw();
```

### Free-Function Math
```cpp
fl::sin(x), fl::cos(x), fl::sqrt(x)
fl::floor(x), fl::ceil(x), fl::fract(x)
fl::lerp(a, b, t), fl::clamp(x, lo, hi)
fl::smoothstep(e0, e1, x)
fl::sincos(angle, sin_out, cos_out)
```

### Arithmetic
```cpp
a + b, a - b, a * b, a / b, -a
a >> 1, a << 2
a < b, a == b, a >= b
```

### Member Methods (Alternative)
```cpp
x.sin(), x.cos(), x.sqrt()
x.floor(), x.ceil(), x.fract()
x.abs()
```

---

## Getting Started (Beginner Checklist)

**✅ Step 1: Use the default configuration**
- Start with **`fl::sfixed_integer<16, 16>`** or its alias **`fl::s16x16`**
- It works for 95% of use cases (angles, brightness, coordinates, easing)

**✅ Step 2: Copy an example above**
- Pick Example 1 or 2 and try it in your code
- Replace the floats with fixed-point variables
- Compile and check that it works

**✅ Step 3: Convert back to output types**
- Use `.to_int()` to convert to `uint8_t` for LED brightness
- Use `.to_float()` only if you need to pass to a float-expecting function
- Keep as fixed-point for intermediate calculations (faster!)

**✅ Step 4: If you need different precision or range**
- Check the lookup table in "Choosing a Template Configuration"
- Change `<16, 16>` to the appropriate type
- That's it — API is identical

**✅ Step 5: Test it**
```bash
bash test fixed_point
```

## Advanced: Next Steps

1. **Run the test suite**: `bash test fixed_point` — validates all implementations
2. **Look at real usage**: Search for `s16x16` in `src/fl/fx/2d/animartrix_detail/` for production examples
3. **Check accuracy bounds**: `tests/fl/fixed_point.cpp` for type-specific precision guarantees
4. **Benchmark your code**: Use profiling if performance matters (fixed-point should be plenty fast)

## Key Files

- **Header**: `fl/stl/fixed_point.h` — Main template APIs and free-function math
- **Aliases**: `fl/stl/fixed_point/s16x16.h`, `u8x8.h`, etc.
- **Tests**: `tests/fl/fixed_point.cpp` — 197+ test groupings with accuracy bounds
- **Internals**: `fl/stl/fixed_point/base.h` — CRTP implementation
