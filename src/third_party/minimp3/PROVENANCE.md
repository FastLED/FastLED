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
RPSL/RCSL-licensed `libhelix_mp3` tree in this repository was not used as a
source, not even for values that are mathematically forced. Every constant is
regenerated from its ISO 11172-3 / 13818-3 formula by
`ci/codec_tables/generate_minimp3_fixed_tables.py`, which also cross-checks
each generated value against upstream minimp3's own CC0 float literals; that
cross-check is what catches a misread formula, and CI runs it with `--check`.

Design notes live in `minimp3.h` next to the code, but the one worth repeating
here is why the conversion is small: minimp3 normalises its pipeline
internally, so a single Q26 int32 format covers every stage and block floating
point is needed only for scalefactor gains and `x**(4/3)`.

FastLED's source-level integration changes are recorded in `FASTLED.patch`.
They add the `fl::third_party` namespace, `FL_NO_EXCEPT` API annotations, SPDX
markers, and the caller-owned `mp3dec_decode_frame_r` scratch API. FastLED does
not expose upstream's `mp3dec_decode_frame` convenience entry point because it
allocates scratch on the stack; all callers use `mp3dec_decode_frame_r` with
owned storage instead. FastLED uses a 7,808-byte scratch arena for the float
build and 7,936 bytes for the fixed-point build -- the latter carries
scalefactor gains as mantissa+exponent rather than as a single float, and gets
its own figure so that a variant the shipping decoder does not use cannot move
the float decoder's working-RAM ledger. FastLED also extends the
decoder's persistent synthesis state so that it can serve as the synthesis
workspace without a duplicate scratch copy. The upstream direct-decoder
2,304-byte free-format frame cap and Phase 0's 4,096-byte stream buffer are
retained; the stream buffer can discover unknown free-format spacing through
2,045 bytes with upstream's three-header validation. The integration meets the
24 KiB working-RAM budget without narrowing that Phase 0 streaming behavior.
