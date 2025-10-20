#pragma once

/// @file clockless_ezws2812_gpio.h
/// @brief FastLED ezWS2812 GPIO controller with automatic frequency selection
///
/// This controller automatically selects the optimal timing implementation
/// based on the CPU frequency (F_CPU). It includes both 39MHz and 78MHz
/// optimized implementations and chooses the correct one at compile time.

#include "eorder.h"

// Check if we're on the right platform
#if !defined(ARDUINO_ARCH_SILABS)
#error "ezWS2812 GPIO controllers are only available for Silicon Labs MGM240/MG24 platforms"
#endif

// Include frequency-specific implementations
#include "platforms/arm/mgm240/clockless_ezws2812_39mhz.h"
#include "platforms/arm/mgm240/clockless_ezws2812_78mhz.h"
namespace fl {
/// @brief Auto-selecting ezWS2812 GPIO controller
///
/// This controller template automatically selects the optimal implementation
/// based on the CPU frequency defined by F_CPU:
/// - F_CPU >= 78MHz: Uses 78MHz optimized timing
/// - F_CPU < 78MHz: Uses 39MHz optimized timing (default)
///
/// This provides the best performance without requiring manual selection
/// for the current WS2812-specific implementation.
///
/// @todo FUTURE IMPROVEMENT: When the underlying 39MHz/78MHz controllers
/// are made generic with T1/T2/T3 parameters, this auto-selector could also
/// become generic and support all clockless chipsets with optimal timing
/// selection based on CPU frequency.
///
/// @tparam DATA_PIN GPIO pin number for LED data
/// @tparam RGB_ORDER Color channel ordering (typically GRB for WS2812)
template<u8 DATA_PIN, EOrder RGB_ORDER = GRB>
class ClocklessController_ezWS2812_GPIO_Auto : public CPixelLEDController<RGB_ORDER> {
public:
    // Select implementation based on CPU frequency
    #if defined(F_CPU) && (F_CPU >= 78000000)
        // Use 78MHz optimized implementation
        using Implementation = ClocklessController_ezWS2812_GPIO_78MHz<DATA_PIN, RGB_ORDER>;
        static constexpr const char* FrequencyMode = "78MHz";
    #else
        // Use 39MHz optimized implementation (default)
        using Implementation = ClocklessController_ezWS2812_GPIO_39MHz<DATA_PIN, RGB_ORDER>;
        static constexpr const char* FrequencyMode = "39MHz";
    #endif

private:
    Implementation mImpl;

public:
    /// @brief Constructor
    ClocklessController_ezWS2812_GPIO_Auto() = default;

    /// @brief Initialize the controller
    virtual void init() override {
        mImpl.init();
    }

    /// @brief Get maximum refresh rate
    virtual u16 getMaxRefreshRate() const override {
        return mImpl.getMaxRefreshRate();
    }

    /// @brief Get selected frequency mode for debugging
    static const char* getFrequencyMode() { return FrequencyMode; }

protected:
    /// @brief Show pixels (used by FastLED internally)
    virtual void showPixels(PixelController<RGB_ORDER>& pixels) override {
        mImpl.showPixels(pixels);
    }
};

/// @brief Main ezWS2812 GPIO controller typedef with auto-selection
///
/// This is the primary GPIO controller users should use. It automatically
/// selects the optimal timing implementation based on F_CPU.
///
/// Usage:
/// @code
/// FastLED.addLeds<EZWS2812_GPIO, 7, GRB>(leds, NUM_LEDS);
/// @endcode
///
/// @tparam DATA_PIN GPIO pin number for LED data
/// @tparam RGB_ORDER Color channel ordering (typically GRB for WS2812)
template<u8 DATA_PIN, EOrder RGB_ORDER = GRB>
using EZWS2812_GPIO = ClocklessController_ezWS2812_GPIO_Auto<DATA_PIN, RGB_ORDER>;

/// @brief Direct access to frequency-specific controllers
///
/// These can be used directly if you want to explicitly select a frequency
/// implementation regardless of F_CPU setting.

/// @brief 39MHz optimized GPIO controller
template<u8 DATA_PIN, EOrder RGB_ORDER = GRB>
using EZWS2812_GPIO_39MHZ = ClocklessController_ezWS2812_GPIO_39MHz<DATA_PIN, RGB_ORDER>;

/// @brief 78MHz optimized GPIO controller
template<u8 DATA_PIN, EOrder RGB_ORDER = GRB>
using EZWS2812_GPIO_78MHZ = ClocklessController_ezWS2812_GPIO_78MHz<DATA_PIN, RGB_ORDER>;
}  // namespace fl