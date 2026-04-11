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
#include "fl/stl/noexcept.h"
#include "fl/stl/compiler_control.h"
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

    bool transmit(const u16 *buffer, size_t size_bytes) FL_NOEXCEPT override;
    bool waitTransmitDone(u32 timeout_ms) FL_NOEXCEPT override;
    bool isBusy() const FL_NOEXCEPT override;

    bool registerTransmitCallback(void *callback,
                                  void *user_ctx) FL_NOEXCEPT override;
    const LcdSpiConfig &getConfig() const FL_NOEXCEPT override;

    u64 getMicroseconds() FL_NOEXCEPT override;
    void delay(u32 ms) FL_NOEXCEPT override;

  private:
    template <typename T, int N>
    friend class ::fl::Singleton;

    friend bool lcd_spi_flush_ready( // ok no noexcept
        esp_lcd_panel_io_handle_t panel_io,
        esp_lcd_panel_io_event_data_t *edata, void *user_ctx);

    LcdSpiPeripheralEsp() FL_NOEXCEPT;

    LcdSpiPeripheralEsp(const LcdSpiPeripheralEsp &) = delete;
    LcdSpiPeripheralEsp &operator=(const LcdSpiPeripheralEsp &) = delete;

    bool mInitialized;
    LcdSpiConfig mConfig;
    esp_lcd_i80_bus_handle_t mI80Bus;
    esp_lcd_panel_io_handle_t mPanelIo;
    SemaphoreHandle_t mCompleteSem;
    void *mCallback;
    void *mUserCtx;
    volatile bool mBusy;
    size_t mLastTransmitSize;
};

} // namespace detail
} // namespace fl

#endif // FL_IS_ESP_32S3 && esp_lcd_panel_io.h
