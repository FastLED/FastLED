#ifndef __INC_FASTSPI_STUB_H
#define __INC_FASTSPI_STUB_H

#include "namespace.h"

// This is a stub implementation of fastspi.

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_STUB_IMPL)

#define FASTLED_ALL_PINS_HARDWARE_SPI

// just a test
// define for wasm
#ifdef __EMSCRIPTEN__
#error
#endif

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

#endif // defined(FASTLED_STUB_IMPL)

FASTLED_NAMESPACE_END

#endif // __INC_FASTSPI_STUB_H