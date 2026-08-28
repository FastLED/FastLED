#pragma once

// Source color metadata for the .fled v1 container - the `video.color`
// envelope block. See FLED_FORMAT.md "Source Color Metadata" for the
// canonical contract (the authority is the ledmapper spec that file mirrors).
//
// This layer only CARRIES and VALIDATES the declaration. Nothing here feeds
// the render path: FastLED does not yet transform pixels according to the
// declared source profile. Reading a .fled tells you what its numbers mean;
// acting on that is the color-pipeline work tracked separately.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

class json;

namespace fled {

// Chromaticities + white point of the source. Custom carries explicit CIE xy
// pairs in VideoColor::customPrimaries.
enum class ColorPrimaries : fl::u8 {
    Bt709 = 0,   // BT.709 / sRGB primaries, D65 white
    DisplayP3,
    Bt2020,
    Custom,
};

// Transfer function. `Srgb` is the piecewise sRGB function, NOT the BT.709
// camera OETF - the spec keeps them distinct on purpose.
enum class ColorTransfer : fl::u8 {
    Srgb = 0,
    Bt709,
    Linear,
};

// Component encoding. v1 defines only the identity (direct RGB) case;
// YCbCr coefficient sets are reserved for a pixel format that can carry them.
enum class ColorMatrix : fl::u8 {
    Rgb = 0,
};

// Code range. v1 defines only full range; `limited` is reserved.
enum class ColorRange : fl::u8 {
    Full = 0,
};

// Outcome of resolving `video.color` against the header's pixel_format.
// Every non-Ok value is a rejection: FLED_FORMAT.md requires a clear
// diagnostic rather than a silent fallback.
enum class ColorStatus : fl::u8 {
    Ok = 0,
    // Caller passed a null out-pointer. API misuse, not a bad file - kept
    // distinct so a diagnostic never sends someone hunting a valid envelope.
    NullOutput,
    // `video.color` absent on a pixel format that defines no default tuple
    // (gray8, rgbw8). Not malformed - just undeclared, and unresolvable.
    NoDefaultTuple,
    // `video.color` present but not a JSON object.
    NotAnObject,
    UnknownPrimaries,
    UnknownTransfer,
    UnknownMatrix,
    UnknownRange,
    // Rules 2/3: display-encoded formats reject linear/pq/hlg;
    // rgb16_linear rejects anything but linear.
    TransferConflictsWithFormat,
    // Rule 4: `range: "limited"` is reserved in v1.
    LimitedRangeUnsupported,
    // Rule 5: any matrix other than `rgb` is reserved in v1.
    MatrixUnsupported,
    // Rule 6: gray8/rgbw8 must declare all four keys or none.
    IncompleteForFormat,
    // Rule 7: custom primaries object missing a key or with a malformed pair.
    MalformedCustomPrimaries,
};

// A resolved source-color declaration. `declared` distinguishes "the file
// said so" from "the default tuple was applied", which matters because the
// default is a compatibility interpretation, not an author's statement.
struct VideoColor {
    ColorPrimaries primaries;
    ColorTransfer  transfer;
    ColorMatrix    matrix;
    ColorRange     range;
    bool           declared;
    // Valid only when primaries == Custom. Layout:
    // {red.x, red.y, green.x, green.y, blue.x, blue.y, white.x, white.y}
    float          customPrimaries[8];
};

// True if `pixelFormat` defines a default color tuple (the display-encoded
// RGB family and rgb16_linear). gray8 and rgbw8 do not: gray8 carries no
// chromaticity and rgbw8's white is a device primary RGB cannot describe.
bool pixelFormatHasDefaultTuple(fl::u8 pixelFormat) FL_NO_EXCEPT;

// The default tuple for a pixel format, per FLED_FORMAT.md. Returns false
// (leaving *out untouched) for formats that define none.
bool defaultVideoColor(fl::u8 pixelFormat, VideoColor* out) FL_NO_EXCEPT;

// Resolve `envelope["video"]["color"]` against the header's pixel_format,
// applying the default tuple for absent keys where the format defines one
// and enforcing every validation rule in FLED_FORMAT.md.
//
// On ColorStatus::Ok, *out holds the resolved tuple. On any other status
// *out is left untouched and the status names the rejection reason.
ColorStatus resolveVideoColor(const fl::json& envelope, fl::u8 pixelFormat,
                              VideoColor* out) FL_NO_EXCEPT;

// Stable human-readable text for a status, for diagnostics. Never null.
const char* colorStatusMessage(ColorStatus status) FL_NO_EXCEPT;

}  // namespace fled
}  // namespace fl
