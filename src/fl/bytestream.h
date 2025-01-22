#pragma once

#include <stddef.h>
#include <stdint.h>

#include "fl/namespace.h"
#include "fl/ptr.h"

#include "crgb.h"

namespace fl {

FASTLED_SMART_PTR(ByteStream);

// An abstract class that represents a stream of bytes.
class ByteStream : public fl::Referent {
  public:
    virtual ~ByteStream() {}
    virtual bool available(size_t) const = 0;
    virtual size_t read(uint8_t *dst, size_t bytesToRead) = 0;
    virtual const char *path() const = 0;
    virtual void close() {}  // default is do nothing on close.
    // convenience functions
    size_t readCRGB(CRGB *dst, size_t n) {
      return read((uint8_t *)dst, n * 3) / 3;
    }
};



}  // namespace fl