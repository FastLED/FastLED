#pragma once

#ifndef FASTLED_STUB_IMPL
#error "why is this being included?"
#endif

#include "fl/stl/stdint.h"
#include "fl/unused.h"
#include "fl/stl/vector.h"
#include "fl/engine_events.h"
#include "platforms/shared/active_strip_data/active_strip_data.h"
#include "platforms/shared/active_strip_tracker/active_strip_tracker.h"

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
    void endTransaction() { release(); }  // For compatibility with chipsets that use endTransaction
    void writeByte(uint8_t byte);
    void writeWord(uint16_t word);
    static void finalizeTransmission() { }

    // Access captured SPI transmission bytes for testing
    const fl::vector<uint8_t>& getCapturedBytes() const { return mBytes; }

private:
    void onEndShowLeds() override;

    ActiveStripTracker mTracker;
    fl::vector<uint8_t> mBytes;  // Captures all raw SPI transmission bytes
};


}  // namespace fl
