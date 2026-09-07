# Gamut-mapping algorithm selection (P7)

Issue #4041 asks for a comparison of clipping, max-normalization, OKLCh compression
and a constrained solve, in order to "pick the smallest algorithm meeting the
A1 budget on embedded hardware". This records that comparison and the choice
it forces.

Harness: `ci/color_gamut_study.py`. Regression test:
`ci/tests/test_color_gamut_study.py`.

## Method

Each candidate maps an out-of-gamut XYZ target onto the device hull. Scores
are CIEDE2000 against **the P5 reference's own mapped result**, so a number
answers "how far from the reference objective does this land", not "is this a
pleasing colour".

Corpus: the 20 out-of-gamut vectors of the `rgb` device (sRGB primaries at
unit luminance) in `ci/golden/color-reference-v1.json`. Normalization is A1's:
relative colorimetry, profile white at full drive, dark surround.

## Result

| algorithm | worst ΔE2000 | mean ΔE2000 |
| --- | --- | --- |
| clip drives into [0, 1] | 20.652 | 3.923 |
| max-normalize | 20.652 | 3.923 |
| desaturate toward neutral | 14.893 | 2.101 |
| **OKLCh chroma compression** | **0.000** | **0.000** |

A1's budget is max ΔE2000 ≤ 0.5 at identity brightness.

## Cost: how much search is actually needed

The reference and the unbounded bisection both run to convergence, which no
per-pixel path can afford. Two bounded options were measured over the same 20
vectors.

**Fixed iteration count**, no table:

| halvings | worst ΔE2000 | mean ΔE2000 |
| --- | --- | --- |
| 2 | 10.149 | 3.166 |
| 4 | 1.604 | 0.590 |
| 6 | 0.623 | 0.214 |
| **8** | **0.152** | **0.046** |
| 10 | 0.030 | 0.010 |

**LUT of maximum chroma over an (L, hue) grid**, two variants -- one
interpolating and shrinking on infeasibility, one storing the per-cell
minimum so a lookup never overshoots:

| grid | table bytes (u16) | worst, shrink-on-miss | worst, conservative |
| --- | --- | --- | --- |
| 8 x 16 | 256 | 4.406 | 10.992 |
| 16 x 32 | 1 024 | 4.373 | 7.631 |
| 32 x 64 | 4 096 | 2.180 | 4.621 |
| 64 x 128 | 16 384 | -- | 2.434 |

## Selection: eight halvings, no table

Eight halvings meet A1 with about a 3x margin and need **no table at all**.
A 16 KB LUT scores 2.434, which is worse than *four* halvings -- so the
"optionally LUT-backed" option in #4041 resolves to no for this budget.

The reason is geometric: the OKLCh gamut boundary has sharp corners at the
primaries, and a grid cannot represent a corner without enormous resolution.
Refining along the ray costs nothing to store and lands on the corner
directly.

Eight halvings is also a *bounded* computation rather than an iterative
solver in the A3/B11 sense. Its cost is fixed at compile time; it does not
loop until a convergence criterion is met, which is the property that makes
`nnls3` unacceptable per pixel.

## What this means



**The objective is what matters; the search is not.** OKLCh chroma
compression implemented as a plain 30-iteration bisection reproduces the
reference exactly on this corpus, even though the reference searches the
zonotope globally with cubic root-finding. The elaborate search is insurance
against a case this corpus does not contain (see below), not a source of
accuracy on the cases it does.

**No amount of clamping substitutes for the objective.** Clip and
max-normalize are identical here — for these targets the clamped drives never
exceed full scale, so the normalization step never fires — and both land ~20
ΔE2000 from the reference, forty times the budget. Desaturating toward the
equal-luminance neutral is better but still ~30x over, because the straight
line to neutral in XYZ is not a line of constant hue.

So the embedded path must implement the OKLCh objective. That is the finding:
the cheap options are not "slightly worse", they are not in the same range,
and A1 cannot be met by clamping.

## Precision: s16.16 is enough, and it is already there

The selected algorithm still has to survive the working domain's arithmetic.
Every intermediate was quantized to a fixed number of fractional bits --
including the branch decisions inside the lightness search, not merely its
result -- and the mapping was then re-scored:

| fraction bits | worst ΔE2000 | mean ΔE2000 | infeasible results |
| --- | --- | --- | --- |
| 8 | 4.861 | 0.804 | 0 |
| 10 | 1.467 | 0.368 | 0 |
| 12 | 1.504 | 0.262 | 0 |
| 14 | 0.420 | 0.100 | 0 |
| **16** | **0.152** | **0.052** | 0 |

At 16 fractional bits the fixed-point mapping is indistinguishable from the
float64 one -- both score 0.152 -- so quantization contributes nothing
measurable on top of the eight-halving truncation. 14 bits passes with little
margin, which is worth knowing: the choice is comfortable *at* 16 and
marginal one step below.

The 10-bit row scores better than the 12-bit one. That is neither an error
nor a reason to prefer it: at coarse quantization the rounding happens to land
bisection endpoints favourably on this corpus, and nothing about that
generalises.

Quantizing the lightness search matters. An earlier version of this table
rounded only the *result* of that search while its 30 internal comparisons
ran in float64, and reported 3.795 with one infeasible result at 10 bits.
Neither survives quantizing the branch decisions themselves. The 12-, 14- and
16-bit rows were unaffected, but a measurement that quantizes only the
boundaries of a stage is not measuring that stage.

s16.16 is what P6's stages already use, so the mapper needs no wider
intermediate than the pipeline carries anyway.

### The cube root was the last stage still running in float64

The table above quantizes every value the mapper passes around, but it
reached OKLCh through Python's own cube root. OKLab's forward transform
needs three of those per pixel, and there was no fixed-point cube root in
the tree to reach for -- so the study was, by its own standard above,
measuring the stages on either side of the root rather than the root
itself.

`fl::icbrt64` (`src/fl/math/fixed_point/icbrt.h`) and `s16x16::cbrt` close
that. The identity is the one `sqrt` already uses one power down: for a Q16
raw `r`, the root satisfies `y^3 = r * 2^32`, so the root is an integer cube
root of the raw value shifted left by 32 -- a 22-step bit-by-bit loop with
no division and no float, exact to within one ULP.

Re-scoring the mapper with that root in place, and then perturbing it to
find out how much accuracy is actually required:

| cube-root error | worst ΔE2000 |
| --- | --- |
| **exact** | **0.149** |
| ± 64 ULP | 0.200 |
| ± 256 ULP | 0.459 |
| ± 1024 ULP | 1.717 |

The exact root reproduces the recorded result, so the cube root costs the
mapper nothing. A1's budget of 0.5 is not reached until roughly 256 ULP,
which the exact root clears by more than two orders of magnitude.

The perturbation is worth more than the reassurance. It says a *cheaper*
root is still on the table -- anything holding under about 64 ULP would
also pass -- so if 22 steps per root ever turns out to cost too much on an
8-bit target, the budget for replacing it is known in advance rather than
guessed at. Both ends are pinned by
`ci/tests/test_color_gamut_study.py`.

### The mapper needs no trigonometry

Compressing chroma at constant hue reads as a polar operation -- convert to
(L, C, h), scale C, convert back -- which would put `atan2`, `hypot`, `cos`
and `sin` in the per-pixel path. It does not have to. Since `a = C cos h` and
`b = C sin h`, scaling C by t at fixed h is exactly scaling both a and b by
t. Hue is preserved by construction because the ratio b/a is untouched.

Scored over the same 20 vectors, the two formulations agree: **0.1491**
for the polar route against **0.1521** for scaling (a, b). The scaling form
is marginally worse -- it rounds t*a and t*b rather than C, cos h and sin h
-- and comfortably inside the budget either way. So the embedded mapper uses
it, and needs no trigonometry at all.

### The whole transform, end to end, in integers

`src/fl/gfx/oklab_q16.{h,cpp.hpp}` implements OKLab in the working domain.
Modelling that implementation exactly -- s16.16 matrix *coefficients*, not
just s16.16 values, the integer cube root, integer cubes, integer bisection
and the scaling form above -- and scoring it over the corpus gives **0.1526
dE2000**, against the 0.152 recorded for the float64 study. Quantizing the
coefficients costs nothing measurable.

One limit is worth recording rather than discovering later. The inverse
followed by the forward transform loses accuracy near black: OKLab
cube-roots the cone responses, and that root's derivative,
`1 / (3 * lms^(2/3))`, diverges as a response approaches zero -- around
L = 0.05 it is about 1200. Restricted to in-gamut colours the worst round
trip there is roughly 300 ULP of s16.16, about 0.005 in OKLab lightness,
against 5 to 15 ULP above L = 0.35.

This is conditioning, not a representation choice, and two plausible fixes
were measured before that conclusion was drawn:

| carried at | worst round trip at L = 0.05 | mapper dE2000 |
| --- | --- | --- |
| lms in Q16 (shipped) | 5 474 ULP | 0.1526 |
| lms in Q32 | 3 655 ULP | 0.1560 |
| XYZ in Q32 | ~1 193 ULP equivalent | -- |

Widening lms makes the mapper *worse*, and widening XYZ -- which the whole
P6 working domain would have to follow -- only improves it about fourfold.
Neither buys anything the budget needs, so the simpler implementation ships.

## Is the feasible chroma ray actually connected?

Bisection assumes it is. The reference does not, so the assumption was tested
rather than argued: 4 680 rays (lightness on a 40-step grid, hue every 3
degrees), each sampled at 401 chroma values -- about 1.9 million feasibility
evaluations.

**No disconnected interval was found.** On every ray sampled, the feasible
chroma was a single interval starting at zero.

That is strong evidence for this device, not a proof, and it says nothing
about the >=4-emitter case where the zonotope gains a redundant generator. A
coarser sweep runs on every test invocation so the assumption cannot rot
silently.

## Lightness must be clamped before chroma

Chroma compression alone cannot rescue a target that is too *bright*: at zero
chroma the point is still outside the hull, and a chroma-only bisection
converges on an infeasible answer. Every mapper here clamps into the hull --
the OKLCh ones by first reducing lightness to the attainable neutral, as the
reference does, and the naive ones by bounding drives to [0, 1].

The corpus contains no over-bright target, so this had to be constructed to
be tested. That gap has now hidden two defects in this harness: this one and
the missing upper-bound check in `is_feasible` fixed alongside it. Worth
noting for whoever extends the corpus.

## The limit of this study

Bisection assumes feasibility along the chroma ray is monotonic. The P5
reference does not assume that — `docs/color-reference-method.md` explains
that an inverse-OKLab fixed-lightness, fixed-hue ray is cubic in chroma, so
feasibility along it can in principle be disconnected.

On these 20 vectors the two agree to float64 print precision, which says the
corpus contains no such case. It does **not** prove none exists. Before the
embedded mapper relies on bisection, either:

1. extend the corpus with a target constructed to sit on a disconnected
   interval, and re-run this study; or
2. bound the error of bisection against the global search analytically for
   the primaries FastLED ships.

Recording this rather than leaving it implicit: the table above is evidence
for the *objective*, and only provisional evidence for the *search*.

## Is the mapping continuous enough to animate?

#4041's acceptance criteria ask that out-of-gamut mapping be continuous.
Strictly, it is not. The halving search returns a quantized scale factor, so
a target crossing the gamut boundary steps rather than glides. The question
worth answering is how large the step is against the output's own
quantization, so that is what was measured — worst summed change across the
three drives between adjacent samples, at the shipped eight halvings:

| path | worst | as 8-bit codes | worst / mean step |
| --- | --- | --- | --- |
| neutral → deep red | 0.002914 | 0.74 | 1.3x |
| **boundary crossing (4 000 samples)** | 0.003128 | **0.80** | **42.8x** |
| hue sweep, fully out of gamut | 0.003189 | 0.81 | 4.6x |
| neutral luminance ramp | 0.004028 | 1.03 | 1.2x |

The 42.8x in the third column is the discontinuity showing itself: on the
boundary crossing the worst step is forty-odd times the typical one, which
is exactly what a jump looks like. It is still under one 8-bit code, which
is why nothing bands.

### More halvings do not remove it

The obvious response is to spend halvings on it. That was measured too, and
it buys less than it looks:

| halvings | worst on the boundary crossing | as 8-bit codes |
| --- | --- | --- |
| 6 | 0.012482 | 3.18 |
| **8** | **0.003128** | **0.80** |
| 10 | 0.001160 | 0.30 |
| 12 | 0.000992 | 0.25 |
| 14 | 0.000916 | 0.23 |

It falls to a floor rather than to zero, and past ten halvings the worst
step also moves to a different point on the path. So the search resolution
is part of the discontinuity and not all of it; the rest is the entry
check's rounding slack and s16.16 itself. Worth knowing before anyone
spends per-pixel work trying to smooth this out.

Eight halvings keeps every path under about one 8-bit code. At a wider
output depth the step is proportionally more visible — 0.31% of full drive
is roughly 13 codes at 12-bit — so a device driving finer than 8 bits
through a slow gradient is the case that would want re-measuring.

`tests/fl/gfx/gamut_map.cpp` pins the bound at 1.5 codes, which six halvings
fails.

## Devices with a white emitter (C3)

A white emitter makes the emitter matrix wide: the preimage of a target is no
longer unique, and C3 asks for the white-preferred one. The P5 reference finds
it by enumerating vertices — every way of choosing three free emitters and
pinning the rest to 0 or 1, solved and filtered. A3/B11 forbid anything
resembling that search per pixel, so the question is whether it is necessary.

Harness: `ci/color_rgbw_study.py`. Regression test:
`ci/tests/test_color_rgbw_study.py`.

### One white emitter needs no search at all

With the white emitter at drive `w`, the RGB drives making up the difference
are

    d(w) = M⁻¹ · target − w · (M⁻¹ · white)

which is **affine in `w`**. Each of the six bounds on the three RGB drives is
therefore a single inequality in `w`, the feasible set is one interval, and
white-preferred is its upper end. One matrix multiply for the target, one for
the white column — which a real implementation precomputes at bind time — then
six comparisons.

Scored against the reference over the corpus:

| device | vectors | worst per-drive disagreement |
| --- | --- | --- |
| `rgbw` (white at D65) | 48 | 1.1e-15 |
| `non_d65_white` (white at D50) | 48 | 7.8e-16 |

That is float64 rounding: the closed form *is* the reference's answer. Nothing
in the derivation assumed the white sat on the neutral axis, and the second row
is what says so.

### Two white emitters do need more, and the corpus hides it

The obvious extension is to run the one-white form once per white and keep the
better answer. It agrees with the reference on **every** `rgbww` vector in the
corpus.

It is also wrong on most targets. Over 34 520 random reachable targets, mixing
both whites beat the better single white **29 245 times** — 85% — by up to a
full emitter's worth of light.

The reason is not subtle once seen: a single emitter's drive is capped at 1, so
any target needing more white than one emitter can supply must use both. The
corpus does not contain such a target. All 48 `rgbww` vectors are dim enough
that one white suffices — 32 of them use no white at all — so **agreement over
this corpus says nothing whatsoever about the reduction**.

Recording that explicitly, because the measurement was nearly taken the other
way round. Maximizing `w₁ + w₂` with two whites is a linear program over a
polygon; `most_white_two` solves it by exact vertex enumeration, and the
regression test keeps the cheap reduction pinned as *failing* so nobody
simplifies to it later.

### What this leaves

`rgbw` and `non_d65_white` are settled and ready to implement. `rgbww` needs
either the 2D enumeration above — or a closed form nobody has found yet. It
also needs corpus vectors bright enough to tell the two apart before any of it
can be trusted.

The enumeration is bounded but not cheap. `most_white_two` builds ten
constraints — six from the RGB drive bounds, four from the box on the two
white drives — and intersects every unordered pair: 45 of them, of which five
are structurally parallel, leaving **40** candidate intersections to test for
feasibility. That is a constant known at compile time, so it is not an
iterative solver in the A3/B11 sense, but it is roughly forty times the work
of the one-white case and would want its own cost measurement before going
per-pixel.

## Not covered

The interaction between the allocation policy and the gamut mapping. Both are
characterized here in isolation; a target that is out of gamut *and* on a
device with a white emitter goes through both, and that composition has not
been scored.
