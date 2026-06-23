# fl::Fled

> **On-disk format spec:** [FLED_FORMAT.md](./FLED_FORMAT.md) - the binary header, JSON envelope, and raw frame payload layout. Canonical spec lives in [zackees/ledmapper](https://github.com/zackees/ledmapper/blob/main/docs/fled-format.md); this is the local mirror.

`fl::Fled` covers FastLED's documented video container format. The format spec is the source of truth for the bytes stored on disk and for compatibility expectations between writers and readers.
