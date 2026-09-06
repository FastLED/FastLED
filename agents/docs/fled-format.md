# .fled Container Format

- `.fled` is FastLED's self-describing LED bundle container: a binary header, a JSON envelope with screenmap/video metadata, and raw frame payload.
- Canonical spec: https://github.com/zackees/ledmapper/blob/main/docs/fled-format.md
- Local FastLED mirror: `src/fl/fled/FLED_FORMAT.md`
- Primary in-tree consumer is `fl::Fled` from #3311 once that subtree lands.
- Legacy `fl::Video` remains relevant for `.rgb` and for consuming the video section inside `.fled`.
- The container is designed to grow beyond video.
- Roadmap sections include channels config plus MicroPython and WASM scripts so one `.fled` can carry video, screenmap, channels, and behavior.

## Source color metadata (`video.color`)

- `video.color` declares what the payload's numbers mean: `{primaries, transfer, matrix, range}`, four independent fields that must never be collapsed into one "BT.709" label.
- Absent metadata resolves to the default tuple `{bt709, srgb, rgb, full}` for formats that define one; that is the historical interpretation and must keep working. Only a declaration that is *present and invalid* is rejected.
- Key inheritance is scoped to formats with a default tuple (the display-encoded RGB family plus `rgb16_linear`). `gray8`/`rgbw8` are all-or-nothing — this is what stops a future YCbCr format from silently inheriting `matrix: "rgb"`.
- `pixel_format` `0x05` is `rgb16_linear` (6 B/LED) and requires `transfer: "linear"`. Reserved values now start at `0x06`.
- FastLED **carries and validates** this declaration; it does not yet transform pixels by it. Its public FLED wire enum is `fl::fled::PixelFormat`; the independent generic storage descriptor is `fl::PixelFormat` plus explicit component byte order. `fl::fled::resolveVideoColor()` / `Fled::videoColor()` report source semantics, while `Fled::videoFrame()` and `Video::readSample()` preserve RGB16 little-endian samples without an RGB8 intermediate. The legacy `Video::draw()` path refuses RGB16 and clears its destination until the managed transform phase lands.
- The producer-side enforcement lives in ledmapper (`packages/gfx/src/render/fled-color.ts` + `tests/unit/fled-color.test.ts`). The two suites are halves of one cross-repo contract: change a rule in one and the other must move in the same change.

### Adding a pixel format is a three-hop cascade

FastLED has **two** `.fled` readers, and adding an enum value to one leaves the other behind:

1. `src/fl/fled/pixel_format.h` — the public on-device C++ wire-format reader and checked mapping to generic storage.
2. `src/platforms/wasm/compiler/package.json` pins `@fastled/gfx` to a **released tarball** (currently `gfx-v0.1.1`), whose bundled reader carries its own bytes-per-LED table. Until that pin moves, the WASM preview rejects any format the release predates.
3. The canonical ledmapper spec + producer.

`rgb16_linear` (`0x05`) is in this state today: FastLED admits it through typed ingress, but `gfx-v0.1.1` predates the corresponding browser-preview support. The preview must fail loudly rather than misparse; the pin moves only after a released producer/parser implementation. The ordering is fixed: **ledmapper merge → gfx release → bump both FastLED package manifests here**.
