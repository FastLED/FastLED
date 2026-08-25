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

Values `0x00` through `0x05` are defined for FLED v1. Values `0x06` through
`0xff` are reserved for future formats.

| Value | Name | Bytes per LED | Payload byte order | Notes |
| ---: | --- | ---: | --- | --- |
| `0x00` | `rgb8` | 3 | `R, G, B` | Current FastLED reader support and the preferred v1 video payload. |
| `0x01` | `gray8` | 1 | `Y` | 8-bit luminance. Consumers expand to RGB as needed. |
| `0x02` | `rgba8` | 4 | `R, G, B, A` | 8-bit RGB plus alpha. Alpha handling is consumer-defined. |
| `0x03` | `rgbw8` | 4 | `R, G, B, W` | 8-bit RGB plus white channel. |
| `0x04` | `rgb565le` | 2 | little-endian RGB565 | 5 bits red, 6 bits green, 5 bits blue. |
| `0x05` | `rgb16_linear` | 6 | `R, G, B` (`u16le` each) | Linear-light 16-bit RGB. Requires `transfer: "linear"`. |
| `0x06`-`0xff` | reserved | variable | TBD | Reserved by ledmapper for future pixel encodings. |

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
- `video.color` may be present to declare the source color encoding of the
  payload. See "Source color metadata" below.
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
    "fps": 30,
    "color": {
      "primaries": "bt709",
      "transfer": "srgb",
      "matrix": "rgb",
      "range": "full"
    }
  }
}
```

## Source Color Metadata

`video.color` describes how the payload's numbers encode color. It describes
the **encoded payload only** — independently of LED layout, output chipset, and
the physical emitter profile of the strip that will display it.

The four fields are independent and must never be collapsed into a single
ambiguous label such as "BT.709":

| field | v1 values | meaning |
| --- | --- | --- |
| `primaries` | `bt709`, `display-p3`, `bt2020`, or a custom object | Chromaticities + white point. `bt709` means the BT.709/sRGB primaries with D65 white. |
| `transfer` | `srgb`, `bt709`, `linear` | The transfer function. `srgb` is the piecewise sRGB function — it is **not** the BT.709 camera OETF and must not be approximated by an unnamed power law. |
| `matrix` | `rgb` | Payload carries direct RGB components; the identity/no-matrix case. YCbCr coefficient values are reserved. |
| `range` | `full` | All codes are image values: for 8-bit, `0` is black and `255` is full channel. `limited` is reserved. |

A custom `primaries` object carries CIE xy pairs:

```json
"primaries": {
  "red":   [0.640, 0.330],
  "green": [0.300, 0.600],
  "blue":  [0.150, 0.060],
  "white": [0.3127, 0.3290]
}
```

`"none"` is not a valid `transfer` value — it is ambiguous. Producers with
genuinely linear-light samples declare `transfer: "linear"` and use a pixel
format whose semantics permit linear data (`rgb16_linear`).

### Default tuple

The canonical default tuple is:

```json
{ "primaries": "bt709", "transfer": "srgb", "matrix": "rgb", "range": "full" }
```

When `video.color` is **absent**, a payload whose pixel format defines a default
tuple is interpreted as that tuple. For the display-encoded RGB formats this
preserves the historical interpretation of `.fled` RGB8 data. Individual missing
keys inherit from the same tuple — but **only** for pixel formats that define
one. Do not describe this default as merely "BT.709"; that leaves the transfer
function unresolved.

### Color classes by pixel format

| pixel_format | color class | default tuple | constraints |
| --- | --- | --- | --- |
| `rgb8`, `rgba8`, `rgb565le` | display-encoded RGB | `{bt709, srgb, rgb, full}` | `transfer` must be `srgb` or `bt709` |
| `rgb16_linear` | linear-light RGB | `{bt709, linear, rgb, full}` | `transfer` must be `linear` |
| `gray8`, `rgbw8` | no defined tuple | none | `video.color` must declare all four keys; absent or partial is unresolvable |

`gray8` carries no chromaticity and `rgbw8`'s white channel is a device
primary that RGB primaries cannot describe, so neither format inherits a
default tuple. Their color metadata is all-or-nothing: absent or partial
resolves to `NoDefaultTuple`, which is *unresolvable*, not *malformed* — the
caller decides whether it cares, rather than treating the file as corrupt.

### Declaration verdicts vs. consumer policy

Rejecting a *declaration* is not the same as refusing a *file*:

- On the display-encoded RGB formats the declaration is **advisory**. A consumer
  that cannot resolve it — an unrecognized name from a future minor, say — may
  fall back to the default tuple and surface a diagnostic, so a newer reader is
  never strictly worse than an older one on the same file.
- On a format whose color semantics are **mandatory** (`rgb16_linear`), an
  unresolvable declaration means the payload cannot be interpreted, and the
  consumer must refuse rather than guess.

Producers and validation tooling always take the strict reading. `resolveVideoColor()`
reports every violation; the latitude above is a playback-consumer policy, and
FastLED does not exercise it yet because nothing here renders from the profile.

### Validation rules

A conforming producer must not write, and a conforming validator must reject:

1. any unrecognized value in any of the four fields — reject with a clear
   diagnostic naming the field and value; never silently fall back;
2. `transfer` of `linear`, `pq`, or `hlg` on a display-encoded RGB format —
   `rgb8` is display-encoded data and must never carry linear-light samples;
3. `rgb16_linear` with any `transfer` other than `linear`;
4. `range: "limited"` on any v1 format — reserved for explicitly labeled
   future/imported payloads;
5. any `matrix` other than `rgb` in v1 — YCbCr payloads need a pixel format
   that does not exist yet, and must then declare their coefficients
   explicitly;
6. a partial `video.color` on a pixel format with no default tuple
   (`gray8`, `rgbw8`) — those must be complete or absent;
7. `video.color` present but not a JSON object, or a custom `primaries` object
   missing any of `red`/`green`/`blue`/`white` or with a malformed xy pair.

An explicit JSON `null` for `video.color` is treated as **absent**, not as a
malformed object: many serializers emit `null` for an unset optional, and
omitting the key must not have a different outcome from nulling it.

`pq` and `hlg` are reserved transfer names. They are rejected in v1: a 16-bit
linear integer payload cannot faithfully carry PQ-decoded content, so HDR
transfers wait for a payload format and working domain that can.

### Forward compatibility

`video.color` is **advisory** for the display-encoded RGB formats. A reader
that predates this section ignores the key and lands on exactly the default
tuple, so old readers degrade to reduced fidelity, never to wrong data. That is
why adding `video.color` is not a version bump.

Payloads whose color semantics are **mandatory** rather than advisory gate on a
new `pixel_format` value instead: `rgb16_linear` is meaningless without its
declaration, and readers that predate it already reject unknown pixel formats.
Mandatory-ness is a property of the payload format, not a version flag.

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
