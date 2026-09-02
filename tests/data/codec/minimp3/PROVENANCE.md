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
- `l3-si_huff.bit` and `.pcm`: MPEG-1 Layer III side-info/Huffman stress. This
  is the vector that exposed FastLED#4127: it drives the dequantised samples to
  3.89, six times higher than any other vector with a reference, and the
  fixed-point path clamped them to 1.0 and lost 81 dB. Vendored specifically so
  that regression cannot come back silently. The rest of the suite is exercised
  by `ci/codec_conformance/run.py`, which fetches all 83 reference vectors from
  the pinned upstream revision rather than vendoring 19 MB of PCM here.

The `.pcm` file contains signed 16-bit little-endian interleaved reference
samples. The Phase 0 golden test applies the upstream decoder's standard
length allowance and a 60 dB limited-accuracy floor to both FastLED MP3
backends.
