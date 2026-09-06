# Host color-reference mapping method

`ci/color_reference.py` is a float64-only reference for P5 vectors.  It is
not runtime firmware and does not quantize to wire codes.

## Mapping objective

The input XYZ and every device-emitter column are linearly Bradford-adapted
from the profile's rendering white to D65.  The mapper expresses the target
in OKLCh, preserving its hue angle.  A target already reproducible by legal
device drives is returned unchanged.

For an out-of-gamut target the objective is lexicographic:

1. Clamp OKLab lightness to the attainable neutral interval from black to the
   profile's selected full-drive rendering white.  Thus black and
   above-white targets deliberately trade luminance before chroma; the mapper
   does not claim to preserve an unattainable lightness.
2. At that lightness and the target hue, choose the greatest feasible chroma
   that does not exceed the target chroma.  Mapping reduces saturation to
   reach the gamut; it never invents saturation from the numerically unstable
   hue of an achromatic target.

An input on the declared rendering-white XYZ ray is classified as neutral
before the OKLab conversion, using the reference's `1e-12` float64 feasibility
tolerance.  It is clamped directly on that same XYZ ray.  This avoids treating
roundoff in neutral OKLab `a`/`b` as a color direction after a lightness clamp.
3. Solve the resulting XYZ using bounded emitter drives.  Among exact bounded
   *vertex* solutions, prefer greater total white-emitter drive, then lower
   squared non-white drive, then lexical drive order.  This is a deterministic
   reference allocation rule, not a claim to minimize non-white energy over
   the full continuous feasible polytope; a later P5 solver objective must
   supply that fidelity if it is required.  A later P5 corpus must also measure
   continuity at active-set transitions.

The D65 mapped result is converted back to rendering-white XYZ only after the
solve.  Signed source intermediates are preserved before this mapping stage.

## Global chroma search, not a monotonic-ray assumption

Legal drives form a convex XYZ zonotope.  An inverse-OKLab fixed-lightness,
fixed-hue ray is cubic in chroma, so feasibility along it is not assumed
monotonic.  For every supporting-plane interval of the zonotope, the host
reference forms the scalar cubic, finds its derivative partitions and all
bracketed roots with 80 bisection iterations, and tests every resulting
connected interval.  It selects the largest feasible interval endpoint.

The search domain is a conservative finite chroma bound derived from the
normalized (each capacity in `(0, 1]`) emitter columns and the absolute OKLab
linear-transform bounds.  It is therefore a mathematical domain bound, not a
nine-point scan or heuristic horizon.  Feasibility comparisons use `1e-12`;
the reference's numerical claim is limited to that float64 tolerance.

## Reported metrics (A1)

Every phase measured against this reference reports the same numbers under the
same normalization, so results from P6 through P10 are comparable rather than
each phase inventing its own yardstick.

### Normalization

Fixed for all reports, per A1:

- **relative colorimetry** — results are expressed against the profile's own
  rendering white, not an absolute illuminant;
- **profile white at full drive** — the white reference is the profile's white
  with every emitter at full drive, which is what the mapper clamps lightness
  against above;
- **dark surround** — no surround compensation is applied;
- **Bradford** is the normative chromatic adaptation transform (B7); this
  reference adapts to D65 before mapping and back afterwards.

Any report that changes one of these is not comparable to the others and must
say so explicitly.

### Budgets

**Implementation fidelity** — an implementation against this float64
reference, inclusive of the brightness/power stage:

| condition | metric | budget |
| --- | --- | --- |
| identity brightness | max ΔE2000 | ≤ 0.5 |
| identity brightness | luminance error | ≤ 0.5 % |
| 1/32 brightness | max ΔE2000 | ≤ 1.0 |

**Profile accuracy** — a profile against measured hardware, at the profile's
reference temperature:

| profile source | metric | budget |
| --- | --- | --- |
| datasheet-derived | median ΔE2000 | ≤ 3.0 |
| datasheet-derived | p95 ΔE2000 | ≤ 6.0 |
| measured | median ΔE2000 | ≤ 1.5 |
| measured | max ΔE2000 | ≤ 4.0 |

The two tables answer different questions and must not be mixed: the first is
"does the implementation match the reference", the second is "does the profile
match the physical part". A phase can pass the first while failing the second.

### What this harness supplies today

`delta_e2000` and the corpus validator provide the ΔE2000 half under the
normalization above. Two gaps are deliberate and belong to later phases:

- the luminance-error percentage is not yet reported per vector, because the
  brightness/power stage it is defined over arrives with P6;
- the profile-accuracy table cannot be exercised at all until measured
  hardware data exists, which is P10's gate.

CIEDE2000 is only defined for pairs whose two D65 XYZ triplets are
non-negative, so the report counts eligible and inapplicable pairs separately
rather than silently dropping the latter.

## Scope

This establishes only host source decoding, adaptation, mapping, bounded
emitter allocation, and response-order primitives.  It does not yet provide
the required response/power model or runtime implementation. The P5 corpus
artifact is plain deterministic JSON for review; its validator recomputes each
named stage against the committed records.  Signed wide XYZ is never clipped:
all signed stages take part in the absolute-error acceptance check.  CIEDE2000
is calculated only for pairs whose two D65 XYZ triplets are non-negative (the
CIELAB domain); the report separately counts eligible and inapplicable pairs.
Its independent anchors and legacy `scale8` provenance are explicit, but it
does not prove native or physical accuracy.
