#include "fl/fled/fled.h"

#include "fl/fled/fled_impl.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/cstring.h"
#include "fl/stl/json.h"
#include "fl/stl/move.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/system/file_system.h"

namespace fl {

namespace {

// Mirrors src/fl/video/pixel_stream.cpp.hpp constants. Kept local on
// purpose; sharing the parser with fl::Video is a future refactor.
constexpr fl::u8 kMagic[4] = {'F', 'L', 'E', 'D'};
constexpr fl::u8 kVersionV1 = 1;
constexpr fl::size kHeaderBytes = 12;
constexpr fl::size kMaxJsonBytes = 1u * 1024u * 1024u;

const fl::json &nullJson() FL_NO_EXCEPT {
    static fl::json sEmpty;
    return sEmpty;
}

// Parse header + JSON from a byte buffer. On success populates outVersion,
// outPayloadOffset, outEnvelope and returns true.
bool parseHeaderAndEnvelope(const fl::u8 *data, fl::size len,
                            fl::u8 *outVersion, fl::size *outPayloadOffset,
                            fl::json *outEnvelope) FL_NO_EXCEPT {
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
    const fl::u32 jsonLen =
        static_cast<fl::u32>(data[8]) |
        (static_cast<fl::u32>(data[9]) << 8) |
        (static_cast<fl::u32>(data[10]) << 16) |
        (static_cast<fl::u32>(data[11]) << 24);
    const fl::size jsonLenSz = static_cast<fl::size>(jsonLen);
    if (jsonLenSz > kMaxJsonBytes) {
        return false;
    }
    if (jsonLenSz > len - kHeaderBytes) {
        return false;
    }
    // Build a string view over the JSON bytes (fl::string copies the bytes
    // internally — required because json::parse takes const string&).
    fl::string jsonText(fl::reinterpret_cast_<const char *>(data + kHeaderBytes),
                        jsonLenSz);
    fl::json parsed = fl::json::parse(jsonText);
    // parse() returns json(nullptr) on failure — that flags an envelope
    // that is not a valid JSON document. Strict-reject per design.
    if (!parsed.has_value()) {
        return false;
    }
    *outVersion = ver;
    *outPayloadOffset = kHeaderBytes + jsonLenSz;
    *outEnvelope = parsed;
    return true;
}

bool sectionNameIsPayload(const char *name) FL_NO_EXCEPT {
    if (!name) return false;
    if (fl::strcmp(name, "frame_payload") == 0) return true;
    if (fl::strcmp(name, "payload") == 0) return true;
    return false;
}

} // namespace

// ---- factories ----

Fled Fled::load(FileSystem &fs, const char *path) FL_NO_EXCEPT {
    fl::ifstream in = fs.openRead(path);
    if (!in.is_open()) {
        return Fled();
    }
    const fl::size total = in.size();
    fl::vector<fl::u8> buf;
    buf.resize(total);
    if (total > 0) {
        const fl::size got = in.read(buf.data(), total);
        if (got != total) {
            return Fled();
        }
    }
    return Fled::loadFromVector(fl::move(buf));
}

Fled Fled::loadFromStatic(fl::span<const fl::u8> bytes) FL_NO_EXCEPT {
    fl::u8 ver = 0;
    fl::size payloadOffset = 0;
    fl::json env;
    if (!parseHeaderAndEnvelope(bytes.data(), bytes.size(), &ver,
                                 &payloadOffset, &env)) {
        return Fled();
    }
    return Fled(fl::make_shared<FledImpl>(bytes, ver, payloadOffset, env));
}

Fled Fled::loadFromVector(fl::vector<fl::u8> &&bytes) FL_NO_EXCEPT {
    fl::u8 ver = 0;
    fl::size payloadOffset = 0;
    fl::json env;
    if (!parseHeaderAndEnvelope(bytes.data(), bytes.size(), &ver,
                                 &payloadOffset, &env)) {
        return Fled();
    }
    return Fled(fl::make_shared<FledImpl>(fl::move(bytes), ver, payloadOffset,
                                           env));
}

// ---- ctors / observers ----

Fled::Fled() FL_NO_EXCEPT : mImpl() {}

Fled::Fled(fl::shared_ptr<FledImpl> impl) FL_NO_EXCEPT : mImpl(impl) {}

Fled::operator bool() const FL_NO_EXCEPT { return static_cast<bool>(mImpl); }

const fl::json &Fled::json() const FL_NO_EXCEPT {
    if (!mImpl) {
        return nullJson();
    }
    return mImpl->envelope();
}

fl::shared_ptr<const fl::u8> Fled::blob(const char *sectionName,
                                        fl::size *outLen) const FL_NO_EXCEPT {
    if (outLen) *outLen = 0;
    if (!mImpl) {
        return fl::shared_ptr<const fl::u8>();
    }
    if (!sectionNameIsPayload(sectionName)) {
        return fl::shared_ptr<const fl::u8>();
    }
    const fl::size n = mImpl->payloadLen();
    if (n == 0) {
        return fl::shared_ptr<const fl::u8>();
    }
    const fl::u8 *raw = mImpl->bytes() + mImpl->payloadOffset();
    if (outLen) *outLen = n;
    // Aliasing ctor: pins mImpl alive while the returned ptr is held.
    // NOTE: for loadFromStatic, the bytes themselves still live in the
    // caller's static span — see Fled::loadFromStatic contract.
    return fl::shared_ptr<const fl::u8>(mImpl, raw);
}

fl::u8 Fled::version() const FL_NO_EXCEPT {
    return mImpl ? mImpl->versionByte() : fl::u8(0);
}

fl::size Fled::sectionCount() const FL_NO_EXCEPT {
    if (!mImpl) return 0;
    return static_cast<fl::size>(mImpl->envelope().keys().size());
}

} // namespace fl
