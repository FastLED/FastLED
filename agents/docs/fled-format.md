# .fled Container Format

- `.fled` is FastLED's self-describing LED bundle container: a binary header, a JSON envelope with screenmap/video metadata, and raw frame payload.
- Canonical spec: https://github.com/zackees/ledmapper/blob/main/docs/fled-format.md
- Local FastLED mirror: `src/fl/fled/FLED_FORMAT.md`
- Primary in-tree consumer is `fl::Fled` from #3311 once that subtree lands.
- Legacy `fl::Video` remains relevant for `.rgb` and for consuming the video section inside `.fled`.
- The container is designed to grow beyond video.
- Roadmap sections include channels config plus MicroPython and WASM scripts so one `.fled` can carry video, screenmap, channels, and behavior.
