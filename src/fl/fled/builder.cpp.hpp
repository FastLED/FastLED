// ok no header - implementation file for fl::fled::FledBuilder
// declared in fl/fled/builder.h. Hand-builds a v1 .fled byte buffer
// from header bytes + optional JSON sections + payload, then routes
// through Fled::loadFromVector for the canonical parse path.

#include "fl/fled/builder.h"

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
                         const fl::string& channelsJson) FL_NO_EXCEPT {
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
    out += '}';
    return out;
}

} // namespace

FledBuilder::FledBuilder() FL_NO_EXCEPT
    : mVersion(1),
      mPixelFormat(0),
      mScreenMapJson(),
      mChannelsJson(),
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

FledBuilder& FledBuilder::setPayload(fl::span<const fl::u8> bytes) FL_NO_EXCEPT {
    mPayload.assign(bytes.begin(), bytes.end());
    return *this;
}

fl::Fled FledBuilder::build() const FL_NO_EXCEPT {
    const fl::string envelope = buildEnvelope(mScreenMapJson, mChannelsJson);
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
