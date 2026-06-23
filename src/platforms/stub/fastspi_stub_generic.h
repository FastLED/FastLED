#pragma once

// IWYU pragma: private

#ifndef FASTLED_STUB_IMPL
#error "why is this being included?"
#endif

#include "fl/stl/stdint.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/vector.h"
#include "fl/system/engine_events.h"
#include "platforms/shared/active_strip_data/active_strip_data.h"
#include "platforms/shared/active_strip_tracker/active_strip_tracker.h"
#include "fl/stl/noexcept.h"

// Signal to the driver that all pins are hardware SPI
#define FASTLED_ALL_PINS_HARDWARE_SPI

namespace fl {

class CLEDController;

class StubSPIOutput : public fl::EngineEvents::Listener {
public:
    StubSPIOutput() FL_NOEXCEPT;
    ~StubSPIOutput();
    void select() FL_NOEXCEPT;
    void init() FL_NOEXCEPT;
    void waitFully() FL_NOEXCEPT;
    void release() FL_NOEXCEPT;
    void endTransaction() FL_NOEXCEPT { release(); }  // For compatibility with chipsets that use endTransaction
    void writeByte(u8 byte) FL_NOEXCEPT;
    void writeWord(u16 word) FL_NOEXCEPT;
    static void finalizeTransmission() FL_NOEXCEPT { }

    // Access captured SPI transmission bytes for testing
    const fl::vector<u8>& getCapturedBytes() const FL_NOEXCEPT { return mBytes; }

private:
    void onEndShowLeds() FL_NOEXCEPT override;

    ActiveStripTracker mTracker;
    fl::vector<u8> mBytes;  // Captures all raw SPI transmission bytes
};


}  // namespace fl
