#ifdef __EMSCRIPTEN__

#include "fastspi_wasm.h"

FASTLED_NAMESPACE_BEGIN

WasmSpiOutput::WasmSpiOutput() { }

void WasmSpiOutput::select() { }

void WasmSpiOutput::init() { }

void WasmSpiOutput::waitFully() { }

void WasmSpiOutput::release() { }

void WasmSpiOutput::writeByte(uint8_t byte) { }

void WasmSpiOutput::writeWord(uint16_t word) { }

FASTLED_NAMESPACE_END


#endif  // __EMSCRIPTEN__
