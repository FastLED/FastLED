# Color pipeline implementation contracts

This implementation addendum addresses the cross-phase findings in
[review #4156](https://github.com/FastLED/FastLED/issues/4156). The program remains
[tracker #4034](https://github.com/FastLED/FastLED/issues/4034), implementing
[specification #4032](https://github.com/FastLED/FastLED/issues/4032) and its
[decision record #4033](https://github.com/FastLED/FastLED/issues/4033).

These are implementation requirements, not a statement that the managed output
path is already available. Phase PRs must link their actual tests and artifacts.
Unimplemented requirements remain open; accepting this document does not close
any implementation phase or the hardware measurement gate.

## Typed boundaries and stage ordering (R2, R5, D1)

The managed path carries these distinct quantities:

1. Encoded source view: storage format, component byte order, pixel stride,
   immutable frame data, and an independent source-color profile.
2. Linear source RGB: decode RGB8 directly to unsigned RGB16; preserve RGB16
   source samples without an RGB8 intermediate.
3. Signed, wide device-independent coordinates: source RGB to XYZ, white-point
   adaptation, and gamut mapping. Negative intermediates must survive until
   gamut mapping; signed multiplication uses wider products with defined
   rounding and overflow admission checks.
4. Linear emitter contributions: three, four, or five normalized light outputs
   produced by the device solve, including white-channel allocation.
5. Dimmed linear emitter contributions: multiply by brightness and the shared
   power scalar once in this domain.
6. Physical drive coordinates: inverse measured code-to-light response,
   followed by chipset-specific current/brightness-field selection and final
   quantization/dithering.

Response inversion in step 6 is necessary physical calibration. It is not a
second artistic gamma stage, and it must not stack with legacy correction.
Multiplying already response-compensated drive codes by brightness is wrong:
if red light is `d` and green light is `d*d`, quartering full-scale drive yields
red light `1/4` but green light `1/16`. Quarter light requires drives `1/4` and
`1/2` respectively. P5 golden vectors and P6/P8 integration must cover this.

Source-profile defaults for ordinary managed, containerless RGB buffers are
linear transfer, BT.709 primaries, and D65 white. This applies to RGB8 buffers
too; eight-bit storage alone does not imply sRGB transfer. In contrast,
[RGB8 `.fled` input with absent metadata](#source-admission-and-playback-r1-r5-r6-r10)
defaults to sRGB transfer. Enabling management is explicit and can change
appearance. With management disabled, existing byte output remains unchanged.

### Container formats and generic storage

`fl::fled::PixelFormat` owns the stable `.fled` wire identifiers. Generic
`fl::PixelFormat` describes FastLED storage. The generic API is independent of
the container. Matching numeric enum values are permitted but do not establish
a casting or serialization contract.

| Container format | Generic storage | Additional meaning |
|---|---|---|
| `Rgb8`, wire `0x00` | RGB8 | Source transfer/primaries from resolved metadata |
| `Rgb16Linear`, wire `0x05` | RGB16 | Little-endian payload, linear transfer, full range |

The `.fled` layer performs checked conversion. It retains the resolved source
profile and payload byte order alongside the storage descriptor. Reverse
serialization validates both storage and source semantics: generic RGB16 is
not enough to label data `rgb16_linear`.

`resolveVideoColor` remains a `.fled` operation and accepts the container enum
through a public header. Unknown raw values are rejected at parsing/admission
boundaries. Its byte-valued compatibility overload, if retained, has identical
validation behavior.

## Source admission and playback (R1, R5, R6, R10)

Recognizing FLED magic commits a reader to container parsing. An unsupported
version, format, or malformed/truncated envelope must not rewind into legacy
raw RGB. Headerless RGB compatibility applies only when FLED magic is absent.
Readers must document how non-seekable input is probed without losing bytes.
Older deployed binaries cannot be assumed to reject newer formats safely.

Loading a bundle, resolving its declaration, and admitting it for rendering
are separate operations. Managed playback is strict by default. Unknown or
unsupported explicit color semantics cause an error; best-effort fallback for
advisory RGB formats requires explicit caller opt-in and a diagnostic. A
mandatory unsupported format never falls back to a byte interpretation.

Absent metadata resolves using these existing default tuples:

- RGB8: BT.709 primaries, sRGB transfer, RGB matrix, full range.
- RGB16-linear: BT.709 primaries, linear transfer, RGB matrix, full range.

An explicit contradictory declaration is rejected. The term "mandatory" means
that the format's semantics cannot be ignored, not that redundant default
metadata must physically appear in every envelope.

File metadata owns the source meaning for file playback. Channel defaults
apply to ordinary buffers and do not silently override file metadata. An
explicit override must be visible as such. Switching sources replaces the
complete profile/view atomically between frames and invalidates derived
caches. Two channels consuming the same source may have different emitter
profiles without changing the source buffer.

Same-space effect arithmetic retains its declared-space semantics. Mixed-space
composition is rejected in v1 unless all inputs have first been converted to
one explicitly typed wide working domain. A decode inside `show()` cannot
retroactively correct an earlier RGB8 blend.

On a playback error after a lit frame, a dark-output policy requires an actual
black-frame submission and an observable error result. Skipping output alone
leaves addressable LEDs displaying the previously latched frame.

### Numerical admission

Syntactic metadata validation is not numerical transform admission. Before a
profile is installed, require finite values, usable white, nondegenerate source
primaries, positive usable emitter capacities, and representable, sufficiently
conditioned matrix coefficients. The embedded implementation publishes its
coefficient bounds and tests their edges against the reference. Profiles
outside those bounds fail explicitly; they do not become native-drive input.

The initial physical-emitter domain uses realizable nonnegative chromaticities.
Support for imaginary *source* primaries must be explicit and bounded by the
same numerical admission checks, never inferred from successful JSON parsing.
Measured response tables must be finite, bounded, monotone, and have defined
endpoints. Inverse response uses a deterministic policy for plateaus and rejects
unachievable targets rather than dividing by a zero response interval.

## White and gamut coordinates (R7)

Separate measured emitter XYZ capacities from the selected rendering white and
its maximum achievable neutral luminance. All emitters at full drive are not
necessarily that white, particularly for RGBWW and target-white overrides.

Bradford adapts source-relative XYZ to the selected rendering white. Standard
Oklab/OKLCh computations use D65-relative XYZ with white Y=1; see the
[Oklab definition](https://bottosson.github.io/posts/oklab/). Therefore the mapper
adapts both target XYZ and the entire feasible emitter gamut from rendering
white to D65 for its objective, and transforms mapped results back before the
device solve. The target and boundary must use exactly the same transforms.

P5/P7 must publish the exact compression objective, luminance/chroma tradeoff,
boundary tolerances, and deterministic tie-breaks as an executable reference.
The algorithm must preserve feasible input exactly within the stated numerical
budget, preserve the selected neutral axis, and be continuous at gamut and
white-allocation boundaries. A named color space by itself is not an algorithm.
Iterative optimization may run at profile/LUT construction, never per pixel in
the embedded output path. RGBW/RGBWW feasibility includes all emitters.

## Power and presentation (R3, R4, R8)

The managed implementation uses two-pass streaming over stable frame inputs:

1. Evaluate solved demand across every channel participating in a supply
   budget; include response inversion, current-field policy, and quantization
   reserve. Compute a wide shared power scalar without advancing dither state.
2. Encode the same source/profile snapshot with that scalar and submit output.

This requires no RGB16 framebuffer. Async drivers may continue owning their
existing encoded wire buffers. The prepass and encode cost both count toward
throughput budgets. Caller-owned source storage must not mutate during either
pass; profile bindings are immutable snapshots until the frame ends.

User brightness remains `b/255` in linear light. The internal power scalar is
wide, not an eight-bit replacement brightness. Fixed controller/LED idle
consumption is independent of that scalar. A budget below unavoidable baseline
returns an infeasible-budget diagnostic and minimum controllable output; it
does not claim zero total consumption. Mixed legacy and managed channels must
be aggregated with their respective electrical models.

The electrical contract bounds modeled demand for each latched output frame,
including upward quantization/dither choices. It does not guarantee limits on
unmodeled internal PWM transients or replace electrical supply protection.
Nonlinear electrical models require a bounded/conservative scalar solution;
assuming watts proportional to XYZ Y is not supported.

### The legacy estimator's emitter count

"Aggregated with their respective electrical models" is not free wording. The
legacy estimator walks the source `CRGB` array, which is what the sketch
wrote, not what the strip is driven with. A controller in RGBW mode converts
every pixel with `rgb_2_rgbw` before latching, and the resulting four drives
are neither bounded by nor proportional to the source triple.

Measured on 300 pixels of source white with the shipped default
`PowerModelRGBW` (r90 g70 b90 w100 dark5), in milliwatts:

| RGBW mode | source triple | four emitters | |
|---|---|---|---|
| `kRGBWMaxBrightness` | 76,205 | 106,086 | source triple is **28% under** |
| `kRGBWBoostedWhite` | 76,205 | 81,470 | **6.5% under** |
| `kRGBWExactColors` | 76,205 | 30,485 | **150% over** |

The first two are budget under-spent against; the third is a strip dimmed for
no reason. `set_power_model(const PowerModelRGBW&)` compounded it by routing
through `toRGB()`, which dropped `white_mW`: the API accepted the figure and
discarded it, so the fourth diode could not have been charged even in
principle. The estimator now runs the same conversion the encoder will and
charges all four emitters.

Two boundaries this leaves in place, both deliberate:

- **Demand is still projected linearly in brightness.** The estimator reports
  demand at full brightness and the limiter scales it. That commutes with
  `kRGBWExactColors` and `kRGBWMaxBrightness` to within 0.4% over the measured
  cases, but not with `kRGBWBoostedWhite`, which reallocates between emitters
  as the scale falls: demand there runs up to **12.1% above** the linear
  projection at low brightness. Closing it means evaluating demand per
  candidate brightness, which is a pixel walk per step of the limiter's
  search. Charging four emitters instead of three is the larger correction by
  a wide margin; the residual is recorded rather than folded into the claim.
- **RGBWW still folds.** `PowerModelRGBWW::toRGB()` spreads its two white
  emitters across R/G/B rather than dropping them, which over-estimates and
  so cannot break a budget. A five-emitter accounting needs the RGBWW
  allocation the way the RGBW path needs `rgb_2_rgbw`.

A controller in RGBW mode under a model that declares no white emitter is
diagnosed once rather than guessed at. There is no basis for a fourth
emitter's draw in a three-emitter declaration, and inventing one is the shape
of the defect this replaced.

### Fidelity accounting

Report three distinct errors:

- Numerical error before native quantization versus the float64 reference,
  including source decode, mapping, solve, and brightness.
- Native encoding error versus ideal emitter light, including quantization,
  response inversion, and current-field effects.
- Profile error versus measured physical output, including instrument
  uncertainty and reference operating conditions.

A reference quantized to the same wire format can prove implementation parity,
but cannot prove ideal-light accuracy. Publish luminance normalization,
relative-error denominators, black-floor treatment, and the exact corpus.
Do not silently exclude difficult vectors to satisfy the issue's budgets.

#### The black floor, stated

The paragraph above requires a black-floor treatment to be published. This is
it, measured rather than chosen.

**A reference drive below one s16.16 unit is outside the budget's scope.** One
unit is `1/65536` = `1.5259e-05` in normalized emitter flux. Below that the
fixed-point path can only answer zero or a whole unit where the reference asks
for a fraction of one, and CIELAB's kappa branch turns that into a whole unit
of L*.

The floor is the arithmetic's resolution and **not** a luminance threshold,
which is the part worth being exact about because the obvious form does not
work. Measured on the P5 corpus's `rgb` device, `bt2020-rgb-02` misses at
Y = 5.4e-04 while `srgb_bt709-rgb-00` passes at Y = 3.0e-04 -- a *darker*
vector inside the budget and a brighter one outside it. What separates them is
that the first has a sub-ULP drive and the second does not.

Its effect on A1, over all 57 vectors the corpus holds for that device:

| | worst dE2000 |
|---|---:|
| every vector | 1.3055, at `bt2020-rgb-03` -- source code (0,0,1) in BT.2020, whose reference drives are (0, 5.242e-05, **1.447e-05**) |
| vectors with no sub-ULP drive (49 of 57) | **0.3951** |

So A1's 0.5 is met everywhere the arithmetic can represent the target **in a
single frame**, which is what these figures measure: `processPixelQ16` returns
one `i32` triple per pixel and the corpus is compared frame by frame. The
vectors above 0.5 are out of reach of a one-frame fixed-point answer of this
width -- by any implementation of it, not just this one. They are not thereby
unreachable altogether, because a cycle can average to a sub-ULP drive; see
the note below on the unsupported low-light region, which is the same
distinction from the other side. Eight of the 57 carry a sub-ULP drive.

What `tests/fl/gfx/pipeline.cpp` asserts, stated exactly rather than as "both
figures are pinned", which they are not:

* the corpus size and the excluded count are pinned exactly -- `kVectorCount
  == 57` and `below_floor == 8` -- so a shrunken corpus, or a floor that
  swallowed it, cannot satisfy the bounds below;
* the floored worst is bounded above twice, at A1's `0.5` and again at `0.45`,
  so a regression that stays inside A1 still fails;
* the unfloored worst is bounded above at `1.4`, so the low-light miss cannot
  quietly grow while the floored bound keeps passing.

Those are upper bounds and not equalities: a floored worst of 0.44 would pass.
The measured values, 0.3951 and 1.3055, are recorded in the comments beside
those assertions and here, and are what a change should be read against. The
count of vectors above 0.5 is not asserted at all -- `below_floor` is.

This does not silently exclude difficult vectors: the excluded set is defined
by a property of the arithmetic, its size is asserted, and the unfloored
number is published beside the floored one.

What this floor does **not** settle is the *unsupported low-light region* the
same paragraph asks for -- that is a statement about what a device is allowed
to render, and it needs P8's dithering answer, since a sub-ULP average drive is
reachable over a cycle even though it is not reachable in one frame.

Dither accuracy is based on time-weighted emitted XYZ over a declared cadence
and observation window, followed by perceptual error calculation. It is not
an unweighted mean of frame codes or frame Delta E values. State advances on
presentation according to the documented driver contract, with explicit tests
for dropped submissions and irregular dwell. For scale: linear16 code 1 needs
one native RGB8 code-1 frame per 257 equally timed frames to match its average
on an ideal linear PWM device. At 60 Hz that is about 4.28 seconds. The native
output report must state the resulting fidelity/flicker limitations.

## Ownership and tiers (R9)

Runtime bindings own immutable profile/configuration data, including response
tables, or accept an explicitly lifetime-bound immutable source handle. Global
defaults always own their values. Do not retain a pointer to a caller's stack
temporary. Rebinding replaces a profile/cache identity, not fields behind an
unchanged cache key.

The TINY minimal configuration uses compile-time/static profile specialization
with no additional per-controller runtime fields. Runtime profile storage is a
separate capability on larger tiers. Prove disabled, static-minimal, and runtime
layouts independently; default-null pointers are not zero added state.

Managed-mode legacy exclusion covers correction, temperature, artistic gamma,
and binary dithering. Binding configuration is distinguishable from active
managed rendering; an API-only phase must not report calibrated output while
the legacy encoder is still used.

## Completion evidence

Each phase has its own implementation PR, focused RED-to-GREEN evidence,
appropriate review and regression checks, and verified merged state. A
prerequisite slice may land separately, but it must not auto-close a phase
whose acceptance criteria remain unmet. The overall program requires measured
reports for representative clockless and high-precision strips, plus the
specified platform budgets and legacy compatibility gates. Neither this
addendum nor synthetic golden vectors substitutes for that evidence.
