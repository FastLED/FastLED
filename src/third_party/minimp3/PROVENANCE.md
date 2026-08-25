# minimp3 provenance

- Component: `lieff/minimp3`
- Upstream repository: <https://github.com/lieff/minimp3>
- Upstream commit: `ea99364f61c14656440e8d77e9c233ccf3124633`
- Commit date: 2022-06-25
- License: CC0-1.0 (`LICENSE`)
- Vendored files: `minimp3.h`

Production and host-golden decoding both use the core `minimp3.h` API with
`MINIMP3_NO_STDIO`; the optional upstream `minimp3_ex.h` stdio/file layer is
not vendored.

FastLED's source-level integration changes are recorded in `FASTLED.patch`.
They add the `fl::third_party` namespace, `FL_NO_EXCEPT` API annotations, SPDX
markers, and the caller-owned `mp3dec_decode_frame_r` scratch API. FastLED does
not expose upstream's `mp3dec_decode_frame` convenience entry point because it
allocates scratch on the stack; all callers use `mp3dec_decode_frame_r` with
owned storage instead. FastLED uses a 7,808-byte scratch arena and extends the
decoder's persistent synthesis state so that it can serve as the synthesis
workspace without a duplicate scratch copy. The upstream direct-decoder
2,304-byte free-format frame cap and Phase 0's 4,096-byte stream buffer are
retained; the stream buffer can discover unknown free-format spacing through
2,045 bytes with upstream's three-header validation. The integration meets the
24 KiB working-RAM budget without narrowing that Phase 0 streaming behavior.
