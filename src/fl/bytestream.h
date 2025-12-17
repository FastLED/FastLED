#pragma once

#include "fl/stl/stdint.h"

#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/int.h"

#include "crgb.h"

namespace fl {

FASTLED_SHARED_PTR(ByteStream);

// An abstract class that represents a stream of bytes.
class ByteStream {
  public:
    virtual ~ByteStream() {}
    virtual bool available(fl::size) const = 0;
    virtual fl::size read(fl::u8 *dst, fl::size bytesToRead) = 0;
    virtual const char *path() const = 0;
    virtual void close() {} // default is do nothing on close.
    // convenience functions
    virtual fl::size readCRGB(CRGB *dst, fl::size n) {
        return read((fl::u8 *)dst, n * 3) / 3;
    }
};

} // namespace fl
