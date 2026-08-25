// ok no header - implementation for fl/fled/color.h.

#include "fl/fled/color.h"

#include "fl/fled/detail/pixel_format.h"
#include "fl/stl/cstring.h"
#include "fl/stl/json.h"
#include "fl/stl/string.h"

namespace fl {
namespace fled {

namespace {

// Recognized transfer names. Pq/Hlg are spec-reserved: named so the
// diagnostic can say "reserved" instead of "unrecognized", but rejected by
// every v1 pixel format.
enum class TransferName : fl::u8 { Srgb, Bt709, Linear, Pq, Hlg, Unrecognized };

bool nameIs(const fl::string& s, const char* lit) FL_NO_EXCEPT {
    return fl::strcmp(s.c_str(), lit) == 0;
}

TransferName parseTransferName(const fl::string& s) FL_NO_EXCEPT {
    if (nameIs(s, "srgb"))   return TransferName::Srgb;
    if (nameIs(s, "bt709"))  return TransferName::Bt709;
    if (nameIs(s, "linear")) return TransferName::Linear;
    if (nameIs(s, "pq"))     return TransferName::Pq;
    if (nameIs(s, "hlg"))    return TransferName::Hlg;
    return TransferName::Unrecognized;
}

// Matrix names that standard video metadata defines but that v1 reserves:
// they describe YCbCr coefficient sets, and no v1 pixel format carries YCbCr.
// Distinguished from outright-unknown values so the diagnostic can say
// "reserved" (look it up in the spec) rather than "unrecognized" (you typoed).
bool isReservedMatrixName(const fl::string& s) FL_NO_EXCEPT {
    static const char* const kReserved[] = {
        "bt709", "bt601", "bt470bg", "smpte170m", "bt2020ncl", "bt2020cl",
        "ycbcr", "ycgco", "fcc",
    };
    for (fl::size i = 0; i < sizeof(kReserved) / sizeof(kReserved[0]); ++i) {
        if (nameIs(s, kReserved[i])) return true;
    }
    return false;
}

// Display-encoded RGB family: rgb8, rgba8, rgb565le.
bool isDisplayEncodedRgb(fl::u8 pf) FL_NO_EXCEPT {
    return pf == static_cast<fl::u8>(PixelFormat::Rgb8) ||
           pf == static_cast<fl::u8>(PixelFormat::Rgba8) ||
           pf == static_cast<fl::u8>(PixelFormat::Rgb565Le);
}

bool isLinearRgb(fl::u8 pf) FL_NO_EXCEPT {
    return pf == static_cast<fl::u8>(PixelFormat::Rgb16Linear);
}

// Reads one CIE xy pair: a 2-element array of numbers.
bool readXy(const fl::json& node, float* outX, float* outY) FL_NO_EXCEPT {
    if (!node.is_array()) return false;
    if (!node.contains(static_cast<fl::size>(0)) ||
        !node.contains(static_cast<fl::size>(1))) {
        return false;
    }
    auto x = node[static_cast<fl::size>(0)].as_float();
    auto y = node[static_cast<fl::size>(1)].as_float();
    if (!x || !y) return false;
    *outX = static_cast<float>(*x);
    *outY = static_cast<float>(*y);
    return true;
}

// Custom primaries object: red/green/blue/white, each a CIE xy pair.
bool readCustomPrimaries(const fl::json& node, float* out8) FL_NO_EXCEPT {
    static const char* const kKeys[4] = {"red", "green", "blue", "white"};
    for (fl::size i = 0; i < 4; ++i) {
        const fl::string key(kKeys[i]);
        if (!node.contains(key)) return false;
        if (!readXy(node[key], &out8[i * 2], &out8[i * 2 + 1])) return false;
    }
    return true;
}

}  // namespace

bool pixelFormatHasDefaultTuple(fl::u8 pixelFormat) FL_NO_EXCEPT {
    return isDisplayEncodedRgb(pixelFormat) || isLinearRgb(pixelFormat);
}

bool defaultVideoColor(fl::u8 pixelFormat, VideoColor* out) FL_NO_EXCEPT {
    if (!out) return false;
    if (!pixelFormatHasDefaultTuple(pixelFormat)) return false;
    out->primaries = ColorPrimaries::Bt709;
    out->transfer  = isLinearRgb(pixelFormat) ? ColorTransfer::Linear
                                              : ColorTransfer::Srgb;
    out->matrix    = ColorMatrix::Rgb;
    out->range     = ColorRange::Full;
    out->declared  = false;
    for (fl::size i = 0; i < 8; ++i) {
        out->customPrimaries[i] = 0.0f;
    }
    return true;
}

ColorStatus resolveVideoColor(const fl::json& envelope, fl::u8 pixelFormat,
                              VideoColor* out) FL_NO_EXCEPT {
    if (!out) return ColorStatus::NullOutput;

    const bool hasTuple = pixelFormatHasDefaultTuple(pixelFormat);

    // Locate video.color without requiring either level to exist.
    const fl::string kVideo("video");
    const fl::string kColor("color");
    bool present = false;
    fl::json colorNode;
    if (envelope.is_object() && envelope.contains(kVideo)) {
        const fl::json videoNode = envelope[kVideo];
        if (videoNode.is_object() && videoNode.contains(kColor)) {
            colorNode = videoNode[kColor];
            // An explicit JSON null means "not set" for most serializers.
            // Treat it as absent rather than rejecting the file - omitting
            // the key and nulling it should not have opposite outcomes.
            present = !colorNode.is_null();
        }
    }

    if (!present) {
        // Absent metadata: the format's default tuple is the historical
        // interpretation. Formats without one are simply unresolvable.
        VideoColor resolved;
        if (!defaultVideoColor(pixelFormat, &resolved)) {
            return ColorStatus::NoDefaultTuple;
        }
        *out = resolved;
        return ColorStatus::Ok;
    }

    if (!colorNode.is_object()) {
        return ColorStatus::NotAnObject;
    }

    const fl::string kPrimaries("primaries");
    const fl::string kTransfer("transfer");
    const fl::string kMatrix("matrix");
    const fl::string kRange("range");

    const bool hasP = colorNode.contains(kPrimaries);
    const bool hasT = colorNode.contains(kTransfer);
    const bool hasM = colorNode.contains(kMatrix);
    const bool hasR = colorNode.contains(kRange);

    // Key inheritance is scoped to formats that define a default tuple.
    // Everything else is all-or-nothing, so a future YCbCr format can never
    // silently inherit `matrix: rgb`.
    if (!hasTuple && !(hasP && hasT && hasM && hasR)) {
        return ColorStatus::IncompleteForFormat;
    }

    VideoColor resolved;
    if (!defaultVideoColor(pixelFormat, &resolved)) {
        // No tuple to seed from; every field is explicit here (checked above).
        resolved.primaries = ColorPrimaries::Bt709;
        resolved.transfer  = ColorTransfer::Srgb;
        resolved.matrix    = ColorMatrix::Rgb;
        resolved.range     = ColorRange::Full;
        for (fl::size i = 0; i < 8; ++i) resolved.customPrimaries[i] = 0.0f;
    }
    resolved.declared = true;

    // ---- primaries ----
    if (hasP) {
        const fl::json node = colorNode[kPrimaries];
        if (node.is_object()) {
            if (!readCustomPrimaries(node, resolved.customPrimaries)) {
                return ColorStatus::MalformedCustomPrimaries;
            }
            resolved.primaries = ColorPrimaries::Custom;
        } else if (node.is_string()) {
            auto s = node.as_string();
            if (!s) return ColorStatus::UnknownPrimaries;
            if (nameIs(*s, "bt709"))            resolved.primaries = ColorPrimaries::Bt709;
            else if (nameIs(*s, "display-p3"))  resolved.primaries = ColorPrimaries::DisplayP3;
            else if (nameIs(*s, "bt2020"))      resolved.primaries = ColorPrimaries::Bt2020;
            else return ColorStatus::UnknownPrimaries;
        } else {
            return ColorStatus::UnknownPrimaries;
        }
    }

    // ---- transfer ----
    if (hasT) {
        const fl::json node = colorNode[kTransfer];
        if (!node.is_string()) return ColorStatus::UnknownTransfer;
        auto s = node.as_string();
        if (!s) return ColorStatus::UnknownTransfer;
        switch (parseTransferName(*s)) {
        case TransferName::Srgb:   resolved.transfer = ColorTransfer::Srgb;   break;
        case TransferName::Bt709:  resolved.transfer = ColorTransfer::Bt709;  break;
        case TransferName::Linear: resolved.transfer = ColorTransfer::Linear; break;
        case TransferName::Pq:
        case TransferName::Hlg:
            // Reserved names: no v1 payload format can carry them.
            return ColorStatus::TransferConflictsWithFormat;
        case TransferName::Unrecognized:
            return ColorStatus::UnknownTransfer;
        }
    }

    // Transfer must agree with what the pixel format's bytes actually hold.
    if (isDisplayEncodedRgb(pixelFormat) &&
        resolved.transfer == ColorTransfer::Linear) {
        return ColorStatus::TransferConflictsWithFormat;
    }
    if (isLinearRgb(pixelFormat) && resolved.transfer != ColorTransfer::Linear) {
        return ColorStatus::TransferConflictsWithFormat;
    }

    // ---- matrix ----
    if (hasM) {
        const fl::json node = colorNode[kMatrix];
        if (!node.is_string()) return ColorStatus::UnknownMatrix;
        auto s = node.as_string();
        if (!s) return ColorStatus::UnknownMatrix;
        if (!nameIs(*s, "rgb")) {
            // "reserved" sends the author to the spec; "unrecognized" tells
            // them they typoed. Conflating the two wastes their time.
            return isReservedMatrixName(*s) ? ColorStatus::MatrixUnsupported
                                            : ColorStatus::UnknownMatrix;
        }
        resolved.matrix = ColorMatrix::Rgb;
    }

    // ---- range ----
    if (hasR) {
        const fl::json node = colorNode[kRange];
        if (!node.is_string()) return ColorStatus::UnknownRange;
        auto s = node.as_string();
        if (!s) return ColorStatus::UnknownRange;
        if (nameIs(*s, "full")) {
            resolved.range = ColorRange::Full;
        } else if (nameIs(*s, "limited")) {
            return ColorStatus::LimitedRangeUnsupported;
        } else {
            return ColorStatus::UnknownRange;
        }
    }

    *out = resolved;
    return ColorStatus::Ok;
}

const char* colorStatusMessage(ColorStatus status) FL_NO_EXCEPT {
    switch (status) {
    case ColorStatus::Ok:
        return "ok";
    case ColorStatus::NullOutput:
        return "resolveVideoColor called with a null out-pointer";
    case ColorStatus::NoDefaultTuple:
        return "video.color is absent and this pixel_format defines no default color tuple";
    case ColorStatus::NotAnObject:
        return "video.color must be a JSON object";
    case ColorStatus::UnknownPrimaries:
        return "video.color.primaries is not a recognized name or custom xy object";
    case ColorStatus::UnknownTransfer:
        return "video.color.transfer is not a recognized transfer function";
    case ColorStatus::UnknownMatrix:
        return "video.color.matrix is not a recognized value";
    case ColorStatus::UnknownRange:
        return "video.color.range is not a recognized value";
    case ColorStatus::TransferConflictsWithFormat:
        return "video.color.transfer conflicts with the header pixel_format";
    case ColorStatus::LimitedRangeUnsupported:
        return "video.color.range \"limited\" is reserved and unsupported in v1";
    case ColorStatus::MatrixUnsupported:
        return "video.color.matrix values other than \"rgb\" are reserved in v1";
    case ColorStatus::IncompleteForFormat:
        return "this pixel_format defines no default tuple: video.color must declare all four keys or be absent";
    case ColorStatus::MalformedCustomPrimaries:
        return "video.color.primaries object needs red/green/blue/white CIE xy pairs";
    }
    return "unknown color status";
}

}  // namespace fled
}  // namespace fl
