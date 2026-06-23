/// @file i2s_spi_peripheral_esp.h
/// @brief ESP32dev native I2S parallel SPI peripheral implementation
///
/// Uses direct I2S register manipulation for driving clocked SPI LED strips
/// (APA102, SK9822) via I2S parallel mode on ESP32 and ESP32-S2.
/// No dependency on Yves I2SClockBasedLedDriver.

#pragma once

// IWYU pragma: private

#include "platforms/esp/is_esp.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_I2S

#include "platforms/esp/32/drivers/i2s_spi/ii2s_spi_peripheral.h"
#include "fl/stl/noexcept.h"

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "soc/i2s_struct.h"
#include "rom/lldesc.h"
// IWYU pragma: end_keep
#include "esp_intr_alloc.h"
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "freertos/semphr.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {

template <typename T, int N>
class Singleton;

namespace detail {

/// @brief ESP32 native I2S parallel SPI peripheral
///
/// Drives up to 16 parallel SPI data lanes + clock via I2S0 in LCD/parallel
/// mode. Uses DMA for non-blocking transfer with ISR completion signaling.
class I2sSpiPeripheralEsp : public II2sSpiPeripheral {
  public:
    static I2sSpiPeripheralEsp &instance() FL_NOEXCEPT;

    ~I2sSpiPeripheralEsp() override;

    bool initialize(const I2sSpiConfig &config) FL_NOEXCEPT override;
    void deinitialize() FL_NOEXCEPT override;
    bool isInitialized() const FL_NOEXCEPT override;

    u8 *allocateBuffer(size_t size_bytes) FL_NOEXCEPT override;
    void freeBuffer(u8 *buffer) FL_NOEXCEPT override;

    bool transmit(const u8 *buffer, size_t size_bytes) FL_NOEXCEPT override;
    bool waitTransmitDone(u32 timeout_ms) FL_NOEXCEPT override;
    bool isBusy() const FL_NOEXCEPT override;

    bool registerTransmitCallback(void *callback,
                                  void *user_ctx) FL_NOEXCEPT override;
    const I2sSpiConfig &getConfig() const FL_NOEXCEPT override;

    u64 getMicroseconds() FL_NOEXCEPT override;
    void delay(u32 ms) FL_NOEXCEPT override;

  private:
    template <typename T, int N>
    friend class ::fl::Singleton;

    I2sSpiPeripheralEsp() FL_NOEXCEPT;

    I2sSpiPeripheralEsp(const I2sSpiPeripheralEsp &) = delete;
    I2sSpiPeripheralEsp &operator=(const I2sSpiPeripheralEsp &) = delete;

    void resetI2s() FL_NOEXCEPT;
    void resetDma() FL_NOEXCEPT;
    void resetFifo() FL_NOEXCEPT;
    void transposeToI2sBuffer(const u8 *src, size_t srcSize) FL_NOEXCEPT;

    // ISR callback (must be friend for IRAM_ATTR access)
    static void IRAM_ATTR isrHandler(void *arg);

    // I2S hardware
    i2s_dev_t *mI2s;
    lldesc_t *mDmaDescs;     // DMA descriptor chain (dynamically allocated)
    int mDmaDescCount;       // Number of descriptors in chain
    u16 *mDmaBuffer;         // DMA buffer (16-bit words for parallel output)
    size_t mDmaBufferWords;  // Size in u16 words
    intr_handle_t mIntrHandle;
    SemaphoreHandle_t mCompleteSem;

    // State
    bool mInitialized;
    I2sSpiConfig mConfig;
    void *mCallback;
    void *mUserCtx;
    volatile bool mBusy;
};

} // namespace detail
} // namespace fl

#endif // FASTLED_ESP32_HAS_I2S
