# FLED v1 Container Format

`.fled` is the FastLED native container for self-contained LED pattern data.
Version 1 stores a small binary header, a UTF-8 JSON envelope that carries the
physical LED map and optional metadata, then a raw frame payload. The goal is to
keep the bytes needed for playback together: video data, the screenmap that
places that data on physical LEDs, and future bundle sections such as channels
configuration and scripts.

> Authority: the canonical `.fled` format specification lives in ledmapper at
> <https://github.com/zackees/ledmapper/blob/main/docs/fled-format.md>.
> ledmapper is the producer of `.fled` files, so if this local mirror and the
> ledmapper spec ever disagree, ledmapper wins. Keep this file as a convenience
> mirror for FastLED readers, agents, and on-device consumers.

Diagnostic tooling lives with the producer:
<https://github.com/zackees/ledmapper/blob/main/scripts/inspect-fled.mjs>.
Use that inspector to dump headers, JSON envelope contents, and payload sizing
when validating generated files.

## File Layout

All integers are unsigned. Multi-byte integers are little-endian. The binary
header is exactly 12 bytes.

| Offset | Size | Field | Type | Notes |
| ---: | ---: | --- | --- | --- |
| 0 | 4 | `magic` | ASCII | Must be `FLED`. |
| 4 | 1 | `version` | `u8` | Must be `1` for this version. |
| 5 | 1 | `pixel_format` | `u8` | Pixel format enum for the raw frame payload. |
| 6 | 2 | `reserved` | `u8[2]` | Must be zero. |
| 8 | 4 | `json_length` | `u32le` | Number of bytes in the UTF-8 JSON envelope. |
| 12 | `json_length` | `json_bytes` | UTF-8 | JSON envelope, with no NUL terminator and no BOM. |
| `12 + json_length` | remaining bytes | `frame_payload` | bytes | Concatenated frame data using `pixel_format`. |

The payload begins at `12 + json_length`. For video content, the frame count is
derived from the remaining file size:

```text
frame_count = payload_bytes / (led_count * bytes_per_led)
```

`led_count` comes from the embedded screenmap. `bytes_per_led` comes from the
pixel format table below. Writers should make `payload_bytes` an exact multiple
of `led_count * bytes_per_led`; readers should treat a remainder as malformed or
truncated input.

## Pixel Format Enum

Values `0x00` through `0x04` are defined for FLED v1. Values `0x05` through
`0xff` are reserved for future formats.

| Value | Name | Bytes per LED | Payload byte order | Notes |
| ---: | --- | ---: | --- | --- |
| `0x00` | `rgb8` | 3 | `R, G, B` | Current FastLED reader support and the preferred v1 video payload. |
| `0x01` | `gray8` | 1 | `Y` | 8-bit luminance. Consumers expand to RGB as needed. |
| `0x02` | `rgba8` | 4 | `R, G, B, A` | 8-bit RGB plus alpha. Alpha handling is consumer-defined. |
| `0x03` | `rgbw8` | 4 | `R, G, B, W` | 8-bit RGB plus white channel. |
| `0x04` | `rgb565le` | 2 | little-endian RGB565 | 5 bits red, 6 bits green, 5 bits blue. |
| `0x05`-`0xff` | reserved | variable | TBD | Reserved by ledmapper for future pixel encodings. |

## JSON Envelope

The JSON envelope is UTF-8 text and is counted exactly by `json_length`.
It is intentionally extensible: readers should consume the sections they
understand and preserve or ignore unknown sections according to their role.

Required screenmap content:

- The envelope carries the LED geometry needed to interpret the payload.
- Current files carry the standard `ScreenMap` schema in `map`.
- The LED order in the screenmap is the order used by the frame payload.
- `led_count` is the total number of LEDs described by the screenmap segments.

Video metadata:

- `video.fps` may be present to declare the playback frame rate.
- If `video.fps` is absent, consumers may use an application default, sketch
  parameter, or external playback setting.
- The raw frame payload immediately follows the JSON envelope; it is not base64
  or otherwise embedded in JSON.

Example envelope shape:

```json
{
  "map": {
    "strip0": {
      "x": [0, 1, 2],
      "y": [0, 0, 0],
      "diameter": 0.25
    }
  },
  "video": {
    "fps": 30
  }
}
```

## Frame Payload

The frame payload is a flat byte stream:

```text
frame 0 LED 0, frame 0 LED 1, ... frame 0 LED N-1,
frame 1 LED 0, frame 1 LED 1, ... frame 1 LED N-1,
...
```

Each LED record uses the `pixel_format` declared in the header. For `rgb8`, the
payload is identical to the legacy headerless `.rgb` layout after the FLED
header and JSON envelope are skipped.

Consumers that only support a subset of pixel formats should reject unsupported
`pixel_format` values before reading frame bytes. FastLED's legacy video reader
currently accepts `rgb8` FLED v1 files and falls back to headerless `.rgb` when
the `FLED` magic is absent.

## Growth Notes

FLED v1 reserves most of the pixel-format enum and keeps the metadata envelope
open so one file can grow from "video plus screenmap" into a complete pattern
bundle.

Expected pixel-format growth includes compressed or alternate encodings such as
BC1/BC3-compressed frames, indexed palettes, higher-bit-depth color, or other
producer-defined formats. New values must be assigned in the canonical
ledmapper spec first.

Expected JSON envelope growth includes:

- `channels`: `fl::MultiChannelConfig` JSON so the file can describe output
  channel wiring and playback routing alongside the screenmap.
- `script.micropython`: embedded MicroPython bytecode or source metadata for
  behavior that travels with the pattern.
- `script.wasm`: embedded WASM module metadata or payload references for
  portable behavior that travels with the pattern.

The roadmap direction is a single `.fled` file that can carry video, screenmap,
channels, and behavior as a self-contained FastLED deployment unit. The
channels section is the next v1 addition tracked under the `fl::Fled` umbrella;
MicroPython and WASM script-carrying sections remain roadmap items until they
land in ledmapper and FastLED together.
