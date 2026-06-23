#include "fl/fled/fled.h"

#include "fl/channels/config.h"
#include "fl/fled/detail/fled_impl.hpp"
#include "fl/fled/detail/parser.h"
#include "fl/fled/detail/pixel_format.h"
#include "fl/math/screenmap.h"
#include "fl/stl/flat_map.h"
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

const fl::json &nullJson() FL_NO_EXCEPT {
    static fl::json sEmpty;
    return sEmpty;
}

}  // namespace

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
    fl::fled::ParsedHeader hdr{};
    fl::json env;
    if (!fl::fled::parseHeaderAndEnvelope(bytes.data(), bytes.size(), &hdr,
                                          &env)) {
        return Fled();
    }
    return Fled(fl::make_shared<fl::fled::FledImpl>(
        bytes, hdr.version, hdr.pixelFormat, hdr.payloadOffset, env));
}

Fled Fled::loadFromVector(fl::vector<fl::u8> &&bytes) FL_NO_EXCEPT {
    fl::fled::ParsedHeader hdr{};
    fl::json env;
    if (!fl::fled::parseHeaderAndEnvelope(bytes.data(), bytes.size(), &hdr,
                                          &env)) {
        return Fled();
    }
    return Fled(fl::make_shared<fl::fled::FledImpl>(
        fl::move(bytes), hdr.version, hdr.pixelFormat, hdr.payloadOffset, env));
}

// ---- ctors / observers ----

Fled::Fled() FL_NO_EXCEPT : mImpl() {}

Fled::Fled(fl::shared_ptr<fl::fled::FledImpl> impl) FL_NO_EXCEPT
    : mImpl(impl) {}

Fled::operator bool() const FL_NO_EXCEPT { return static_cast<bool>(mImpl); }

// ---- header info ----

fl::u8 Fled::version() const FL_NO_EXCEPT {
    return mImpl ? mImpl->versionByte() : fl::u8(0);
}

fl::u8 Fled::pixelFormat() const FL_NO_EXCEPT {
    return mImpl ? mImpl->pixelFormatByte() : fl::u8(0);
}

fl::u8 Fled::bytesPerLed() const FL_NO_EXCEPT {
    return mImpl ? fl::fled::bytesPerLed(mImpl->pixelFormatByte()) : fl::u8(0);
}

fl::size Fled::payloadBytes() const FL_NO_EXCEPT {
    return mImpl ? mImpl->payloadLen() : fl::size(0);
}

fl::size Fled::frameCount(fl::size ledCount) const FL_NO_EXCEPT {
    if (!mImpl || ledCount == 0) return 0;
    const fl::u8 bpp = fl::fled::bytesPerLed(mImpl->pixelFormatByte());
    if (bpp == 0) return 0;
    const fl::size perFrame = ledCount * static_cast<fl::size>(bpp);
    if (perFrame == 0) return 0;
    return mImpl->payloadLen() / perFrame;
}

// ---- JSON ----

const fl::json &Fled::json() const FL_NO_EXCEPT {
    if (!mImpl) {
        return nullJson();
    }
    return mImpl->envelope();
}

fl::size Fled::sectionCount() const FL_NO_EXCEPT {
    if (!mImpl) return 0;
    return static_cast<fl::size>(mImpl->envelope().keys().size());
}

float Fled::videoFps(float defaultFps) const FL_NO_EXCEPT {
    if (!mImpl) return defaultFps;
    const fl::json &env = mImpl->envelope();
    if (!env.contains(fl::string("video")) || !env["video"].is_object()) {
        return defaultFps;
    }
    const fl::json videoNode = env["video"];
    if (!videoNode.contains(fl::string("fps"))) {
        return defaultFps;
    }
    auto fpsOpt = videoNode["fps"].as_float();
    if (!fpsOpt) return defaultFps;
    return static_cast<float>(*fpsOpt);
}

// ---- blobs ----

fl::shared_ptr<const fl::u8> Fled::blob(const char *sectionName,
                                        fl::size *outLen) const FL_NO_EXCEPT {
    if (outLen) *outLen = 0;
    if (!mImpl) {
        return fl::shared_ptr<const fl::u8>();
    }
    if (!fl::fled::sectionNameIsPayload(sectionName)) {
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
    // caller's static span - see Fled::loadFromStatic contract.
    return fl::shared_ptr<const fl::u8>(mImpl, raw);
}

// ---- typed section accessors ----

fl::shared_ptr<ScreenMap> Fled::screenMap() const FL_NO_EXCEPT {
    if (!mImpl) {
        return fl::shared_ptr<ScreenMap>();
    }
    const fl::json &env = mImpl->envelope();
    // The v1 envelope carries the screen map under the "map" key. The
    // canonical parser is ScreenMap::ParseJson, which takes a JSON
    // document containing a top-level "map" object - exactly the shape
    // of the envelope, so we hand it straight to ParseJson.
    if (!env.contains(fl::string("map")) || !env["map"].is_object()) {
        return fl::shared_ptr<ScreenMap>();
    }
    fl::string envText = env.to_string();
    fl::flat_map<fl::string, ScreenMap> segments;
    if (!ScreenMap::ParseJson(envText.c_str(), &segments)) {
        return fl::shared_ptr<ScreenMap>();
    }
    if (segments.empty()) {
        return fl::shared_ptr<ScreenMap>();
    }
    // Single-strip path: return the first segment. Multi-strip support
    // would be a dedicated accessor (out of scope here).
    ScreenMap &first = segments.begin()->second;
    return fl::make_shared<ScreenMap>(first);
}

fl::shared_ptr<MultiChannelConfig> Fled::channels() const FL_NO_EXCEPT {
    // TODO: construct MultiChannelConfig from envelope["channels"] once
    // the MultiChannelConfig JSON deserializer lands in fl/channels/.
    return fl::shared_ptr<MultiChannelConfig>();
}

}  // namespace fl
