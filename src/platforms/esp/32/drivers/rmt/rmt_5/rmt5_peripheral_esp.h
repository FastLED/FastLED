/// @file rmt5_peripheral_esp.h
/// @brief Real ESP32 RMT5 peripheral interface (thin header)
///
/// This header provides a thin interface to the ESP32 RMT5 hardware.
/// All implementation details and ESP-IDF dependencies are in the .cpp file.
///
/// ## Design Philosophy
///
/// This implementation follows the "thin wrapper" pattern:
/// - NO business logic (pure delegation to ESP-IDF)
/// - NO state validation beyond what ESP-IDF provides
/// - NO performance overhead (inline-able calls)
/// - ALL logic stays in ChannelEngineRMT (testable via mock)
///
/// ## Thread Safety
///
/// Thread safety is inherited from ESP-IDF RMT driver:
/// - createTxChannel() NOT thread-safe (call once per channel)
/// - transmit() can be called from ISR context (ISR-safe)
/// - Other methods NOT thread-safe (caller synchronizes)
///
/// ## Error Handling
///
/// All methods return bool for success/failure:
/// - true: Operation succeeded (ESP_OK)
/// - false: Operation failed (any ESP-IDF error code)
///
/// Detailed error codes are NOT propagated through the interface.
/// The ChannelEngineRMT logs errors internally for debugging.

#pragma once

#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "irmt5_peripheral.h"
#include "fl/singleton.h"

namespace fl {
namespace detail {

// Forward declaration for Singleton friendship
class Rmt5PeripheralESPImpl;

//=============================================================================
// Real Hardware Peripheral Interface
//=============================================================================

/// @brief Real ESP32 RMT5 peripheral interface
///
/// Thin wrapper around ESP-IDF RMT5 APIs. All methods delegate
/// directly to ESP-IDF with minimal overhead.
///
/// This is an abstract interface - use instance() to access the singleton.
class Rmt5PeripheralESP : public IRMT5Peripheral {
public:
    /// @brief Get the singleton instance
    /// @return Reference to the singleton peripheral
    ///
    /// Mirrors the hardware constraint that there is only one RMT peripheral
    /// (though multiple channels can be created).
    static Rmt5PeripheralESP& instance();

    /// @brief Destructor - cleanup is handled by implementation
    ~Rmt5PeripheralESP() override;

    // Prevent copying (singleton pattern)
    Rmt5PeripheralESP(const Rmt5PeripheralESP&) = delete;
    Rmt5PeripheralESP& operator=(const Rmt5PeripheralESP&) = delete;

    //=========================================================================
    // IRMT5Peripheral Interface Implementation
    //=========================================================================

    bool createTxChannel(const Rmt5ChannelConfig& config,
                         void** out_handle) override = 0;
    bool deleteChannel(void* channel_handle) override = 0;
    bool enableChannel(void* channel_handle) override = 0;
    bool disableChannel(void* channel_handle) override = 0;
    bool transmit(void* channel_handle, void* encoder_handle,
                  const uint8_t* buffer, size_t buffer_size) override = 0;
    bool waitAllDone(void* channel_handle, uint32_t timeout_ms) override = 0;
    void* createEncoder(const ChipsetTiming& timing,
                        uint32_t resolution_hz) override = 0;
    void deleteEncoder(void* encoder_handle) override = 0;
    bool resetEncoder(void* encoder_handle) override = 0;
    bool registerTxCallback(void* channel_handle, void* callback,
                            void* user_ctx) override = 0;
    void configureLogging() override = 0;
    bool syncCache(void* buffer, size_t size) override = 0;
    uint8_t* allocateDmaBuffer(size_t size) override = 0;
    void freeDmaBuffer(uint8_t* buffer) override = 0;

protected:
    /// @brief Protected constructor for singleton
    Rmt5PeripheralESP() = default;
};

} // namespace detail
} // namespace fl

#endif // FASTLED_RMT5
#endif // ESP32
