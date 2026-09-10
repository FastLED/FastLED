# WS2812-class 8-bit shaping paths: what each one costs on its own (P8)

Section 5 of the colour-pipeline spec asks which strategy best spends a
WS2812's eight bits: independent RGB gamma, value-only shaping, the HUE16/HSV
path, `colorBoost()`, or linear-light temporal dithering, scored on dark-level
hue and neutral-axis error rather than on smoothness.

That comparison cannot be run until each candidate is characterised *alone*. A
path that moves hue or tints neutrals by itself is not a re-encoding, and no
amount of smoothness makes it one. This document is that first step, and it
removes two of the five candidates.

Harness: the `ws2812_shaping` section of `tests/fl/gfx/hsv16.cpp`. It is grouped
into that file rather than standing alone because the paths under test --
`HSV16::ToRGB` and `CRGB::colorBoost` -- both live in `fl/gfx/hsv16.h`, and the
repo prefers consolidating tests over paying another file's compile time.

## Why this runs in C++

`#4042` left the harness's shape open, and the reason was specific:

> `colorBoost()`, the HUE16/HSV path and the gamma LUTs are real
> implementations in `src/fl/gfx/`, and re-modelling them in Python risks
> scoring a paraphrase rather than the code that ships.

So the paths here *are* the shipping functions, called directly. The scoring
is the shipping colorimetry too — `rgb_source_to_XYZ` against the WS2812B
profile, then `xyzToOklabQ16` — rather than a second copy of it. Nothing in
this study is a model of FastLED; it is FastLED, measured.

Metrics are OKLab-based because the pipeline's own objective is defined in
OKLCh (A3). A1's ΔE2000 budget is a whole-pipeline claim and is checked
against the P5 reference elsewhere; it is not what distinguishes these paths.

## Method

360 samples: twelve hues, three saturations (96, 176, 255) and ten levels
from code 1 to 255, weighted into the bottom of the range because that is
where the earlier baseline measurement showed the error lives — linear
quantisation is already inside a quarter of a ΔE2000 above about ten percent
drive.

Two details that decide whether the numbers mean anything:

* **Saturation has to vary.** A fully saturated sweep makes
  `colorBoost(EASE_IN_QUAD, …)` a no-op — there is no headroom left to boost —
  and it then scores as perfectly colour-preserving, for the one reason that
  says nothing about it. The first version of this harness had that bug and
  reported a chroma gain of exactly 1.000x.
* **Hue is screened on visible chroma.** Hue is ill-conditioned near the
  neutral axis, so a path that merely desaturates would otherwise report a
  huge drift that is arithmetic rather than appearance. Only samples staying
  above 0.01 OKLab chroma on both sides are scored, and the count of scored
  samples is asserted so the screen cannot quietly empty the set.

## Result

| path | worst hue drift | chroma ratio | collapses to black |
| --- | --- | --- | --- |
| identity (control) | 0.000 deg | 1.000x | 0 / 360 |
| HSV16 round trip | **0.649 deg** | 0.973x .. 1.003x | 0 / 360 |
| `colorBoost(EASE_IN_QUAD, EASE_NONE)` | **142.969 deg** | 0.170x .. 6.479x | 0 / 360 |
| `colorBoost(EASE_NONE, EASE_IN_QUAD)` | 143.798 deg | 0.288x .. 2.123x | **254 / 360** |

### The HSV16 round trip is colour-preserving

Under two thirds of a degree of hue, and under 3% chroma either way, across
the whole sweep including the darkest codes. The worst case is a single code
of green:

```
(83, 226, 25) -> (83, 227, 25)
```

So the HUE16 path can carry a shaping strategy without contributing error of
its own. It stays a candidate.

### `colorBoost` is an appearance control, not an encoding

The worst case is small enough to check by hand, which is why it is worth
quoting:

```
(1, 3, 1) -> (0, 3, 0)
```

A dim, lightly saturated colour becomes *pure green*. Both minor channels are
zeroed, and the hue moves 143 degrees. This is not the near-axis
ill-conditioning described above — the sample stays visibly chromatic on both
sides, and all 360 samples survive the chroma screen.

The mechanism is straightforward once seen: easing saturation upward at low
codes has nothing to work with, and rounding takes the minor channels to
zero. The range where section 5 needs a strategy is exactly the range where
`colorBoost` destroys the colour.

**Ruled out** as a hue-preserving 8-bit shaping stage. It remains a perfectly
good appearance control, which is what it was written to be — it changes the
colour on purpose, and this document exists so a later comparison cannot
mistake it for a neutral re-encoding.

### Luminance easing is a dimming curve

`colorBoost(EASE_NONE, EASE_IN_QUAD)` drives **254 of 360 samples to black**.
Quadratic easing on an eight-bit value annihilates dark content. Also **ruled
out**; it is a brightness curve and belongs with brightness, not with
encoding.

### The neutral axis leaves the axis before any path touches it

Measured on a real D65 neutral target — solved to drives, then rounded —
rather than on equal codes:

| | worst OKLab chroma on a neutral |
| --- | --- |
| identity | **0.0605**, at 2% luminance |
| HSV16 round trip | 0.0605 (identical) |
| `colorBoost` luminance easing | 0.0676 (worse) |

**0.0605 of chroma on what should be a neutral, from rounding three drives to
eight bits.** Neither existing path improves it and the boost makes it worse,
so this is the number a dithering strategy has to beat — it is the whole
opportunity at the dark end of the neutral axis.

That the metric is evaluated on a solved neutral and not on equal codes is
load-bearing. This profile normalises each emitter to unit luminance, so an
equal-code triple is neither D65 nor unit luminance; scoring it measures the
device normalisation instead of the path, and produces numbers that look
alarming and mean nothing. The test pins the correct form — substituting equal
drives fails it.

## The ceiling for a static strategy is above what the pipeline reaches

Before comparing shaping functions it is worth knowing what the best possible
static answer is, because every candidate sits under it. The output is one of
256^3 code triples and the best is whichever lands nearest the target, so that
bound is computable rather than a matter of taste.

I expected round-to-nearest to *be* that bound -- that the 0.0605 above was
the lattice rather than the rule. It is not.

`quantize_u8` rounds each drive independently, minimising error in **drive**
space. The three drives carry very different perceptual weight, so the nearest
drive triple is not the nearest colour. Searching a +/-2 neighbourhood in
OKLab over 40 neutral luminances from 1% to 40%:

| | |
| --- | --- |
| rounding was optimal | **13 of 40** |
| worst shortfall | **0.0115** OKLab distance |

### Distance is not chroma, and the two do not always agree

The search minimises Euclidean OKLab distance, which includes lightness. A
candidate can therefore land closer to the ideal neutral overall while sitting
*further* off the neutral axis, so "closer" and "less off-axis" are different
claims. An earlier revision of this section quoted the distance shortfall
against the 0.0605 of neutral chroma as though they were the same quantity;
they are not, and measuring chroma directly gives a better answer anyway.

Over the same 40 targets, comparing the chroma of the distance-optimal code
against the rounded one:

| | |
| --- | --- |
| chroma improved | 23 |
| unchanged | 13 |
| **chroma got worse** | **4** |

The largest of those four goes 0.01076 -> 0.01230 while total distance
improved -- exactly the case the objectives coming apart predicts.

Where it matters most they agree. At the worst target -- 2% luminance, the one
the neutral sweep reports at **0.0605** -- the distance-optimal code measures
**0.0382**, a 37% reduction in chroma.

Two things this does not say. A 125-candidate search does not belong on the
per-pixel path -- it plainly does not, and nothing here proposes it. And it
does not name a cheap rule that captures the gain; finding one is work this
has not done. What it establishes is that the remaining candidates are being
compared against a floor that is lower than it needs to be, which is worth
knowing before spending effort ranking them.

## What this leaves for section 5

Two of the five candidates are gone, and the remaining comparison is between
independent RGB gamma, value-only shaping over the HSV16 path, and
linear-light temporal dithering, with 0.0605 OKLab chroma at 2% luminance as
the neutral-axis target to beat.

## Not covered

Temporal dithering, which is stateful and needs the frame cadence P6 wires up;
it is a separate P8 deliverable. The gamma LUT paths are not scored here
either — they are shaping *strategies* rather than fixed paths, and scoring
them requires the strategy-space definition that the section-5 comparison
proper has to settle.

Only the WS2812B placeholder profile is used. It is marked
`uncalibrated`; the numbers describe the quantisation geometry, not a measured
part. P10 owns measured profiles.
