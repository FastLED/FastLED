#pragma once

// Internal PIMPL for fl::Fled. Owns either a heap-backed byte buffer
// (load + loadFromVector) or borrows a static span (loadFromStatic).
// Lives in fl::fled per the subsystem namespace rule (#3311).

#include "fl/stl/int.h"
#include "fl/stl/json.h"
#include "fl/stl/move.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"

namespace fl {
namespace fled {

class FledImpl {
  public:
    enum Ownership { kOwned, kStatic };

    // Owned: takes ownership of an already-loaded byte vector.
    FledImpl(fl::vector<fl::u8> &&owned, fl::u8 versionByte,
             fl::u8 pixelFormatByte, fl::size payloadOffset,
             fl::json envelope) FL_NO_EXCEPT
        : mOwned(fl::move(owned)),
          mStaticPtr(nullptr),
          mStaticLen(0),
          mOwnership(kOwned),
          mVersion(versionByte),
          mPixelFormat(pixelFormatByte),
          mPayloadOffset(payloadOffset),
          mJson(envelope) {}

    // Static: borrows the caller's span (no copy, no free).
    FledImpl(fl::span<const fl::u8> staticBytes, fl::u8 versionByte,
             fl::u8 pixelFormatByte, fl::size payloadOffset,
             fl::json envelope) FL_NO_EXCEPT
        : mOwned(),
          mStaticPtr(staticBytes.data()),
          mStaticLen(staticBytes.size()),
          mOwnership(kStatic),
          mVersion(versionByte),
          mPixelFormat(pixelFormatByte),
          mPayloadOffset(payloadOffset),
          mJson(envelope) {}

    const fl::u8 *bytes() const FL_NO_EXCEPT {
        return mOwnership == kOwned ? mOwned.data() : mStaticPtr;
    }
    fl::size totalLen() const FL_NO_EXCEPT {
        return mOwnership == kOwned ? mOwned.size() : mStaticLen;
    }
    fl::u8 versionByte() const FL_NO_EXCEPT { return mVersion; }
    fl::u8 pixelFormatByte() const FL_NO_EXCEPT { return mPixelFormat; }
    fl::size payloadOffset() const FL_NO_EXCEPT { return mPayloadOffset; }
    fl::size payloadLen() const FL_NO_EXCEPT {
        const fl::size total = totalLen();
        return total > mPayloadOffset ? total - mPayloadOffset : 0;
    }
    const fl::json &envelope() const FL_NO_EXCEPT { return mJson; }

  private:
    fl::vector<fl::u8> mOwned;
    const fl::u8 *mStaticPtr;
    fl::size mStaticLen;
    Ownership mOwnership;
    fl::u8 mVersion;
    fl::u8 mPixelFormat;
    fl::size mPayloadOffset;
    fl::json mJson;
};

}  // namespace fled
}  // namespace fl
