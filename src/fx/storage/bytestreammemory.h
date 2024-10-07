#pragma once

#include "namespace.h"
#include "ptr.h"
#include <stddef.h>
#include <stdint.h>
#include "bytestream.h"

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(ByteStreamMemory);

class ByteStreamMemory : public ByteStream {
  public:
    ByteStreamMemory(uint32_t size_buffer);
    ~ByteStreamMemory() override;
    bool available() const override;
    size_t size() const override;
    size_t read(uint8_t *dst, size_t bytesToRead) override;
    const char *path() const override { return "ByteStreamMemory"; }
};

FASTLED_NAMESPACE_END
