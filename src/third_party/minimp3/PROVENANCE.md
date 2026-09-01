# minimp3 provenance

- Component: `lieff/minimp3`
- Upstream repository: <https://github.com/lieff/minimp3>
- Upstream commit: `ea99364f61c14656440e8d77e9c233ccf3124633`
- Commit date: 2022-06-25
- License: CC0-1.0 (`LICENSE`)
- Vendored files: `minimp3.h`, `minimp3_fixed_tables.h` (FastLED-generated)

Production and host-golden decoding both use the core `minimp3.h` API with
`MINIMP3_NO_STDIO`; the optional upstream `minimp3_ex.h` stdio/file layer is
not vendored.

## Fixed-point path

`MINIMP3_FIXED_POINT` selects an integer DSP pipeline. It is FastLED-authored
under the same CC0 dedication as the rest of the component, and it is intended
to go back to `lieff/minimp3`: it is written as an addition to the upstream
file rather than a fork of it, so `FASTLED.patch` remains a readable diff
against the pristine header.

It is a clean-room conversion in the sense the parent tracker requires. The
RPSL/RCSL-licensed `libhelix_mp3` tree that this repository carried at the time
was not used as a source, not even for values that are mathematically forced.
That tree has since been deleted (FastLED#4056), which is the point of the
exercise: had any of it been copied, deleting it would not have been possible. Every constant is
regenerated from its ISO 11172-3 / 13818-3 formula by
`ci/codec_tables/generate_minimp3_fixed_tables.py`, which also cross-checks
each generated value against upstream minimp3's own CC0 float literals; that
cross-check is what catches a misread formula, and CI runs it with `--check`.

Design notes live in `minimp3.h` next to the code, but the one worth repeating
here is why the conversion is small: minimp3 normalises its pipeline
internally, so a single Q26 int32 format covers every stage and block floating
point is needed only for scalefactor gains and `x**(4/3)`.

## Integer SIMD, and where it stops

The fixed-point path carries SSE4.1 and NEON kernels for the polyphase filter
and the DCT-32, selected at run time on x86 and unconditionally on ARM64, and
suppressed entirely by `MINIMP3_NO_SIMD`. They are held to exact equality with
the scalar path rather than to a tolerance: both operate on the same integer
representation, so any difference is a defect in one of them.

Three findings from Phase 4 (FastLED/FastLED#4055) worth not rediscovering:

- **SSE2 is slower than scalar here.** SSE2 has no signed 32x32 -> 64 multiply.
  Emulating it (unsigned multiply plus a sign correction) measured 0.66x of
  scalar in a standalone harness and 0.95x end to end. SSE2-only hardware
  therefore stays scalar; `_mm_mul_epi32` from SSE4.1 measures 1.71x on the
  kernel and about 1.16x end to end once the DCT-32 is included.
- **SSE has no saturating 32-bit integer add** (only 8- and 16-bit), which is
  the dominant operation in the DCT-32. Emulating it is still worth it, which
  was not the expected answer and is why it was benchmarked before being
  written off.
- **`_mm_blendv_epi8` selects per byte**, on that byte's high bit. Handing it a
  raw value rather than a lane mask produces output that is right in the high
  bits and wrong in the low ones -- a one-LSB error that a PSNR gate would not
  have caught.

`L3_imdct36` is **not** vectorised, and the reason is structural rather than a
measurement: its inner transform is a 9-point DCT-III whose butterfly does not
map onto four lanes, and vectorising across bands would need stride-18 gathers
because the IMDCT works within a band rather than across them (unlike the
DCT-32, where four consecutive bands are four consecutive int32). Only the
closing twiddle-and-window loop vectorises -- which is exactly what upstream's
float kernel does -- and at ~13% of decode with roughly half the kernel
addressable, the ceiling is around 2%. It is left scalar until something wants
that 2% more than it wants the smaller diff.

## Cortex-M4/M7 DSP evaluation

`SMLAD` and its relatives are dual 16x16 -> 32 multiply-accumulates. The
pipeline's contract is Q26 samples against Q27..Q31 coefficients, i.e. 32-bit
mantissas throughout, and halving either operand to 16 bits costs roughly 16 dB
of the margin the fixed-vs-float gates are currently passing with. `SMMUL` and
`SMMLA` (32x32 -> upper 32) preserve the width but discard the low half, which
is where this decoder's rounding lives -- the whole path rounds half toward
+infinity on the full 64-bit product, and #4055's SIMD kernels are required to
be bit-identical to it. Using them would fork the arithmetic between M4/M7 and
everything else, which is the one property the phase exists to prevent.

The conclusion is that Cortex-M4/M7 stay on the scalar kernels, which the
codegen ledger already covers: `codec_cpu_trend.json` records the cortex-m4
inner-loop counts for both DCT-32 and polyphase, and those are the numbers a
future DSP attempt would have to beat while still proving bit-exactness.

FastLED's source-level integration changes are recorded in `FASTLED.patch`.
They add the `fl::third_party` namespace, `FL_NO_EXCEPT` API annotations, SPDX
markers, the caller-owned `mp3dec_decode_frame_r` scratch API, the fixed-point
DSP path and its integer SIMD kernels.

Nothing in CI regenerates or verifies `FASTLED.patch`, so it can go stale
silently -- it did, between the fixed-point work and FastLED#4056. Regenerate
and verify it with:

```bash
root=$(git rev-parse --show-toplevel)
rev=$(sed -n 's/^upstream_revision = "\(.*\)"/\1/p' \
        "$root/src/third_party/minimp3/manifest.toml")
work=$(mktemp -d); mkdir -p "$work/a" "$work/b"

curl -sSL -o "$work/a/minimp3.h" \
  "https://raw.githubusercontent.com/lieff/minimp3/$rev/minimp3.h"
# The patch's own `index` line names this blob hash; if it disagrees, the wrong
# revision was fetched and nothing below means anything.
git hash-object "$work/a/minimp3.h"

cp "$root/src/third_party/minimp3/minimp3.h" "$work/b/minimp3.h"
( cd "$work" && git diff --no-index a/minimp3.h b/minimp3.h ) \
  | sed -e 's|^diff --git a/a/minimp3.h b/b/minimp3.h$|diff --git a/minimp3.h b/minimp3.h|' \
        -e 's|^--- a/a/minimp3.h$|--- a/minimp3.h|' \
        -e 's|^+++ b/b/minimp3.h$|+++ b/minimp3.h|' \
  > "$root/src/third_party/minimp3/FASTLED.patch"

# Verify: applying it to the pristine upstream file must reproduce ours exactly.
rm -rf "$work/v" && mkdir -p "$work/v"
curl -sSL -o "$work/v/minimp3.h" \
  "https://raw.githubusercontent.com/lieff/minimp3/$rev/minimp3.h"
( cd "$work/v" && patch -p1 < "$root/src/third_party/minimp3/FASTLED.patch" )
diff -q "$work/v/minimp3.h" "$root/src/third_party/minimp3/minimp3.h"
```

`git diff --no-index` labels its output with the paths it was given, so the
`sed` rewrites `a/a/minimp3.h` back to the `a/minimp3.h` the committed patch
uses; without it the patch no longer applies at `-p1`.

FastLED does
not expose upstream's `mp3dec_decode_frame` convenience entry point because it
allocates scratch on the stack; all callers use `mp3dec_decode_frame_r` with
owned storage instead. FastLED uses a 7,808-byte scratch arena for the float
build and 7,936 bytes for the fixed-point build -- the latter carries
scalefactor gains as mantissa+exponent rather than as a single float, and gets
its own figure so that each variant is charged for its own arena. Fixed point
is what ships (FastLED#4056); the float build survives as the reference the
fixed-vs-float gates compare against. FastLED also extends the
decoder's persistent synthesis state so that it can serve as the synthesis
workspace without a duplicate scratch copy. The upstream direct-decoder
2,304-byte free-format frame cap and Phase 0's 4,096-byte stream buffer are
retained; the stream buffer can discover unknown free-format spacing through
2,045 bytes with upstream's three-header validation. The integration meets the
24 KiB working-RAM budget without narrowing that Phase 0 streaming behavior.
