# Gamut-mapping algorithm selection (P7)

#4041 asks for a comparison of clipping, max-normalization, OKLCh compression
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
| clip negative drives | 20.652 | 3.923 |
| max-normalize | 20.652 | 3.923 |
| desaturate toward neutral | 14.893 | 2.101 |
| **OKLCh chroma compression** | **0.000** | **0.000** |

A1's budget is max ΔE2000 ≤ 0.5 at identity brightness.

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

## Not covered

Only the three-emitter `rgb` device. RGBW and RGBWW add a redundant emitter,
so the hull is a zonotope with a non-unique preimage and the allocation policy
(C3, white-preferred) interacts with the mapping. That needs its own study
once the ≥4-emitter solve exists.
