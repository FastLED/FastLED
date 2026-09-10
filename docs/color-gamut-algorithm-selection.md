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

## Is the feasible chroma ray actually connected? No.

Bisection assumes it is. The reference does not, so the assumption was tested
rather than argued: 4 680 rays (lightness on a 40-step grid, hue every 3
degrees), each sampled at 401 chroma values -- about 1.9 million feasibility
evaluations. **No disconnected interval was found**, and the same held across
another 7.5 million evaluations on the four white-emitter devices in "Wide
hulls" below.

The assumption is false anyway. Sampling was the wrong instrument, and the
number of samples was never going to fix it.

The ray does not have to be sampled. As the method note below already says, an
inverse-OKLab fixed-lightness, fixed-hue ray is cubic in chroma; written out,
each drive along the ray is `d_i(c) = sum_j K_ij (p_j + q_j c)^3`, an ordinary
cubic in `c`. So `{c : 0 <= d_i(c) <= 1}` is decided by the real roots of `d_i`
and `d_i - 1`, and the feasible set for a ray is *exact* -- no gap can hide
between two samples, however narrow. That is `ci/color_ray_roots.py`.

Exact given that the solver returns every real root, which is worth stating
because the closed form loses accuracy on a near-double root and can merge two
that sit closer than the polished result separates. The partition is therefore
the union of the roots with a uniform guard grid, so the failure mode degrades
to the resolution of a sampled scan rather than past it. An exact tangency --
a drive touching a bound with even multiplicity -- is not resolvable in
floating point at all; it is also measure-zero in lightness and hue, so what a
grid meets is the near-tangency beside it, which is an ordinary narrow
interval and is how the wedge below presents.

Computed that way, on the primaries this report uses:

| | |
| --- | --- |
| lightness | 0.5 |
| hue | 264.06 degrees |
| feasible chroma | `[0, 0.29443]` and `[0.34575, 0.34644]` |
| what a bisection returns | 0.29443 |
| what is actually reachable | 0.34644 |
| chroma given up | 0.0520, **17.6% of the reachable maximum** |

The far interval is a real colour, not an artefact: its LMS response is
positive on all three cones and its drives are `(0.000005, 0.000214, 0.097553)`
-- a deep blue at a tenth of full drive. Re-rendering those drives through the
forward matrix returns `L = 0.500000, C = 0.346092, h = 264.0600`, the target
it was asked for.

### Why 1.9 million samples walked past it

Two independent reasons, and the second is the one that matters.

**The wedge is thinner than the hue grid.** Disconnection holds for hue in
roughly `[264.06, 264.20]` degrees -- about 0.14 degrees wide, and present at
every lightness from 0.05 to 0.75. It contains no multiple of 3, so the dense
sweep's hue grid steps over it. At 264.0 degrees the set is one interval
ending at 0.28995; by 264.25 the two have merged into one interval ending at
0.34578. The disconnection lives in the fold between.

**Density is not the fix.** Handed the exact hue, a 401-sample chroma scan
*still* reports one interval, because the far island is 0.00069 wide against a
0.00125 sample step. Refining the grid moves the problem rather than solving
it: whatever the step, some ray's island is thinner than it.

### What it costs, and what it does not

Narrow: 0.14 degrees of 360, so 0.04% of hues. Inside it, an out-of-gamut
target above 0.34644 chroma is mapped to 0.29443 instead -- under-saturated,
hue exact, still in gamut. The mapper returns a producible colour that is
merely less saturated than one it could have produced, so this is a quality
loss in a sliver, not a correctness failure anywhere.

It is still worth having on the record as fact rather than as an assumption,
because the shipped search's validity was resting on it, and because the same
argument shows what a future device would need: the wedge exists where the ray
runs nearly along the `red = 0` face, so a different primary set puts it
somewhere else rather than removing it.

### Scope

The exact method is for the three-emitter case, where feasibility is "the
unique preimage lands in the box" and each drive is one cubic. With a white
emitter the preimage stops being unique and feasibility becomes a linear
program, so the wide hulls in "Wide hulls" below remain swept rather than
solved. Their claim is still the sampled one, and now known to be the weaker
kind of evidence.

## Lightness must be clamped before chroma

Chroma compression alone cannot rescue a target that is too *bright*: at zero
chroma the point is still outside the hull, and a chroma-only bisection
converges on an infeasible answer. Every mapper here clamps into the hull --
the OKLCh ones by first reducing lightness to the attainable neutral, as the
reference does, and the naive ones by bounding drives to [0, 1].

That is still true of the *bisection*, and it is no longer the whole story of
the shipped mapper. "The clamp gives up more than it needs to" below shows the
clamp discards up to 29.7% of the reachable lightness, and the mapper now
clamps chroma into its feasible interval above the cap instead, falling back
to this when that interval is empty. Read this section as why a bracket at
zero cannot work there, not as a description of what happens.

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

Option 1 has since been taken as far as sampling can take it, for the wide
hulls as well as this one -- see "Wide hulls" below, and it found nothing.
Option 2 was then taken for the three-emitter device by solving the cubic
instead of sampling it, and it did not produce a bound: it produced a
counterexample. See "Is the feasible chroma ray actually connected? No."
above. The paragraph this section opens with -- that feasibility along the ray
"can in principle be disconnected" -- turns out to describe this device rather
than a hypothetical one.

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

The last column is the discontinuity showing itself: on the boundary
crossing the worst step is forty-odd times the typical one, which is exactly
what a jump looks like. It is still under one 8-bit code, which is why
nothing bands.

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

### Two whites also have a closed form

Not the reduction above, and not a search. Write the total `s = w₁ + w₂` and
substitute `w₂ = s − w₁`: the RGB drives become

    (d₀ − s·dW₂) − w₁·(dW₁ − dW₂)

which is the one-white shape with a shifted target and a difference column.
At any fixed total the feasible `w₁` is again an interval, bounded by five
lower and five upper bounds — three from the RGB drives, and two more because
`w₁` and `w₂ = s − w₁` are drives in their own right. Every one of those
bounds is **affine in `s`**, so "some `w₁` exists at this total" is exactly
the conjunction of the pairwise inequalities `lowerₖ(s) ≤ upperⱼ(s)`, each
linear in `s`. Intersecting the 25 of them gives the feasible totals directly.

The interval has to be *found*, not assumed to start at zero, and that is
where the first attempt went wrong (#4198). A bright target is unreachable
with the primaries alone, so its feasible totals begin above zero and a
bisection seeded at zero reports it out of gamut. That attempt's validation
hid the failure: the harness skipped every target its method could not answer
and reported "0 disagreements over 10 285" while silently dropping roughly
30 000 of 40 000 — including precisely the bright ones.

Measured against `most_white_two`, nothing skipped:

| targets | how drawn | solved by both | disagreements |
|---|---|---:|---:|
| 8 000 | uniform drives | 8 000 | 0 |
| 4 000 | both whites near full | 4 000 | 0 |
| 4 000 | drives to 1.35, so many are out of hull | 2 862 | 0 |

Worst difference in total white, 2.2 × 10⁻¹⁵. One target in 4 000 lands on
the hull to within 10⁻⁷ and is admitted by the enumeration's per-constraint
slack while the closed form's exact interval refuses it; that is counted
separately rather than folded into agreement.

**At the extreme total the split is not free.** The reference settles a free
split by minimizing the sum of squares of the RGB drives, and an earlier
revision of the embedded path did the same — one division, since the drives
are affine in `w₁`. Measurement removed it: the feasible split at the extreme
total is a single point. Width measured 0.0 over 4 000 targets on the
cool/warm device, and over 951 on a device constructed so one drive's
constraint is parallel to `w₁ + w₂`, which is the shape that could have
produced an optimal edge. Two whites of the *same* colour are the exception,
and there the reference's own tie-break — lexicographically smallest drives —
is the end the closed form takes anyway.

### The mapper has to target the two-white hull too

The same argument as one emitter down. Testing a two-white device against the
*one-white* hull under-reports what it can produce, and every under-reported
target gets compressed despite being reachable exactly.

Measured on the corpus's cool/warm device, over a deterministic 4⁵ sweep of
its own five-emitter zonotope — so every target is reachable by construction:
**418 of 1024, 41%**, are refused by the one-white allocation. That is the
same magnitude as the 43% the RGB-only solve refuses on a four-emitter device.

So `mapAndAllocateRgbwwQ16` runs the same eight halvings as the other two
paths and takes every feasibility decision through the two-white allocation.
Its lightness bound is derived the same way, and the second white is worth
having there: the brightest reachable D65 neutral goes from **87 723** to
**98 293** in OKLab L (s16.16). Dropping the second white's column from the
relaxation drops it to 87 424, below the one-white bound, which is what pins
that term in the test suite.

`allocateTwoWhiteDrivesQ16` is the s16.16 implementation. It costs up to 25
divisions per pixel against the one-white path's six, which is recorded
rather than optimized away: cross-multiplying the pairwise comparisons would
leave one division and a great many i64 multiplies, and nobody has measured
which wins on an 8-bit target.

### The mapper has to target the real hull

A white emitter enlarges the reachable set, so testing a target against the
RGB hull alone under-reports it. By how much was measured: over 200 000
targets drawn from inside a four-emitter device's own zonotope, the RGB-only
solve **rejects 43%** of them. Every one of those would be compressed by the
three-emitter mapper despite the device being able to produce it exactly —
typically because an RGB-only drive lands just above full scale where the
white emitter would have covered it.

So `mapAndAllocateRgbwQ16` runs the same eight halvings as the three-emitter
path but takes every feasibility decision through the white-preferred
allocation.

Its lightness bound is derived once, at bind time, between two numbers that
are cheap to compute and easy to be sure of.

The lower one is the three-emitter bound, `1 / max(d0)`, reached with the
white emitter off — always attainable. The upper one relaxes the problem:
along the D65 ray the RGB drives are `s·d0 − w·dW`, so the *upper* limit on
drive `i` is loosest at `w = 1` when `dW_i` is positive and at `w = 0` when
it is negative, giving `min_i (1 + max(dW_i, 0)) / d0_i`. Dropping the lower
limits, and letting each channel pick its own `w`, are both relaxations, so
that is an upper bound and never an under-estimate.

**The upper bound is not generally attainable, and an earlier revision of
this stored it directly.** It enforces only the upper limits, and full white
can push a *different* channel negative: with `dW = (0.9, 0.05, 0.05)`
against `d0 = (0.21, 0.72, 0.07)` the formula returns 1.468, where the red
drive is `1.468 × 0.21 − 0.9 = −0.59`. Storing that leaves the mapper with a
zero-chroma candidate its own halving search cannot satisfy, and the
fallback then returns four zero drives for a colour that is not black.

So the bound is bisected between the two, using the allocation itself as the
feasibility test. That is not an iterative solver in the A3/B11 sense — it
runs once per profile, never per pixel, and the per-pixel path sees only the
stored result. Where the relaxation happens to be tight, which includes the
ordinary case of a white emitter at D65 and unit luminance, the bisection
converges straight to it: the bound moves from **1.398** to **2.398** times
D65, exactly the one unit that emitter contributes.

### What this leaves

`rgbw`, `non_d65_white` and `rgbww` are all settled and implemented.

The enumeration stays where it belongs, as the oracle. `most_white_two` builds
ten constraints — six from the RGB drive bounds, four from the box on the two
white drives — and intersects every unordered pair: 45 of them, of which five
are structurally parallel, leaving **40** candidate intersections to test for
feasibility. That is a constant known at compile time, so it is not an
iterative solver in the A3/B11 sense, but it is roughly forty times the work
of the one-white case, and the closed form above makes it unnecessary
per-pixel.

Corpus coverage was the last gap and is now closed. All 48 `rgbww` vectors are
reachable with one white or none — every emitter on that device has unit
capacity, which makes the strip five times brighter than the white it renders,
so no target the corpus can express needs a second white. A device profile
`rgbww_two_white` fixes that: its primaries split luminance the way sRGB does
and its two whites carry 0.35 each, for 1.6 at full drive, so the strip can
only just exceed its own rendering white.

Thirteen of its 57 vectors light both whites, and they cover the shapes that
matter:

- six saturate **both** whites at a total of exactly 2.0 — the corner of the
  feasible totals, and what an allocation assuming those totals start at zero
  calls out of gamut;
- others saturate the cool white with the warm one part-way;
- and the `off_neutral_tint` vectors do the reverse, saturating the *warm*
  white first. An allocation that always fills the first white first
  reproduces every other vector and fails those.

`allocate_two_white` reproduces the reference's recorded drives on all 57 to
within 10⁻⁹, which is the check that could not be written before.

## Wide hulls: is the ray still connected, and does the composition hold?

Two claims above are made for the three-emitter device and do not carry over
by assumption. A white emitter adds a redundant generator to the zonotope,
which changes what "feasible" even means: the preimage of a target stops being
unique, so the question goes from "does the one solution land in the box" to
"does *some* solution", which is the predicate `mapAndAllocateRgbwQ16` and
`mapAndAllocateRgbwwQ16` actually consult.

Harness: `ci/color_wide_hull_study.py`. Regression test:
`ci/tests/test_color_wide_hull_study.py`.

### The ray stays connected

The same sweep as the three-emitter one -- lightness on a 40-step grid, hue
every 3 degrees, 401 chroma steps per ray -- against each white-emitter device
the corpus ships, using the *oracle* hull rather than the closed form. The
steps are intervals, not points: both endpoints are evaluated, so a ray costs
402 evaluations and not 401.
That choice matters: a sweep built on `allocate_two_white` could only confirm
that function's own idea of the hull, which is the assumption under test.

| device | rays | evaluations | disconnected | not anchored at zero |
| --- | --- | --- | --- | --- |
| `rgbw` | 4 680 | 1 881 360 | 0 | 0 |
| `non_d65_white` | 4 680 | 1 881 360 | 0 | 0 |
| `rgbww` | 4 680 | 1 881 360 | 0 | 0 |
| `rgbww_two_white` | 4 680 | 1 881 360 | 0 | 0 |

**7.5 million evaluations, no disconnected interval, and every feasible ray
reaches the neutral axis.** The second column is worth stating separately: an
interval that is connected but floats off the axis would defeat the shipped
search just as thoroughly, because it seeds its bracket at chroma zero.

This is the same kind of evidence as the three-emitter result -- strong for
these devices, not a proof. The interval detector is given a predicate with a
hole punched in it and required to report two intervals, so the sweep cannot
pass by never firing.

**Read that row with the three-emitter refutation in mind.** The identical
sweep, at the identical density, reported a clean zero on a device that does
have a disconnected ray -- see "Is the feasible chroma ray actually connected?
No." above. A zero in this column means "the grid did not land on one", and
these numbers are the same grid. The exact method that found the wedge does
not carry over here, because a white emitter makes feasibility a linear
program rather than one cubic per drive, so for the wide hulls the question is
genuinely still open rather than answered in the affirmative.

### The composition holds, once both halves use the same hull

Scored over roughly 28 000 out-of-gamut targets, mapped at the shipped eight
halvings and then allocated, with the drives **re-rendered** rather than
trusted -- which is what would catch an allocation reporting success while
producing something else.

| device | out-of-gamut | hue drift | dE2000, realized vs mapped | worst step | worst / mean |
| --- | --- | --- | --- | --- | --- |
| `rgbw` | 7 012 | 0.000000 deg | 8.6e-14 | 0.002871 | 12.5x |
| `non_d65_white` | 7 012 | 0.000000 deg | 7.9e-14 | 0.003140 | 12.7x |
| `rgbww` | 7 012 | 0.000000 deg | 7.9e-14 | 0.003441 | 10.1x |
| `rgbww_two_white` | 7 441 | 0.000000 deg | 1.0e-13 | 0.011086 | 9.7x |

Allocation adds nothing measurable: the realized colour matches what the
mapper chose to float64 noise, and hue survives the second stage exactly. The
worst/mean ratios sit *below* the three-emitter path's 42.8x, so the white
emitter does not make the boundary step worse.

`rgbww_two_white` reads 3.5x the others in absolute step only because its
primaries carry 0.22/0.60/0.08 rather than unit capacity, so a given change in
XYZ needs proportionally larger drives -- the blue emitter alone is 12x more
sensitive. As a fraction of that device's own full drive it is in line.

### The tolerance skew between the two hulls, measured

Scoring this composition the obvious way -- oracle in front, closed form
behind -- reports 56 failures on `rgbww` that neither method commits alone.
They are not defects. `most_white_two` allows `ENUMERATION_TOLERANCE` (1e-7)
and `allocate_two_white` allows `DRIVE_TOLERANCE` (1e-9), so a target sitting
on the hull boundary is accepted by one and refused by the other. The worst
case found is a target whose blue drive is **-7.6e-08** with the whites off:
genuinely outside, by less than the oracle's slack.

Recorded because it is the trap waiting for the next person to score these
two stages together: the mapper and the allocation behind it have to consult
the *same* predicate, which is what the embedded path does. With both on the
closed form the count is 0 on every device.

### Above the brightest neutral is not outside the hull (#4245)

Constructing over-bright targets for these devices turned up a case worth
recording on its own. The mappers clamp lightness to the brightest reachable
*neutral*, and the hull reaches higher than that off the neutral axis:

Two denominators are in play and they are not interchangeable, so both are
given. *Headroom* is the gain relative to the neutral cap, which is what the
clamp gives up as a fraction of what it keeps. *Share* is the same interval as
a fraction of the whole reachable range.

| device | brightest neutral | brightest reachable | at | headroom | share of reachable |
| --- | --- | --- | --- | --- | --- |
| `rgbw` | 1.338544 | 1.623559 | hue 300, C 0.48 | **+21.3%** | 17.6% |
| `non_d65_white` | 1.333993 | 1.633584 | hue 300, C 0.50 | **+22.5%** | 18.3% |
| `rgbww` | 1.499835 | 1.748872 | hue 300, C 0.46 | **+16.6%** | 14.2% |
| `rgbww_two_white` | 1.152403 | 1.167419 | hue 0, C 0.02 | +1.3% | 1.3% |

So a chromatic target just above the neutral cap can be exactly reachable and
is compressed anyway. That is #4245, and these are the first numbers on it for
wide hulls: on a four-emitter device the clamp gives up **22.5% on top of the
neutral cap**, which is **18.3% of the reachable range**. It is not fixed
here: the clamp is what makes the chroma bisection valid at all (see
"Lightness must be clamped before chroma"), and removing it without replacing
the search inverts the bisection's invariant.

## The clamp gives up more than it needs to (#4245)

"Lightness must be clamped before chroma" above is correct about why a chroma
bisection fails on an over-bright target, and the conclusion drawn from it --
that lightness must therefore be spent first -- is too strong. What was
missing is the shape of the feasible set, which is now measured.

Harness: `ci/color_lightness_headroom_study.py`. Regression test:
`ci/tests/test_color_lightness_headroom.py`.

### The feasible chroma is still one interval; it just stops containing zero

Sampled along fixed-lightness, fixed-hue rays on the three-emitter device
(neutral cap L = 1.118228):

| L | hue 30 | hue 120 | hue 300 |
| --- | --- | --- | --- |
| 1.0623 (below cap) | [0.0000, 0.4215] | [0.0000, 0.1913] | [0.0000, 0.5670] |
| 1.1741 (above) | **[0.0795, 0.3195]** | (none) | **[0.1275, 0.6000]** |
| 1.2860 (above) | **[0.2070, 0.2310]** | (none) | **[0.3082, 0.6000]** |
| 1.3978 (above) | (none) | (none) | **[0.4432, 0.6000]** |

Connected throughout -- so a search is possible -- and above the cap simply
not anchored at zero. That is the whole of it. A bisection bracketed at
`[0, C]` has an infeasible low end from its first step, its invariant is
inverted, and it walks down to zero, which is also infeasible there. The
clamp is what stops that, not a shortcut.

The headroom being given up is large: brightest reachable L is **1.450614**
at hue 300, C 0.50, against a neutral cap of 1.118228 -- **29.7% above the
cap**, on the same device the rest of this document measures.

### Finding both edges keeps the lightness

Probe for a feasible chroma, then bisect each edge. Measured against the
shipped path on over-bright targets:

| target | shipped L, C | interval clamp L, C | dL |
| --- | --- | --- | --- |
| L 1.174, hue 30, C 0.50 | 1.1182, 0.3672 | 1.1741, 0.3196 | +0.056 |
| L 1.286, hue 300, C 0.30 | 1.1182, 0.2988 | **1.2860, 0.3081** | +0.168 |
| L 1.398, hue 300, C 0.30 | 1.1182, 0.2988 | **1.3978, 0.4431** | +0.280 |
| L 1.398, hue 120, C 0.50 | 1.1182, 0.0000 | 1.1182, 0.0000 | 0.000 |

**Never worse** across the sweep -- it falls back to the shipped path when no
chroma is feasible at that lightness, which is the last row. Hue is preserved
exactly, and every result is feasible.

Cost is `probes + 2 x halvings`. Those are parameters in the harness, because
sweeping them is what a study is for; what A3/B11 need is that the cost is
bounded by a count rather than by a convergence criterion, so an
implementation fixes both and the loop count is known at compile time. An
earlier revision of this section called the parameters themselves
compile-time constants, which they are not.

### What the embedded mapper does with it

Shipped in `gamut_map.cpp.hpp` for all three mappers, at
`kGamutMapProbes = 8` and `kGamutMapHalvings = 8`, so the above-cap path costs
24 feasibility tests and the loop count is known at build time. Targets at or
below the cap never enter it, and neither does an in-gamut target, so nothing
below the cap moved.

Re-rendered through the emitters rather than read back off the mapper, it
reproduces the table above:

| target | embedded L, C |
| --- | --- |
| L 1.286, hue 300, C 0.30 | 1.2860, 0.3081 |
| L 1.398, hue 300, C 0.30 | 1.3978, 0.4431 |
| L 1.398, hue 120, C 0.50 | 1.1182, 0.0000 |

**One deliberate difference from the harness.** The study probes to an
absolute `PROBE_CEILING = 0.8`; the mapper works in factors of the request and
reaches four times it. Both reproduce every row above, and they differ only on
targets the table does not contain: a bright *near-neutral*, which is
infeasible precisely because it is not saturated enough. An absolute ceiling
answers that with a saturated colour; a relative one finds no seed and falls
back to the clamp, keeping a near-neutral request near-neutral. An over-bright
grey has no chroma to scale at all and clamps under either.

**A note for whoever tests the wide mappers.** A hull with a white emitter
swallows a *saturated* target a little above the cap outright -- the
allocation succeeds and the mapper returns before the clamp is reached -- so
a case written at chroma 0.30 exercises none of this and passes with the whole
path disabled. The reachable-but-outside region above the cap is at low
chroma, which is the finding restating itself. The tests assert the target is
outside the hull before they assert anything else.

The lower edge is not optional: clamping to the upper edge alone produces
*infeasible* output on exactly the targets this is meant to fix, which the
test suite pins.

### Two shapes that do not work, recorded so they are not re-attempted

**Compressing chroma at the target's own lightness.** The same inverted
bisection described above. Measured converging to zero.

**A line search from the neutral-at-cap toward the target.** Attractive
because hue is exactly preserved along it -- `(a, b)` scales linearly, so the
direction is constant. It fails for a different reason: the anchor sits *on*
the hull boundary, so the segment leaves immediately, the largest feasible
step is tiny, and chroma collapses. Measured worse than the shipped path on
most targets.

### What this does not do

It is a host-side result. The embedded mapper still clamps, and converting it
means a probe loop and a second bisection in `gamut_map.cpp.hpp` for three
device topologies, plus the s16.16 accuracy work. This measures the shape,
fixes the algorithm choice, and leaves that.

## Not covered

The composition is scored against the reference objective and against itself,
not against measured hardware -- that is P10's gate.

Ray connectivity above four emitters is measured on the four devices the
corpus ships, and only by sampling. A device whose whites are close enough to
be near-parallel generators, or one with more than two whites, is outside what
was swept. The three-emitter case is no longer in this list: it is solved
exactly, and the answer is that the ray is sometimes disconnected.
