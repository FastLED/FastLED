#pragma once

/// @file fl/gfx/crgb_json.h
/// @brief JSON serialization helpers for CRGB pixel buffers.
///
/// The encoded schema is versioned so persisted data can be validated before
/// it replaces a live pixel buffer:
/// @code
/// {"version":1,"pixels":[[255,0,0],[0,255,0],[0,0,255]]}
/// @endcode
///
/// These helpers convert pixels to and from fl::json. Choosing and operating a
/// persistent store (LittleFS, SD, EEPROM, and so on) remains platform-specific.

#include "fl/gfx/crgb.h"
#include "fl/stl/json.h"
#include "fl/stl/optional.h"
#include "fl/stl/span.h"

namespace fl {

/// @brief Encode a CRGB pixel buffer as a versioned JSON object.
/// @param pixels Pixel buffer to encode.
/// @return JSON object with `version` and `pixels` fields, or nullopt if the
///         document could not be built (for example, due to allocation
///         failure).
optional<json> crgbToJson(span<const CRGB> pixels) FL_NO_EXCEPT;

/// @brief Decode a versioned JSON object into an existing CRGB pixel buffer.
///
/// The document must use schema version 1 and contain exactly one RGB triplet
/// per output pixel. Every channel must be an integer from 0 through 255. The
/// complete document is validated before any output pixel is changed.
///
/// @param document JSON object produced by crgbToJson().
/// @param pixels Destination pixel buffer.
/// @return true on success; false if the document is invalid. On failure,
///         `pixels` is left unchanged.
bool crgbFromJson(const json& document, span<CRGB> pixels) FL_NO_EXCEPT;

} // namespace fl
