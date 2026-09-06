// ok no header - implementation file for fl::fled::FledBuilder
// declared in fl/fled/builder.h. Hand-builds a v1 .fled byte buffer
// from header bytes + optional JSON sections + payload, then routes
// through Fled::loadFromVector for the canonical parse path.

#include "fl/fled/builder.h"

#include "fl/fled/color.h"
#include "fl/fled/fled.h"
#include "fl/stl/int.h"
#include "fl/stl/move.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"

namespace fl {
namespace fled {

namespace {

// Append a u32 little-endian to a byte vector.
void appendU32LE(fl::vector<fl::u8>& out, fl::u32 v) FL_NO_EXCEPT {
    out.push_back(static_cast<fl::u8>(v & 0xff));
    out.push_back(static_cast<fl::u8>((v >> 8) & 0xff));
    out.push_back(static_cast<fl::u8>((v >> 16) & 0xff));
    out.push_back(static_cast<fl::u8>((v >> 24) & 0xff));
}

// Append the characters of a fl::string to a byte vector.
void appendString(fl::vector<fl::u8>& out, const fl::string& s) FL_NO_EXCEPT {
    const fl::size n = s.size();
    for (fl::size i = 0; i < n; ++i) {
        out.push_back(static_cast<fl::u8>(s[i]));
    }
}

// Build the JSON envelope by concatenating the configured sections under
// their canonical top-level keys. If no sections are configured the
// envelope is "{}".
fl::string buildEnvelope(const fl::string& screenMapJson,
                         const fl::string& channelsJson,
                         const fl::string& videoColorJson) FL_NO_EXCEPT {
    fl::string out;
    out += '{';
    bool first = true;
    if (!screenMapJson.empty()) {
        out += "\"map\":";
        out += screenMapJson;
        first = false;
    }
    if (!channelsJson.empty()) {
        if (!first) out += ',';
        out += "\"channels\":";
        out += channelsJson;
        first = false;
    }
    if (!videoColorJson.empty()) {
        if (!first) out += ',';
        out += "\"video\":{\"color\":";
        out += videoColorJson;
        out += '}';
        first = false;
    }
    out += '}';
    return out;
}

// Spellings are the ones FLED_FORMAT.md defines. They are written here rather
// than derived from the enum so that renaming a C++ enumerator cannot silently
// change the wire format.
const char* primariesName(ColorPrimaries p) FL_NO_EXCEPT {
    switch (p) {
    case ColorPrimaries::Bt709:     return "bt709";
    case ColorPrimaries::DisplayP3: return "display-p3";
    case ColorPrimaries::Bt2020:    return "bt2020";
    case ColorPrimaries::Custom:    return nullptr;  // emitted as an object
    }
    return "bt709";
}

const char* transferName(ColorTransfer t) FL_NO_EXCEPT {
    switch (t) {
    case ColorTransfer::Srgb:   return "srgb";
    case ColorTransfer::Bt709:  return "bt709";
    case ColorTransfer::Linear: return "linear";
    }
    return "srgb";
}

// A CIE xy pair, four decimals -- enough for the D65 white point (0.3127,
// 0.3290) to survive the round trip exactly.
void appendXy(fl::string& out, float x, float y) FL_NO_EXCEPT {
    out += '[';
    out += fl::to_string(x, 4);
    out += ',';
    out += fl::to_string(y, 4);
    out += ']';
}

fl::string serializeVideoColor(const VideoColor& c) FL_NO_EXCEPT {
    fl::string out;
    out += "{\"primaries\":";
    const char* named = primariesName(c.primaries);
    if (named != nullptr) {
        out += '"';
        out += named;
        out += '"';
    } else {
        out += "{\"red\":";
        appendXy(out, c.customPrimaries[0], c.customPrimaries[1]);
        out += ",\"green\":";
        appendXy(out, c.customPrimaries[2], c.customPrimaries[3]);
        out += ",\"blue\":";
        appendXy(out, c.customPrimaries[4], c.customPrimaries[5]);
        out += ",\"white\":";
        appendXy(out, c.customPrimaries[6], c.customPrimaries[7]);
        out += '}';
    }
    out += ",\"transfer\":\"";
    out += transferName(c.transfer);
    // v1 defines exactly one value for each of matrix and range; the parser
    // rejects anything else, so emitting them literally keeps producer and
    // parser from drifting apart.
    out += "\",\"matrix\":\"rgb\",\"range\":\"full\"}";
    return out;
}

} // namespace

FledBuilder::FledBuilder() FL_NO_EXCEPT
    : mVersion(1),
      mPixelFormat(0),
      mScreenMapJson(),
      mChannelsJson(),
      mVideoColorJson(),
      mPayload() {}

FledBuilder& FledBuilder::setVersion(fl::u8 v) FL_NO_EXCEPT {
    mVersion = v;
    return *this;
}

FledBuilder& FledBuilder::setPixelFormat(fl::u8 fmt) FL_NO_EXCEPT {
    mPixelFormat = fmt;
    return *this;
}

FledBuilder& FledBuilder::setScreenMapJson(const char* json) FL_NO_EXCEPT {
    mScreenMapJson = (json != nullptr) ? fl::string(json) : fl::string();
    return *this;
}

FledBuilder& FledBuilder::setChannelsJson(const char* json) FL_NO_EXCEPT {
    mChannelsJson = (json != nullptr) ? fl::string(json) : fl::string();
    return *this;
}

FledBuilder& FledBuilder::setVideoColor(const VideoColor& color) FL_NO_EXCEPT {
    mVideoColorJson = serializeVideoColor(color);
    return *this;
}

FledBuilder& FledBuilder::setPayload(fl::span<const fl::u8> bytes) FL_NO_EXCEPT {
    mPayload.assign(bytes.begin(), bytes.end());
    return *this;
}

fl::Fled FledBuilder::build() const FL_NO_EXCEPT {
    const fl::string envelope =
        buildEnvelope(mScreenMapJson, mChannelsJson, mVideoColorJson);
    const fl::u32 jsonLen = static_cast<fl::u32>(envelope.size());

    fl::vector<fl::u8> buf;
    buf.reserve(12 + envelope.size() + mPayload.size());

    // 12-byte header: 4-byte magic, version, pixel_format, 2 reserved,
    // u32 little-endian JSON length.
    buf.push_back('F');
    buf.push_back('L');
    buf.push_back('E');
    buf.push_back('D');
    buf.push_back(mVersion);
    buf.push_back(mPixelFormat);
    buf.push_back(0);  // reserved
    buf.push_back(0);  // reserved
    appendU32LE(buf, jsonLen);

    // JSON envelope.
    appendString(buf, envelope);

    // Optional frame payload (already a copy held by the builder).
    for (fl::size i = 0; i < mPayload.size(); ++i) {
        buf.push_back(mPayload[i]);
    }

    return fl::Fled::loadFromVector(fl::move(buf));
}

} // namespace fled
} // namespace fl
