#pragma once

#ifndef FASTLED_STUB_IMPL
#error "why is this being included?"
#endif

#include "fl/stdint.h"
#include "fl/namespace.h"
#include "fl/unused.h"

// Signal to the engine that all pins are hardware SPI
#define FASTLED_ALL_PINS_HARDWARE_SPI

namespace fl {

class StubSPIOutput {
public:
    StubSPIOutput() { }
    void select() { }
    void init() {}
    void waitFully() {}
    void release() {}
    void writeByte(uint8_t byte) { FASTLED_UNUSED(byte); }
    void writeWord(uint16_t word) { FASTLED_UNUSED(word); }
};


}  // namespace fl
