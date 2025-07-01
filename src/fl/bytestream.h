#pragma once

#include "fl/stdint.h"

#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fl/int.h"

#include "crgb.h"

namespace fl {

FASTLED_SMART_PTR(ByteStream);

// An abstract class that represents a stream of bytes.
class ByteStream : public fl::Referent {
  public:
    virtual ~ByteStream() {}
    virtual bool available(fl::sz) const = 0;
    virtual fl::sz read(fl::u8 *dst, fl::sz bytesToRead) = 0;
    virtual const char *path() const = 0;
    virtual void close() {} // default is do nothing on close.
    // convenience functions
    virtual fl::sz readCRGB(CRGB *dst, fl::sz n) {
        return read((fl::u8 *)dst, n * 3) / 3;
    }
};

} // namespace fl
