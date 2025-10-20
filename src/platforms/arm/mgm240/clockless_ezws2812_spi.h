#pragma once

/// @file clockless_ezws2812_spi.h
/// @brief FastLED ezWS2812 SPI controller for Silicon Labs MGM240/MG24
///
/// This controller provides hardware-accelerated WS2812 control using the SPI peripheral.
/// It converts WS2812 bits to SPI bit patterns for precise timing generation.
///
/// IMPORTANT: This controller consumes a hardware SPI peripheral and must be
/// explicitly enabled with #define FASTLED_USES_EZWS2812_SPI

#include "controller.h"
#include "pixel_controller.h"
#include "eorder.h"
#include "fl/force_inline.h"

// Check if we're on the right platform
#if !defined(ARDUINO_ARCH_SILABS)
#error "ezWS2812 SPI controller is only available for Silicon Labs MGM240/MG24 platforms"
#endif

// Only include SPI controller if explicitly requested
#ifdef FASTLED_USES_EZWS2812_SPI
#include "third_party/ezws2812/ezWS2812.h"
#endif
namespace fl {
#ifdef FASTLED_USES_EZWS2812_SPI

/// @brief ezWS2812 SPI controller for FastLED
///
/// This controller uses the Silicon Labs ezWS2812 SPI driver to provide
/// hardware-accelerated WS2812 control. It consumes one SPI peripheral.
///
/// Each WS2812 bit is encoded as 8 SPI bits at 3.2MHz:
/// - '1' bit: 0xFC (11111100) - long high pulse
/// - '0' bit: 0x80 (10000000) - short high pulse
///
/// IMPORTANT: Must define FASTLED_USES_EZWS2812_SPI before including FastLED.h
///
/// @tparam RGB_ORDER Color channel ordering (typically GRB for WS2812)
template<EOrder RGB_ORDER = GRB>
class ClocklessController_ezWS2812_SPI : public CPixelLEDController<RGB_ORDER> {
private:
    fl::third_party::ezWS2812* mDriver;
    u16 mNumLeds;
    bool mOwnsDriver;

    /// @brief Convert WS2812 bit to SPI signal for '1' bit
    /// @return SPI byte pattern for logical '1'
    FASTLED_FORCE_INLINE u8 spi_one() const {
        return 0xFC; // 11111100 - long high pulse for '1'
    }

    /// @brief Convert WS2812 bit to SPI signal for '0' bit
    /// @return SPI byte pattern for logical '0'
    FASTLED_FORCE_INLINE u8 spi_zero() const {
        return 0x80; // 10000000 - short high pulse for '0'
    }

    /// @brief Convert 8-bit color value to SPI bit pattern array
    /// @param color 8-bit color channel value
    /// @param buffer Output buffer for SPI bytes (8 bytes)
    FASTLED_FORCE_INLINE void colorToSPI(u8 color, u8* buffer) const {
        // Convert each bit to SPI pattern (MSB first)
        buffer[0] = (color & 0x80) ? spi_one() : spi_zero(); // bit 7
        buffer[1] = (color & 0x40) ? spi_one() : spi_zero(); // bit 6
        buffer[2] = (color & 0x20) ? spi_one() : spi_zero(); // bit 5
        buffer[3] = (color & 0x10) ? spi_one() : spi_zero(); // bit 4
        buffer[4] = (color & 0x08) ? spi_one() : spi_zero(); // bit 3
        buffer[5] = (color & 0x04) ? spi_one() : spi_zero(); // bit 2
        buffer[6] = (color & 0x02) ? spi_one() : spi_zero(); // bit 1
        buffer[7] = (color & 0x01) ? spi_one() : spi_zero(); // bit 0
    }

public:
    /// @brief Constructor
    ClocklessController_ezWS2812_SPI() : mDriver(nullptr), mNumLeds(0), mOwnsDriver(false) {}

    /// @brief Destructor
    ~ClocklessController_ezWS2812_SPI() {
        if (mOwnsDriver && mDriver) {
            mDriver->end();
            delete mDriver;
        }
    }

    /// @brief Initialize the controller
    virtual void init() override {
        // Driver will be created when showPixels is first called
    }

    /// @brief Get maximum refresh rate
    virtual u16 getMaxRefreshRate() const override {
        return 1000; // SPI allows high refresh rates
    }

protected:
    /// @brief Output pixels to LED strip using SPI acceleration
    /// @param pixels FastLED pixel controller with RGB data
    virtual void showPixels(PixelController<RGB_ORDER>& pixels) override {
        // Create driver on first use
        if (!mDriver) {
            mNumLeds = pixels.size();
            mDriver = new fl::third_party::ezWS2812(mNumLeds);
            mOwnsDriver = true;
            mDriver->begin();
        }

        // Pre-allocate SPI buffer for one pixel (3 colors × 8 bits = 24 SPI bytes)
        u8 spi_buffer[24];

        // Process all pixels directly via SPI
        while (pixels.has(1)) {
            u8 r = pixels.loadAndScale0();
            u8 g = pixels.loadAndScale1();
            u8 b = pixels.loadAndScale2();

            // Convert to SPI bit patterns (GRB order for WS2812)
            colorToSPI(g, &spi_buffer[0]);  // Green first (8 SPI bytes)
            colorToSPI(r, &spi_buffer[8]);  // Red second (8 SPI bytes)
            colorToSPI(b, &spi_buffer[16]); // Blue third (8 SPI bytes)

            // Send pixel data via SPI
            for (int j = 0; j < 24; j++) {
                SPI.transfer(spi_buffer[j]);
            }

            pixels.advanceData();
            pixels.stepDithering();
        }

        // Complete transfer with reset pulse
        mDriver->end_transfer();
    }
};

/// @brief Convenient typedef for ezWS2812 SPI controller
/// @tparam RGB_ORDER Color channel ordering (typically GRB for WS2812)
template<EOrder RGB_ORDER = GRB>
using EZWS2812_SPI = ClocklessController_ezWS2812_SPI<RGB_ORDER>;

#endif // FASTLED_USES_EZWS2812_SPI
}  // namespace fl