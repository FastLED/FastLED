/// @file ii2s_spi_peripheral.h
/// @brief Virtual interface for I2S parallel SPI peripheral hardware abstraction
///
/// This interface enables mock injection for unit testing of the I2S_SPI driver.
/// It abstracts all I2S parallel mode API calls into a clean interface that can be:
/// - Implemented by I2sSpiPeripheralEsp (real hardware on ESP32dev/ESP32-S2)
/// - Implemented by I2sSpiPeripheralMock (unit test simulation)
///
/// The I2S peripheral in parallel mode drives up to 16 data lanes + clock
/// simultaneously for clocked SPI LED chipsets (APA102, SK9822, etc.).

#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

/// @brief Configuration for I2S parallel SPI peripheral
struct I2sSpiConfig {
    fl::vector_fixed<int, 16> data_gpios; ///< Data lane GPIOs (D0-D15)
    int clock_gpio;                        ///< Clock output GPIO (SCK)
    int num_lanes;                         ///< Active data lanes (1-16)
    u32 clock_hz;                          ///< SPI clock frequency in Hz
    size_t max_transfer_bytes;             ///< Maximum bytes per transfer

    I2sSpiConfig() FL_NOEXCEPT
        : data_gpios(),
          clock_gpio(-1),
          num_lanes(0),
          clock_hz(0),
          max_transfer_bytes(0) {
        data_gpios.resize(16);
        for (size_t i = 0; i < 16; i++) {
            data_gpios[i] = -1;
        }
    }

    I2sSpiConfig(int lanes, int clk_gpio, u32 clk_hz, size_t max_bytes) FL_NOEXCEPT
        : data_gpios(),
          clock_gpio(clk_gpio),
          num_lanes(lanes),
          clock_hz(clk_hz),
          max_transfer_bytes(max_bytes) {
        data_gpios.resize(16);
        for (size_t i = 0; i < 16; i++) {
            data_gpios[i] = -1;
        }
    }
};

/// @brief Virtual interface for I2S parallel SPI peripheral
///
/// Abstracts the I2S peripheral in parallel mode for driving clocked SPI
/// LED strips. The peripheral outputs data on up to 16 parallel lanes
/// with a shared clock signal.
class II2sSpiPeripheral {
public:
    virtual ~II2sSpiPeripheral() = default;

    // Lifecycle
    virtual bool initialize(const I2sSpiConfig& config) FL_NOEXCEPT = 0;
    virtual void deinitialize() FL_NOEXCEPT = 0;
    virtual bool isInitialized() const FL_NOEXCEPT = 0;

    // Buffer management (DMA-capable buffers)
    virtual u8* allocateBuffer(size_t size_bytes) FL_NOEXCEPT = 0;
    virtual void freeBuffer(u8* buffer) FL_NOEXCEPT = 0;

    // Transmission
    virtual bool transmit(const u8* buffer, size_t size_bytes) FL_NOEXCEPT = 0;
    virtual bool waitTransmitDone(u32 timeout_ms) FL_NOEXCEPT = 0;
    virtual bool isBusy() const FL_NOEXCEPT = 0;

    // Callback — WARNING: callback is invoked from ISR context and
    // MUST be placed in IRAM (IRAM_ATTR) on ESP32 platforms.
    virtual bool registerTransmitCallback(void* callback,
                                          void* user_ctx) FL_NOEXCEPT = 0;

    // State
    virtual const I2sSpiConfig& getConfig() const FL_NOEXCEPT = 0;

    // Platform utilities
    virtual u64 getMicroseconds() FL_NOEXCEPT = 0;
    virtual void delay(u32 ms) FL_NOEXCEPT = 0;
};

} // namespace detail
} // namespace fl
