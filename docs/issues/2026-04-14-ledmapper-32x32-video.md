# Problem Statement: FASTLED4 32x32 Video Demo

GitHub issue: https://github.com/FastLED/FastLED/issues/2257

Create a FASTLED4 video demo that plays `data/video1.rgb` as a 32x32 square-mapped raw RGB asset using `data/screenmap.json` (`strip1`) as the mapping resource. The demo should behave the same way in both environments: bundled local files for WASM preview and SD-backed files for hardware playback. This is not a new mapping problem; the required screen map already exists and should be used as-is.

## Required Resources

- Video asset: `examples/Fx/FxLedmapper32x32/data/video1.rgb`
- Screen map: `examples/Fx/FxLedmapper32x32/data/screenmap.json`
- Screen map source: `C:\Users\niteris\dev\ledmapper\public\screenmaps\32x32_quad_serpentine.json`
- Sketch: `examples/Fx/FxLedmapper32x32/FxLedmapper32x32.ino`

## Scope

- Use the existing 32x32 square mapping only.
- Use the same file-backed asset layout for WASM and SD-card flows.
- Keep playback focused on this single `video1.rgb` file.

## Done When

- FASTLED4 loads `video1.rgb`.
- FASTLED4 loads `screenmap.json` for `strip1`.
- The same asset/mapping pair works as a WASM-bundled demo and as an SD-card-backed demo.
