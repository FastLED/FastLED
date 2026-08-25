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
markers, and the caller-owned `mp3dec_decode_frame_r` scratch API. The original
`mp3dec_decode_frame` entry point remains available for upstream compatibility,
but it allocates its 16,384-byte scratch object on the stack. Constrained
targets must use `mp3dec_decode_frame_r` with caller-owned storage instead.
