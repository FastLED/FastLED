data/ directory will appear automatically in emscripten web builds.

This sketch supports two on-disk formats:

  * **FLED v1 container** (`*.fled`) — preferred:
      - `video.fled`              — header + v2 screenmap + raw RGB frames
      - `color_line_bubbles.fled` — header + v2 screenmap + raw RGB frames
    Spec: https://github.com/zackees/ledmapper/blob/main/docs/fled-format.md
    The 12-byte header carries the screenmap inline; no sidecar file is
    needed when these are loaded.

  * **Legacy headerless** + sidecar (kept for back-compat):
      - `*.rgb`             — raw RGB video data, 3 bytes per LED per frame
      - `screenmap.json`    — v1 ScreenMap for "strip1"

The sketch tries the .fled files first and falls back to the .rgb pair
if no .fled is on the SD card.

The .fled files were produced by `wrap-fled.mjs` from the .rgb + v1
screenmap.json. To regenerate (e.g. after editing screenmap.json):

  cd examples/Fx/FxSdCard/data
  node wrap-fled.mjs screenmap.json video.rgb              video.fled
  node wrap-fled.mjs screenmap.json color_line_bubbles.rgb color_line_bubbles.fled
