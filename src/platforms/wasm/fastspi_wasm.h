#pragma once

#ifndef __EMSCRIPTEN__
#error "This file should only be included in an Emscripten build"
#endif

// WASM now uses the stub platform SPI implementation with LED capture
#include "platforms/stub/fastspi_stub_generic.h"

namespace fl {

// Compatibility alias - WASM uses stub SPI implementation
typedef StubSPIOutput WasmSpiOutput;

} // namespace fl
