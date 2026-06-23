#pragma once

// fl::fled::FledBuilder - imperative builder that assembles a v1 .fled
// byte buffer from header bytes + optional JSON sections + an optional
// frame payload, then parses the result through the canonical
// fl::Fled::loadFromVector() path. The returned Fled is byte-equivalent
// to one loaded from disk.
//
// Lives in fl::fled (not fl::) per the subsystem rule: only the public
// Fled type itself stays at top-level fl::; every auxiliary moves to
// the fl::fled namespace.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/span.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"

namespace fl {

class Fled;  // fl/fled/fled.h

namespace fled {

class FledBuilder {
  public:
    // Header is obligatory. version defaults to 1 (the only v1 we ship);
    // pixel format defaults to 0 (rgb8).
    FledBuilder() FL_NO_EXCEPT;

    FledBuilder& setVersion(fl::u8 v) FL_NO_EXCEPT;
    FledBuilder& setPixelFormat(fl::u8 fmt) FL_NO_EXCEPT;

    // Optional JSON sections. Each is merged into the envelope under
    // its canonical top-level key ("map", "channels"). Pass a JSON
    // object string (not a primitive); the builder copies the bytes.
    FledBuilder& setScreenMapJson(const char* json) FL_NO_EXCEPT;
    FledBuilder& setChannelsJson(const char* json) FL_NO_EXCEPT;

    // Optional frame payload (raw bytes appended after the envelope).
    // The builder copies the bytes.
    FledBuilder& setPayload(fl::span<const fl::u8> bytes) FL_NO_EXCEPT;

    // Assemble: serializes the envelope, prepends the 12-byte header,
    // appends the payload, then parses the whole thing through the
    // canonical Fled::loadFromVector() path so the returned Fled is
    // indistinguishable from one loaded from disk.
    fl::Fled build() const FL_NO_EXCEPT;

  private:
    fl::u8             mVersion;
    fl::u8             mPixelFormat;
    fl::string         mScreenMapJson;
    fl::string         mChannelsJson;
    fl::vector<fl::u8> mPayload;
};

} // namespace fled
} // namespace fl
