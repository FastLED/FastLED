# MP3 conformance vector provenance

These files are mirrored by `lieff/minimp3` at upstream commit
`ea99364f61c14656440e8d77e9c233ccf3124633` under `vectors/`:

- `l3-hecommon.bit` and `.pcm`: MPEG Layer III limited-accuracy vector.
- `l3-he_free.bit`: MPEG-1 Layer III free-format stream.
- `M2L3_bitrate_16_all.bit`: MPEG-2 Layer III bitrate coverage at 16 kHz.
- `l3-lame-vbrtag.bit`: LAME VBR-tag synthetic (upstream name
  `l3-nonstandard-sin1k0db_lame_vbrtag.bit`).
- `l3-compl-cut.mp3`: truncated-input fuzz regression from `vectors/fuzz/`.
- `ILL2_layer1.bit` and `.pcm`: MPEG Layer I synthesis vector used to cover
  minimp3's 12-band in-place QMF state slide.

The `.pcm` file contains signed 16-bit little-endian interleaved reference
samples. The Phase 0 golden test applies the upstream decoder's standard
length allowance and a 60 dB limited-accuracy floor to both FastLED MP3
backends.
