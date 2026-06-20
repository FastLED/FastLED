#pragma once

// Free functions in fl::fled that parse a .fled v1 byte buffer into its
// header + JSON envelope. The fl::Fled factories in fled.cpp.hpp delegate
// here so the public type stays a thin facade and the parser internals
// can be reused (e.g. by FledBuilder) without dragging in the PIMPL type.
//
// See FLED_FORMAT.md for the on-disk layout.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

class json;  // fl/stl/json.h

namespace fled {

struct ParsedHeader {
    fl::u8   version;
    fl::u8   pixelFormat;
    fl::size payloadOffset;
};

// Parse the 12-byte header + JSON envelope.
// Returns true on success and populates outHeader + outEnvelope.
// Returns false on any of:
//   - len < 12
//   - magic != "FLED"
//   - version != 1
//   - jsonLen > kMaxJsonBytes
//   - jsonLen > len - 12 (truncated)
//   - JSON parse failed
bool parseHeaderAndEnvelope(const fl::u8* data, fl::size len,
                            ParsedHeader* outHeader,
                            fl::json* outEnvelope) FL_NO_EXCEPT;

// Section-name aliases recognized by Fled::blob(). Centralized here so
// FledBuilder and the public accessor agree on the canonical name.
bool sectionNameIsPayload(const char* name) FL_NO_EXCEPT;

} // namespace fled
} // namespace fl
