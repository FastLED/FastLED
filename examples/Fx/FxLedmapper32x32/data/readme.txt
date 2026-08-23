data/ directory is bundled automatically for emscripten web builds.

  * screenmap.json is the 32x32 screen map resource for `strip1`.
    - Source: ledmapper 32x32 quad serpentine preset.
    - sha256: a6851e2a5eaf728b0018f1a32918969b2e90673ed2c18c1a3a5d7327cf5993d7
    - Kept local because it is small (13,790 bytes) and required to build the screen map.

  * video1.rgb is NOT stored in this repo. It is hosted in FastLED/assets and
    referenced by the sidecar file `video1.rgb.lnk`.

    The .lnk format is fbuild's native JSON descriptor:

      { "sha256": "<hex>", "size": <int>, "url": "<url>", "v": 1 }

    Generate one with:  fbuild lnk add <url> -o <name>.lnk
    Resolve them with:  fbuild lnk pull            (fetches into the global cache)
    Verify them with:   fbuild lnk check           (offline sha256 check)

    fbuild already resolves .lnk files at build time and caches blobs by sha256
    in ~/.fbuild/prod/cache/archives/lnk-blobs/, shared across all projects.

    A build-time resolver reads `<name>.lnk`, fetches the URL, verifies the
    sha256 if present, and materializes `<name>` beside it. See FastLED/fbuild#1354.

    video1.rgb details:
    - sha256: 164804d94802dd96cd8cd37f7528ca6d30f8fd9396e812fe67ec6164bbdd2282
    - Size: 6,051,840 bytes.
    - Frame size at 32x32 RGB888: 3,072 bytes.
    - Total frames: 1,970.
    - Expected playback duration:
      - 65.67s at 30 FPS
      - 32.83s at 60 FPS
    - The sketch defaults to VIDEO_FPS = 30. Change that constant if the export
      was authored at a different rate.

  * To fetch the video before building:

      fbuild lnk pull examples/Fx/FxLedmapper32x32

    Note this populates fbuild's global blob cache. Wiring that cache through
    to a deployable filesystem image (LittleFS) is tracked in FastLED/fbuild#1354.

  * A standalone bundle containing video1.rgb, screenmap.json, and a readme is
    available at:
      https://cdn.jsdelivr.net/gh/FastLED/assets@main/examples/FxLedmapper32x32/ledmapper-32x32-video1-asset.zip

  * Full asset manifest (zackees/manifest.json format, sha256-verified):
      https://raw.githubusercontent.com/FastLED/assets/main/examples/FxLedmapper32x32/manifest.json

  * Assumption: the asset is already mapped into physical LED order for the
    target ledmapper.com model.
