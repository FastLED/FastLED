/// @file ilcd_spi_peripheral.h
/// @brief Virtual interface for LCD_CAM SPI peripheral hardware abstraction
///
/// Abstracts the ESP32-S3 LCD_CAM I80 bus for driving clocked SPI LED strips.
/// The peripheral outputs data on up to 16 parallel lanes with a shared PCLK
/// clock signal routed to the LED strip clock input.

#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

/// @brief Identifies which channel driver currently owns the shared
///        LCD_CAM I80 peripheral singleton.
///
/// The ESP32-S3 LCD_CAM peripheral is a shared hardware resource that
/// can be driven by either the clocked-SPI driver (APA102/SK9822) or the
/// clockless driver (WS2812/SK6812) but not both simultaneously. When the
/// active driver changes, the peripheral MUST be fully torn down — even
/// if the lane count / clock / transfer size happen to match — because
/// the ISR callback, ring-buffer ownership, and completion semaphore all
/// belong to the previous driver. See issue #2270.
enum class LcdSpiOwnerDriver : u8 {
    NONE = 0,         ///< No driver has initialized the peripheral yet.
    LCD_SPI = 1,      ///< Clocked-SPI driver (channel_driver_lcd_spi).
    LCD_CLOCKLESS = 2 ///< Clockless driver (channel_driver_lcd_clockless).
};

/// @brief Configuration for LCD_CAM SPI peripheral
struct LcdSpiConfig {
    fl::vector_fixed<int, 16> data_gpios; ///< Data lane GPIOs (D0-D15)
    int clock_gpio;                        ///< Clock output GPIO (PCLK)
    int dc_gpio;                           ///< Data/Command GPIO (required by I80 bus)
    int num_lanes;                         ///< Active data lanes (1-16)
    u32 clock_hz;                          ///< SPI clock frequency in Hz
    size_t max_transfer_bytes;             ///< Maximum bytes per transfer
    bool use_psram;                        ///< Allocate buffers in PSRAM
    LcdSpiOwnerDriver owner;               ///< Which driver is initializing (issue #2270)

    LcdSpiConfig() FL_NOEXCEPT
        : data_gpios(),
          clock_gpio(-1),
          dc_gpio(-1),
          num_lanes(0),
          clock_hz(0),
          max_transfer_bytes(0),
          use_psram(true),
          owner(LcdSpiOwnerDriver::NONE) {
        data_gpios.resize(16);
        for (size_t i = 0; i < 16; i++) {
            data_gpios[i] = -1;
        }
    }

    LcdSpiConfig(int lanes, int clk_gpio, u32 clk_hz,
                 size_t max_bytes) FL_NOEXCEPT
        : data_gpios(),
          clock_gpio(clk_gpio),
          dc_gpio(-1),
          num_lanes(lanes),
          clock_hz(clk_hz),
          max_transfer_bytes(max_bytes),
          use_psram(true),
          owner(LcdSpiOwnerDriver::NONE) {
        data_gpios.resize(16);
        for (size_t i = 0; i < 16; i++) {
            data_gpios[i] = -1;
        }
    }
};

/// @brief Virtual interface for LCD_CAM SPI peripheral
///
/// Abstracts the LCD_CAM peripheral in I80 mode for driving clocked SPI
/// LED strips. Uses 16-bit words (one word per clock cycle, each bit goes
/// to a different data lane).
class ILcdSpiPeripheral {
  public:
    virtual ~ILcdSpiPeripheral() = default;

    // Lifecycle
    virtual bool initialize(const LcdSpiConfig &config) FL_NOEXCEPT = 0;
    virtual void deinitialize() FL_NOEXCEPT = 0;
    virtual bool isInitialized() const FL_NOEXCEPT = 0;

    // Buffer management (DMA-capable, 16-bit aligned)
    virtual u16 *allocateBuffer(size_t size_bytes) FL_NOEXCEPT = 0;
    virtual void freeBuffer(u16 *buffer) FL_NOEXCEPT = 0;

    // Transmission
    virtual bool transmit(const u16 *buffer,
                          size_t size_bytes) FL_NOEXCEPT = 0;
    virtual bool waitTransmitDone(u32 timeout_ms) FL_NOEXCEPT = 0;
    virtual bool isBusy() const FL_NOEXCEPT = 0;

    // Callback — WARNING: callback is invoked from ISR context and
    // MUST be placed in IRAM (IRAM_ATTR) on ESP32 platforms.
    virtual bool registerTransmitCallback(void *callback,
                                          void *user_ctx) FL_NOEXCEPT = 0;

    // State
    virtual const LcdSpiConfig &getConfig() const FL_NOEXCEPT = 0;

    // Platform utilities
    virtual u64 getMicroseconds() FL_NOEXCEPT = 0;
    virtual void delay(u32 ms) FL_NOEXCEPT = 0;
};

} // namespace detail
} // namespace fl
