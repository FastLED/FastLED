/// @file lcd_rgb_peripheral_esp.h
/// @brief ESP32-P4 LCD RGB peripheral implementation
///
/// This is the real hardware implementation of ILcdRgbPeripheral for ESP32-P4.
/// It wraps ESP-IDF LCD RGB APIs with the minimal necessary abstraction.

#pragma once

// Only compile for ESP32-P4 with RGB LCD support
#if defined(CONFIG_IDF_TARGET_ESP32P4) && __has_include("esp_lcd_panel_rgb.h")

#include "ilcd_rgb_peripheral.h"
#include "esp_lcd_panel_ops.h"

namespace fl {

// Forward declaration for friend access
template <typename T, int N> class Singleton;

namespace detail {

/// @brief ESP32-P4 LCD RGB peripheral implementation
///
/// Thin wrapper around ESP-IDF LCD RGB APIs. This class handles:
/// - LCD RGB panel creation and configuration
/// - DMA buffer allocation (PSRAM or internal)
/// - Frame transfer via esp_lcd_panel_draw_bitmap()
/// - Callback registration for frame completion
class LcdRgbPeripheralEsp : public ILcdRgbPeripheral {
public:
    /// @brief Get singleton instance
    /// @return Reference to singleton
    ///
    /// ESP32-P4 has only one RGB LCD peripheral, so we use singleton pattern.
    static LcdRgbPeripheralEsp& instance();

    ~LcdRgbPeripheralEsp() override;

    //=========================================================================
    // ILcdRgbPeripheral Implementation
    //=========================================================================

    bool initialize(const LcdRgbPeripheralConfig& config) override;
    void deinitialize() override;
    bool isInitialized() const override;

    uint16_t* allocateFrameBuffer(size_t size_bytes) override;
    void freeFrameBuffer(uint16_t* buffer) override;

    bool drawFrame(const uint16_t* buffer, size_t size_bytes) override;
    bool waitFrameDone(uint32_t timeout_ms) override;
    bool isBusy() const override;

    bool registerDrawCallback(void* callback, void* user_ctx) override;
    const LcdRgbPeripheralConfig& getConfig() const override;

    uint64_t getMicroseconds() override;
    void delay(uint32_t ms) override;

private:
    friend class fl::Singleton<LcdRgbPeripheralEsp>;

    LcdRgbPeripheralEsp();

    // Non-copyable
    LcdRgbPeripheralEsp(const LcdRgbPeripheralEsp&) = delete;
    LcdRgbPeripheralEsp& operator=(const LcdRgbPeripheralEsp&) = delete;

    // State
    bool mInitialized;
    LcdRgbPeripheralConfig mConfig;
    esp_lcd_panel_handle_t mPanelHandle;

    // Callback storage
    void* mCallback;
    void* mUserCtx;

    // Transfer state
    volatile bool mBusy;
};

} // namespace detail
} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4 && esp_lcd_panel_rgb.h
