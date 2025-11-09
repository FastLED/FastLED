/// @file clockless_parlio_esp32p4.h
/// @brief ESP32-P4 PARLIO channel adapter - individual LED strip interface
///
/// This file provides the ParlioChannel adapter that bridges individual LED strips
/// to the PARLIO parallel I/O system. Each strip becomes a channel in the architecture.
///
/// Architecture:
/// - ParlioChannel: Individual strip adapter (N instances)
/// - IParlioGroup: Broadcasts to K channels with same timing
/// - ParlioHub: Coordinates all groups
/// - ParlioEngine: DMA hardware controller
///
/// Key features:
/// - Seamless integration with FastLED API
/// - Hardware-accelerated parallel output
/// - No distinction between clockless/clocked - everything is a channel

#pragma once

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This file is only for ESP32-P4"
#endif

#include "sdkconfig.h"

#include "fl/stdint.h"

#include "crgb.h"
#include "cpixel_ledcontroller.h"
#include "eorder.h"
#include "pixel_iterator.h"
#include "platforms/shared/clockless_timing.h"
// Include the PARLIO driver
#include "../bulk/parlio_channel.h"
#include "fl/chipsets/chipset_timing_config.h"

namespace fl {

// ===== Concrete Driver Layer (NO TEMPLATES) =====

/// @brief PARLIO channel driver - handles runtime configuration for a single LED strip
///
/// This class is completely non-templated and uses runtime ChipsetTimingConfig.
/// All template parameters from entry points are converted to runtime values
/// before reaching this class. Each instance represents one channel in the system.
class ParlioChannelDriver {
public:
    /// @brief Constructor with runtime pin and timing
    /// @param pin GPIO pin number (runtime value)
    /// @param timing Chipset timing configuration (runtime value)
    explicit ParlioChannelDriver(int pin, const ChipsetTimingConfig& timing);

    void init();
    void beginShowLeds(int nleds);
    void showPixels(PixelIterator& pixel_iterator);
    void endShowLeds();

private:
    int mPin;
    ChipsetTimingConfig mTiming;
};

// ===== Templated Entry Points (convert compile-time types to runtime) =====

/// @brief PARLIO channel adapter - FastLED's interface to a single LED strip
///
/// This is the main entry point that FastLED users interact with via addLeds<>().
/// Each instance represents one channel in the parallel I/O architecture.
///
/// Template parameters (compile-time):
/// - DATA_PIN: GPIO pin number
/// - CHIPSET: Chipset timing trait (e.g., TIMING_WS2812_800KHZ)
/// - RGB_ORDER: Color channel ordering (RGB, GRB, etc.)
///
/// These are converted to runtime values and passed to the ParlioGroup,
/// which broadcasts to all channels with matching timing.
///
/// @tparam DATA_PIN GPIO pin number (compile-time)
/// @tparam CHIPSET Chipset timing trait (e.g., TIMING_WS2812_800KHZ)
/// @tparam RGB_ORDER Color channel ordering (RGB, GRB, etc.)
template <int DATA_PIN, typename CHIPSET, EOrder RGB_ORDER = RGB>
class ClocklessController_Parlio_Esp32P4
    : public CPixelLEDController<RGB_ORDER> {
private:
    typedef CPixelLEDController<RGB_ORDER> Base;
    ParlioChannelDriver mDriver;  // Concrete driver instance

public:
    /// @brief Constructor - converts all template parameters to runtime values
    ClocklessController_Parlio_Esp32P4()
        : mDriver(DATA_PIN, makeTimingConfig<CHIPSET>())
    {
        // Template parameters converted to runtime values:
        // - DATA_PIN (compile-time int) → constructor argument (runtime int)
        // - CHIPSET (compile-time type) → makeTimingConfig<CHIPSET>() (runtime ChipsetTimingConfig)
        // - RGB_ORDER handled by CPixelLEDController<RGB_ORDER> base class
    }

    void init() override {
        mDriver.init();
    }

    uint16_t getMaxRefreshRate() const override { return 800; }

protected:
    void *beginShowLeds(int nleds) override {
        void *data = Base::beginShowLeds(nleds);
        mDriver.beginShowLeds(nleds);
        return data;
    }

    void showPixels(PixelController<RGB_ORDER> &pixels) override {
        auto pixel_iterator = pixels.as_iterator(this->getRgbw());
        mDriver.showPixels(pixel_iterator);
    }

    void endShowLeds(void *data) override {
        Base::endShowLeds(data);
        mDriver.endShowLeds();
    }
};

}  // namespace fl
