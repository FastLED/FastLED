#pragma once

#ifndef FASTLED_STUB_IMPL
#error "why is this being included?"
#endif

#include <stdint.h>
#include "namespace.h"

// Signal to the engine that all pins are hardware SPI
#define FASTLED_ALL_PINS_HARDWARE_SPI

FASTLED_NAMESPACE_BEGIN

class StubSPIOutput {
public:
    StubSPIOutput() { }
    void select() { }
    void init() {}
    void waitFully() {}
    void release() {}
    void writeByte(uint8_t byte) {}
    void writeWord(uint16_t word) {}
};


FASTLED_NAMESPACE_END

