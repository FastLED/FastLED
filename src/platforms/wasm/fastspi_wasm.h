#pragma once

#ifndef __EMSCRIPTEN__
#error "This file should only be included in an Emscripten build"
#endif

#include <stdint.h>

#include <stdio.h>
#include <vector>

#include "active_strip_data.h"
#include "fl/namespace.h"
#include "fl/singleton.h"
// #include "ui/events.h"
#include "crgb.h"
#include "dither_mode.h"
#include "pixel_controller.h"
#include "platforms/wasm/engine_listener.h"
#include "platforms/wasm/js.h"
#include "platforms/wasm/strip_id_map.h"

namespace fl {

extern uint8_t get_brightness();

#define FASTLED_ALL_PINS_HARDWARE_SPI

class WasmSpiOutput : public fl::EngineEvents::Listener {
  public:
    WasmSpiOutput() ;
    ~WasmSpiOutput() ;
    CLEDController *tryFindOwner();
    void onEndShowLeds() override;

    void select() {}
    void init() {}
    void waitFully() {}
    void release() {}

    void writeByte(uint8_t byte) {}

    void writeWord(uint16_t word) {
        writeByte(word >> 8);
        writeByte(word & 0xFF);
    }

  private:
    int mId = -1; // Deferred initialization
    std::vector<uint8_t> mRgb;
};

// Compatibility alias
typedef WasmSpiOutput StubSPIOutput;

} // namespace fl
