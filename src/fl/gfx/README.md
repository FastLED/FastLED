# fl/gfx — 2D Graphics for LED Matrices

Antialiased lines, circles, and rings on rectangular pixel buffers.
One header, four functions, any pixel type.

```cpp
#include "fl/gfx/gfx.h"
```

---

## 1. Hello Canvas

A **canvas** wraps a pixel buffer you already have. It doesn't allocate.

```cpp
CRGB leds[256];                              // your buffer
fl::CanvasRGB canvas(leds, 16, 16);          // 16 wide, 16 tall

canvas.drawDisc(CRGB::Red, 8.0f, 8.0f, 4.0f);  // red circle at center
```

That's it. The pixels are written directly into `leds[]`.

---

## 2. Drawing Shapes

Every function takes **color first, then coordinates**. Coordinates can be
`float`, `int`, or fixed-point — see [Section 5](#5-coordinate-types).

### drawLine — thin line

```cpp
canvas.drawLine(CRGB::White, x0, y0, x1, y1);
```

Wu-style antialiased line. Diagonal lines get smooth subpixel blending.
Axis-aligned lines with integer coords produce crisp 1px strokes.

```cpp
// crisp horizontal line
canvas.drawLine(CRGB::White, 0, 8, 15, 8);

// smooth diagonal with subpixel positioning
canvas.drawLine(CRGB::White, 0.0f, 0.0f, 15.0f, 8.3f);
```

### drawDisc — filled circle

```cpp
canvas.drawDisc(color, cx, cy, radius);
```

Solid disc with a soft 1px antialiased edge. Pixels fully inside the
radius get full color. Pixels at the boundary get partial brightness.

```cpp
canvas.drawDisc(CRGB::Red, 8.0f, 8.0f, 4.5f);
```

### drawRing — hollow circle

```cpp
canvas.drawRing(color, cx, cy, innerRadius, thickness);
```

An annulus (donut). The band runs from `innerRadius` to
`innerRadius + thickness`. Both edges are antialiased. Pixels inside
`innerRadius` are not touched — the "hole" stays transparent.

```cpp
// ring centered at (8,8), inner radius 4, 2px wide band
canvas.drawRing(CRGB::Blue, 8.0f, 8.0f, 4.0f, 2.0f);
```

### drawStrokeLine — thick line

```cpp
canvas.drawStrokeLine(color, x0, y0, x1, y1, thickness);
canvas.drawStrokeLine(color, x0, y0, x1, y1, thickness, cap);
```

A line with width. The `thickness` is the full width (a thickness of 3
means 1.5px on each side of the centerline). Edges use distance-based
falloff for smooth antialiasing.

```cpp
// 3px wide line, default flat caps
canvas.drawStrokeLine(CRGB::Green, 2.0f, 8.0f, 14.0f, 8.0f, 3.0f);

// same line with round end caps
canvas.drawStrokeLine(CRGB::Green, 2.0f, 8.0f, 14.0f, 8.0f, 3.0f,
                      fl::LineCap::ROUND);
```

**End cap styles:**

| Style | What it does |
|-------|-------------|
| `FLAT` | Ends exactly at the endpoints (default) |
| `ROUND` | Semicircle caps extending past each endpoint |
| `SQUARE` | Rectangle caps extending half-thickness past each endpoint |

---

## 3. Working with the Canvas

### Creating

```cpp
// Most common — raw array
CRGB leds[256];
fl::CanvasRGB canvas(leds, 16, 16);

// Explicit span
fl::span<CRGB> buf(leds, 256);
fl::CanvasRGB canvas(buf, 16, 16);
```

`CanvasRGB` is a convenience alias for `Canvas<CRGB>`. For other pixel
types, use `Canvas<T>` directly (see [Section 6](#6-custom-pixel-types)).

### Reading and writing pixels

```cpp
canvas.at(3, 7) = CRGB::Red;       // set pixel at x=3, y=7
CRGB c = canvas.at(3, 7);          // read it back
```

### Bounds checking

```cpp
if (canvas.has(x, y)) { ... }      // true if 0<=x<width and 0<=y<height
int n = canvas.size();              // width * height
```

### Clearing between frames

Drawing is **additive** — each draw call adds to what's already there.
To start a fresh frame, zero the buffer yourself:

```cpp
memset(leds, 0, sizeof(leds));      // clear to black
canvas.drawDisc(CRGB::Red, ...);    // draw on clean slate
```

This is intentional. Additive blending lets you layer shapes naturally:

```cpp
memset(leds, 0, sizeof(leds));
canvas.drawDisc(CRGB(255, 0, 0), 6.0f, 8.0f, 3.0f);   // red circle
canvas.drawDisc(CRGB(0, 0, 255), 10.0f, 8.0f, 3.0f);   // blue circle
// overlap region becomes purple
```

---

## 4. Putting It Together

A typical animation loop:

```cpp
CRGB leds[256];
fl::CanvasRGB canvas(leds, 16, 16);

void loop() {
    memset(leds, 0, sizeof(leds));

    float t = millis() / 1000.0f;

    // bouncing disc
    float cx = 8.0f + 5.0f * sin(t);
    float cy = 8.0f + 5.0f * cos(t * 0.7f);
    canvas.drawDisc(CRGB::Red, cx, cy, 3.0f);

    // crosshair
    canvas.drawLine(CRGB(0, 80, 0), cx - 4.0f, cy, cx + 4.0f, cy);
    canvas.drawLine(CRGB(0, 80, 0), cx, cy - 4.0f, cx, cy + 4.0f);

    // pulsing ring
    float r = 2.0f + sin(t * 3.0f);
    canvas.drawRing(CRGB::Blue, 8.0f, 8.0f, r, 1.5f);

    FastLED.show();
}
```

---

## 5. Coordinate Types

All draw functions are templated on coordinate type. Use whatever fits:

| Type | When to use | Example |
|------|------------|---------|
| `float` | General purpose, smooth animation | `8.0f, 8.3f` |
| `int` | Pixel-aligned geometry, no AA needed | `0, 8, 15, 8` |
| `fl::s16x16` | No FPU (8-bit MCUs), deterministic math | `fl::s16x16(8.5f)` |

```cpp
// These all draw the same circle:

// float
canvas.drawDisc(CRGB::Red, 8.0f, 8.0f, 4.5f);

// int (snaps to pixel grid — no fractional positioning)
canvas.drawDisc(CRGB::Red, 8, 8, 4);

// fixed-point
fl::s16x16 cx(8.0f), cy(8.0f), r(4.5f);
canvas.drawDisc(CRGB::Red, cx, cy, r);
```

All coordinates in a single call must be the same type — you can't mix
`float` and `int` in one call.

---

## 6. Custom Pixel Types

`Canvas<T>` works with any pixel type that satisfies this contract:

```cpp
struct MyPixel {
    typedef SomeType fp;                        // channel precision type

    MyPixel& nscale8(fl::u8 scale);             // dim by scale/256
    MyPixel& operator+=(const MyPixel& rhs);    // saturating add
    // must be copyable: MyPixel c = other;
};
```

**What each method does:**

- **`nscale8(scale)`** — multiply all channels by `scale/256`. Called
  for antialiased edges to blend partial-brightness pixels.
- **`operator+=`** — add another pixel's color, clamping to max. Called
  by every draw operation to write into the buffer (additive blending).
- **`fp`** — the underlying channel type. `u8` for CRGB, `u8x8` for
  CRGB16. Used by higher-level code to choose precision-appropriate
  scaling.

### CRGB16 — 16-bit color

`CRGB16` uses `u8x8` (8.8 unsigned fixed-point) channels instead of
`u8`. That's 256x finer brightness steps per channel — useful when you
need smooth fades, HDR accumulation, or antialiased compositing without
banding.

```cpp
#include "fl/gfx/gfx.h"
#include "fl/gfx/crgb16.h"

using fl::CRGB16;
using fl::u8x8;

CRGB16 buffer[256];
fl::Canvas<CRGB16> canvas(fl::span<CRGB16>(buffer, 256), 16, 16);

CRGB16 red(u8x8(255.0f), u8x8(0.0f), u8x8(0.0f));

canvas.drawDisc(red, 8.0f, 8.0f, 4.5f);
canvas.drawLine(red, 0.0f, 0.0f, 15.0f, 8.0f);
canvas.drawRing(red, 8.0f, 8.0f, 5.0f, 2.0f);
canvas.drawStrokeLine(red, 2.0f, 8.0f, 14.0f, 8.0f, 3.0f);
```

CRGB16 also has `nscale(u8x8)` for full-precision scaling without
truncating to 8 bits.

---

## 7. Edge Cases

All primitives are safe with degenerate inputs — no crashes, no
out-of-bounds writes:

| Input | Behavior |
|-------|----------|
| Zero radius or thickness | Draws nothing |
| Zero-length line | Draws nothing |
| Center off-screen | Clips to canvas bounds |
| Shape partially off-screen | Only visible pixels are written |
| Negative coordinates | Clipped — no wrap-around |

---

## 8. How It Works

**Buffer layout.** Pixels are stored row-major: `buffer[y * width + x]`.
This gives sequential memory access when iterating rows — optimal for
cache performance on LED matrices.

**Antialiasing.** All primitives use distance-based antialiasing with a
half-pixel soft zone:

- `drawLine` — Wu's algorithm: each pixel along the line is split across
  two adjacent rows/columns based on fractional position.
- `drawDisc` — pixels within `radius ± 0.5` get brightness proportional
  to `(r_outer² - d²) / (r_outer² - r_inner²)`.
- `drawRing` — same AA math applied to both inner and outer edges.
- `drawStrokeLine` — perpendicular distance from centerline, with a
  precomputed 256-entry LUT for sqrt-based falloff.

**Performance.** The primitives iterate only the bounding box of each
shape (not the entire canvas). On a 32x32 canvas, 100 draw calls
complete in under 100ms — roughly 10,000 calls/sec in release mode.

---

## Files

| File | What's in it |
|------|-------------|
| `gfx.h` | Main include — brings in everything below |
| `canvas.h` | `Canvas<T>` struct and method declarations |
| `primitives.h` | Drawing algorithm implementations |
| `crgb16.h` | `CRGB16` pixel type with `u8x8` channels |
| `detail/` | Internal helpers (distance LUT, integer math) |
