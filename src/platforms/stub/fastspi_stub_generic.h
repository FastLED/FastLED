#pragma once

#ifndef FASTLED_STUB_IMPL
#error "why is this being included?"
#endif

#include "fl/stdint.h"
#include "fl/unused.h"
#include "fl/vector.h"
#include "fl/engine_events.h"
#include "platforms/shared/active_strip_data/active_strip_data.h"
#include "platforms/shared/strip_id_map/strip_id_map.h"

// Signal to the engine that all pins are hardware SPI
#define FASTLED_ALL_PINS_HARDWARE_SPI

namespace fl {

class CLEDController;

class StubSPIOutput : public fl::EngineEvents::Listener {
public:
    StubSPIOutput();
    ~StubSPIOutput();
    void select();
    void init();
    void waitFully();
    void release();
    void writeByte(uint8_t byte);
    void writeWord(uint16_t word);
    static void finalizeTransmission() { }

private:
    CLEDController *tryFindOwner();
    void onEndShowLeds() override;

    int mId = -1; // Deferred initialization
    fl::vector<uint8_t> mRgb;
};


}  // namespace fl
