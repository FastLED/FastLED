#ifndef __INC_FASTSPI_STUB_H
#define __INC_FASTSPI_STUB_H

#include "namespace.h"

// This is a stub implementation of fastspi.

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_STUB_IMPL)

#define FASTLED_ALL_PINS_HARDWARE_SPI

class StubSPIOutput {
public:
    StubSPIOutput() { }
};

#endif // defined(FASTLED_STUB_IMPL)

FASTLED_NAMESPACE_END

#endif // __INC_FASTSPI_STUB_H