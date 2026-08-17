# AnimartrixRing

**Minimal demo of one technique: sampling a circular LED ring out of a 2D Animartrix grid.**

```text
Animartrix (16x16 grid) -> ScreenMap (circle of 244 points)
                        -> Fx2dTo1d (bilinear sample) -> 1D ring
```

That is the entire sketch. Pick an animation from the dropdown, scrub the speed
slider, and see how each Animartrix pattern reads once it has been wrapped
around a ring.

## Files

| File | Purpose |
|------|---------|
| `AnimartrixRing.ino` | The demo: grid, ring ScreenMap, `Fx2dTo1d`, UI |
| `ring_screenmap.{h,cpp}` | Builds the circular `ScreenMap` inside a rectangular grid |
| `auto_brightness.{h,cpp}` | Content-aware brightness compression |

## Run it

```bash
pip install fastled
cd examples/AnimartrixRing
fastled
```

## Looking for the audio-reactive version?

It moved to [`examples/MoodRing/`](../MoodRing/). This sketch deliberately has
no audio: it is the technique demo, and audio belongs to the product sketch.
See [issue #2256](https://github.com/FastLED/FastLED/issues/2256).
