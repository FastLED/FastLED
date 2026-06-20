#include "fl/fled/detail/parser.h"

#include "fl/stl/bit_cast.h"
#include "fl/stl/cstring.h"
#include "fl/stl/json.h"
#include "fl/stl/string.h"

namespace fl {
namespace fled {

namespace {

// Mirrors src/fl/video/pixel_stream.cpp.hpp constants. Kept local on
// purpose; sharing the parser with fl::Video is a future refactor.
constexpr fl::u8 kMagic[4] = {'F', 'L', 'E', 'D'};
constexpr fl::u8 kVersionV1 = 1;
constexpr fl::size kHeaderBytes = 12;
constexpr fl::size kMaxJsonBytes = 1u * 1024u * 1024u;

} // namespace

bool parseHeaderAndEnvelope(const fl::u8* data, fl::size len,
                            ParsedHeader* outHeader,
                            fl::json* outEnvelope) FL_NO_EXCEPT {
    if (!outHeader || !outEnvelope) {
        return false;
    }
    if (!data) {
        return false;
    }
    if (len < kHeaderBytes) {
        return false;
    }
    if (data[0] != kMagic[0] || data[1] != kMagic[1] ||
        data[2] != kMagic[2] || data[3] != kMagic[3]) {
        return false;
    }
    const fl::u8 ver = data[4];
    if (ver != kVersionV1) {
        return false;
    }
    const fl::u8 pixelFormat = data[5];
    const fl::u32 jsonLen =
        static_cast<fl::u32>(data[8]) |
        (static_cast<fl::u32>(data[9]) << 8) |
        (static_cast<fl::u32>(data[10]) << 16) |
        (static_cast<fl::u32>(data[11]) << 24);
    // Bound-check in u32 BEFORE narrowing to fl::size - on 16-bit targets
    // (AVR) fl::size is 16 bits and naive narrowing would wraparound.
    if (jsonLen > static_cast<fl::u32>(kMaxJsonBytes)) {
        return false;
    }
    if (jsonLen > static_cast<fl::u32>(len - kHeaderBytes)) {
        return false;
    }
    const fl::size jsonLenSz = static_cast<fl::size>(jsonLen);
    // Build a string view over the JSON bytes (fl::string copies the bytes
    // internally - required because json::parse takes const string&).
    fl::string jsonText(fl::reinterpret_cast_<const char *>(data + kHeaderBytes),
                        jsonLenSz);
    fl::json parsed = fl::json::parse(jsonText);
    // parse() returns json(nullptr) on failure - that flags an envelope
    // that is not a valid JSON document. Strict-reject per design.
    if (!parsed.has_value()) {
        return false;
    }
    outHeader->version = ver;
    outHeader->pixelFormat = pixelFormat;
    outHeader->payloadOffset = kHeaderBytes + jsonLenSz;
    *outEnvelope = parsed;
    return true;
}

bool sectionNameIsPayload(const char* name) FL_NO_EXCEPT {
    if (!name) return false;
    if (fl::strcmp(name, "frame_payload") == 0) return true;
    if (fl::strcmp(name, "payload") == 0) return true;
    return false;
}

} // namespace fled
} // namespace fl
