/// @file i2s_lcd_cam_peripheral_esp.h
/// @brief ESP32-S3 I2S LCD_CAM peripheral implementation
///
/// This is the real hardware implementation of II2sLcdCamPeripheral for ESP32-S3.
/// It wraps ESP-IDF LCD I80 bus APIs with the minimal necessary abstraction.

#pragma once

// Only compile for ESP32-S3 with LCD I80 support
#if defined(CONFIG_IDF_TARGET_ESP32S3) && __has_include("esp_lcd_panel_io.h")

#include "ii2s_lcd_cam_peripheral.h"
#include "esp_lcd_panel_io.h"

namespace fl {
// Forward declaration for friend
template<typename T, int N>
class Singleton;
namespace detail {

/// @brief ESP32-S3 I2S LCD_CAM peripheral implementation
///
/// Thin wrapper around ESP-IDF LCD I80 bus APIs. This class handles:
/// - LCD I80 bus creation and configuration
/// - DMA buffer allocation (PSRAM or internal)
/// - Frame transfer via tx_color()
/// - Callback registration for transfer completion
class I2sLcdCamPeripheralEsp : public II2sLcdCamPeripheral {
public:
    /// @brief Get singleton instance
    /// @return Reference to singleton
    ///
    /// ESP32-S3 has only one LCD_CAM peripheral, so we use singleton pattern.
    static I2sLcdCamPeripheralEsp& instance();

    ~I2sLcdCamPeripheralEsp() override;

    //=========================================================================
    // II2sLcdCamPeripheral Implementation
    //=========================================================================

    bool initialize(const I2sLcdCamConfig& config) override;
    void deinitialize() override;
    bool isInitialized() const override;

    uint16_t* allocateBuffer(size_t size_bytes) override;
    void freeBuffer(uint16_t* buffer) override;

    bool transmit(const uint16_t* buffer, size_t size_bytes) override;
    bool waitTransmitDone(uint32_t timeout_ms) override;
    bool isBusy() const override;

    bool registerTransmitCallback(void* callback, void* user_ctx) override;
    const I2sLcdCamConfig& getConfig() const override;

    uint64_t getMicroseconds() override;
    void delay(uint32_t ms) override;

private:
    // Allow Singleton to call private constructor
    template<typename T, int N>
    friend class ::fl::Singleton;

    // Allow ISR callback to access members
    friend bool i2s_lcd_cam_flush_ready(
        esp_lcd_panel_io_handle_t panel_io,
        esp_lcd_panel_io_event_data_t* edata,
        void* user_ctx);

    I2sLcdCamPeripheralEsp();

    // Non-copyable
    I2sLcdCamPeripheralEsp(const I2sLcdCamPeripheralEsp&) = delete;
    I2sLcdCamPeripheralEsp& operator=(const I2sLcdCamPeripheralEsp&) = delete;

    // State
    bool mInitialized;
    I2sLcdCamConfig mConfig;
    esp_lcd_i80_bus_handle_t mI80Bus;
    esp_lcd_panel_io_handle_t mPanelIo;

    // Callback storage
    void* mCallback;
    void* mUserCtx;

    // Transfer state
    volatile bool mBusy;
};

} // namespace detail
} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32S3 && esp_lcd_panel_io.h
