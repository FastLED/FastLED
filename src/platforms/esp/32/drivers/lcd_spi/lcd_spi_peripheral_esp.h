/// @file lcd_spi_peripheral_esp.h
/// @brief ESP32-S3 LCD_CAM SPI peripheral implementation
///
/// Wraps ESP-IDF LCD I80 bus APIs for driving clocked SPI LED strips
/// (APA102, SK9822) via LCD_CAM peripheral on ESP32-S3.

#pragma once

// IWYU pragma: private

#include "platforms/esp/is_esp.h"
#include "fl/stl/has_include.h"

#if defined(FL_IS_ESP_32S3) && FL_HAS_INCLUDE("esp_lcd_panel_io.h")

#include "platforms/esp/32/drivers/lcd_spi/ilcd_spi_peripheral.h"
#include "fl/stl/compiler_control.h"  // FL_IRAM, FL_EXTERN_C_BEGIN
#include "fl/stl/noexcept.h"
#include "esp_lcd_panel_io.h"

FL_EXTERN_C_BEGIN
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

/// @brief ESP32-S3 LCD_CAM SPI peripheral implementation
///
/// Thin wrapper around ESP-IDF LCD I80 bus APIs configured for clocked SPI
/// output. PCLK serves as the SPI clock, D0-D15 as parallel data lanes.
class LcdSpiPeripheralEsp : public ILcdSpiPeripheral {
  public:
    static LcdSpiPeripheralEsp &instance() FL_NOEXCEPT;

    ~LcdSpiPeripheralEsp() override;

    bool initialize(const LcdSpiConfig &config) FL_NOEXCEPT override;
    void deinitialize() FL_NOEXCEPT override;
    bool isInitialized() const FL_NOEXCEPT override;

    u16 *allocateBuffer(size_t size_bytes) FL_NOEXCEPT override;
    void freeBuffer(u16 *buffer) FL_NOEXCEPT override;

    // FL_IRAM: ChannelDriverLcdClockless::isrChunkDone re-arms the next
    // chunk from ISR context, so transmit() must be IRAM-resident to survive
    // a flash-cache stall (NVS commit, SPI flash erase, etc.).
    bool FL_IRAM transmit(const u16 *buffer, size_t size_bytes) FL_NOEXCEPT override;
    bool FL_IRAM queueTransmit(const u16 *buffer, size_t size_bytes) FL_NOEXCEPT override;
    bool waitTransmitDone(u32 timeout_ms) FL_NOEXCEPT override;
    bool isBusy() const FL_NOEXCEPT override;

    bool registerTransmitCallback(void *callback,
                                  void *user_ctx) FL_NOEXCEPT override;
    const LcdSpiConfig &getConfig() const FL_NOEXCEPT override;

    u64 getMicroseconds() FL_NOEXCEPT override;
    void delay(u32 ms) FL_NOEXCEPT override;

    /// @brief Currently-recorded owning driver (issue #2270).
    /// Exposed for diagnostics and tests — production callers should set
    /// `LcdSpiConfig::owner` and let initialize() manage the transition.
    LcdSpiOwnerDriver getOwner() const FL_NOEXCEPT { return mOwner; }

  private:
    template <typename T, int N>
    friend class ::fl::Singleton;

    friend bool lcd_spi_flush_ready( // ok no noexcept
        esp_lcd_panel_io_handle_t panel_io,
        esp_lcd_panel_io_event_data_t *edata, void *user_ctx);

    LcdSpiPeripheralEsp() FL_NOEXCEPT;

    LcdSpiPeripheralEsp(const LcdSpiPeripheralEsp &) = delete;
    LcdSpiPeripheralEsp &operator=(const LcdSpiPeripheralEsp &) = delete;

    /// @brief Tear down every piece of peripheral state held by this
    /// singleton. Called from deinitialize() and from initialize() when
    /// the owning driver changes (issue #2270).
    void teardownLocked() FL_NOEXCEPT;
    bool FL_IRAM transmitInternal(const u16 *buffer, size_t size_bytes,
                                  bool wait_for_slot) FL_NOEXCEPT;

    bool mInitialized;
    LcdSpiConfig mConfig;
    esp_lcd_i80_bus_handle_t mI80Bus;
    esp_lcd_panel_io_handle_t mPanelIo;
    SemaphoreHandle_t mCompleteSem;
    void *mCallback;
    void *mUserCtx;
    volatile bool mBusy;
    volatile size_t mPendingTransmits;
    size_t mLastTransmitSize;
    LcdSpiOwnerDriver mOwner; ///< Tracks which driver owns the singleton (#2270).

    // Latched errors from the FL_IRAM transmit path. transmit() may be called
    // from ISR context (see ChannelDriverLcdClockless::isrChunkDone), so direct
    // FL_WARN there is unsafe (LoggingInIramChecker forbids it). waitTransmitDone()
    // drains and reports these in task context.
    volatile bool mLastWaitForSlotTimeout;
    volatile esp_err_t mLastPanelIoRecreateError;
    volatile esp_err_t mLastTxColorError;
};

} // namespace detail
} // namespace fl

#endif // FL_IS_ESP_32S3 && esp_lcd_panel_io.h
