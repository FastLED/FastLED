#pragma once

#ifndef __EMSCRIPTEN__
#error "This file should only be included in an Emscripten build"
#endif

#include "fl/vector.h"
#include "fl/stdint.h"
#include "platforms/wasm/engine_listener.h"
#include "fl/namespace.h"

#define FASTLED_ALL_PINS_HARDWARE_SPI


FASTLED_NAMESPACE_BEGIN
class CLEDController;
FASTLED_NAMESPACE_END

namespace fl {


class WasmSpiOutput : public fl::EngineEvents::Listener {
  public:
    WasmSpiOutput();
    ~WasmSpiOutput();
    void select();
    void init();
    void waitFully();
    void release();
    void writeByte(uint8_t byte);
    void writeWord(uint16_t word);

  private:
    CLEDController *tryFindOwner();
    void onEndShowLeds() override;

    int mId = -1; // Deferred initialization
    fl::vector<uint8_t> mRgb;
};

// Compatibility alias
typedef WasmSpiOutput StubSPIOutput;

} // namespace fl
