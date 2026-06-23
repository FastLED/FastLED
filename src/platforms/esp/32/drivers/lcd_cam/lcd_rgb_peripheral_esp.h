/// @file lcd_rgb_peripheral_esp.h
/// @brief ESP32-P4 LCD RGB peripheral implementation
///
/// This is the real hardware implementation of ILcdRgbPeripheral for ESP32-P4.
/// It wraps ESP-IDF LCD RGB APIs with the minimal necessary abstraction.

#pragma once

// IWYU pragma: private

#include "platforms/esp/is_esp.h"
#include "fl/stl/has_include.h"

// Only compile for ESP32-P4 with RGB LCD support
#if defined(FL_IS_ESP_32P4) && FL_HAS_INCLUDE("esp_lcd_panel_rgb.h")

#include "platforms/esp/32/drivers/lcd_cam/ilcd_rgb_peripheral.h"
#include "esp_lcd_panel_ops.h"
#include "fl/stl/noexcept.h"

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
    static LcdRgbPeripheralEsp& instance() FL_NOEXCEPT;

    ~LcdRgbPeripheralEsp() override;

    //=========================================================================
    // ILcdRgbPeripheral Implementation
    //=========================================================================

    bool initialize(const LcdRgbPeripheralConfig& config) FL_NOEXCEPT override;
    void deinitialize() FL_NOEXCEPT override;
    bool isInitialized() const FL_NOEXCEPT override;

    u16* allocateFrameBuffer(size_t size_bytes) FL_NOEXCEPT override;
    void freeFrameBuffer(u16* buffer) FL_NOEXCEPT override;

    bool drawFrame(const u16* buffer, size_t size_bytes) FL_NOEXCEPT override;
    bool waitFrameDone(u32 timeout_ms) FL_NOEXCEPT override;
    bool isBusy() const FL_NOEXCEPT override;

    bool registerDrawCallback(void* callback, void* user_ctx) FL_NOEXCEPT override;
    const LcdRgbPeripheralConfig& getConfig() const FL_NOEXCEPT override;

    u64 getMicroseconds() FL_NOEXCEPT override;
    void delay(u32 ms) FL_NOEXCEPT override;

    /// @brief Clear the busy flag (called from VSYNC ISR)
    void clearBusy() FL_NOEXCEPT { mBusy = false; }

private:
    friend class fl::Singleton<LcdRgbPeripheralEsp>;

    LcdRgbPeripheralEsp() FL_NOEXCEPT;

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
