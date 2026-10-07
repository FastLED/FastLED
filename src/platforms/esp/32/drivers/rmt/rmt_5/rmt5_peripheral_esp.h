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

// IWYU pragma: private

#include "platforms/esp/is_esp.h"

#ifdef FL_IS_ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "platforms/esp/32/drivers/rmt/rmt_5/irmt5_peripheral.h"
#include "fl/stl/singleton.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/span.h"

namespace fl {
namespace detail {

//=============================================================================
// Real Hardware Peripheral Interface
//=============================================================================

/// @brief Real ESP32 RMT5 peripheral interface
///
/// Thin wrapper around ESP-IDF RMT5 APIs. All methods delegate
/// directly to ESP-IDF with minimal overhead.
///
/// Production uses direct calls to this concrete wrapper. The engine keeps
/// IRMT5Peripheral injection on host builds for the same driver tests.
class Rmt5PeripheralESP {
public:
    /// @brief Get the singleton instance
    /// @return Reference to the singleton peripheral
    ///
    /// Mirrors the hardware constraint that there is only one RMT peripheral
    /// (though multiple channels can be created).
    static Rmt5PeripheralESP& instance() FL_NO_EXCEPT;

    /// @brief Channel and encoder cleanup belongs to ChannelEngineRMT
    ~Rmt5PeripheralESP() = default;

    // Prevent copying (singleton pattern)
    Rmt5PeripheralESP(const Rmt5PeripheralESP&) = delete;
    Rmt5PeripheralESP& operator=(const Rmt5PeripheralESP&) = delete;

    //=========================================================================
    // Hardware Delegation
    //=========================================================================

    bool createTxChannel(const Rmt5ChannelConfig& config,
                         void** out_handle) FL_NO_EXCEPT;
    bool deleteChannel(void* channel_handle) FL_NO_EXCEPT;
    bool enableChannel(void* channel_handle) FL_NO_EXCEPT;
    bool disableChannel(void* channel_handle) FL_NO_EXCEPT;
    bool transmit(void* channel_handle, void* encoder_handle,
                  fl::span<const u8> buffer) FL_NO_EXCEPT;
    bool waitAllDone(void* channel_handle, u32 timeout_ms) FL_NO_EXCEPT;
    void* createEncoder(const ChipsetTiming& timing,
                        u32 resolution_hz) FL_NO_EXCEPT;
    void deleteEncoder(void* encoder_handle) FL_NO_EXCEPT;
    bool resetEncoder(void* encoder_handle) FL_NO_EXCEPT;
    bool registerTxCallback(void* channel_handle,
                            Rmt5TxDoneCallback callback,
                            void* user_ctx) FL_NO_EXCEPT;
    void configureLogging() FL_NO_EXCEPT;
    bool syncCache(fl::span<u8> buffer) FL_NO_EXCEPT;
    bool canTransmitDirectly(const void* buffer) const FL_NO_EXCEPT;
    u8* allocateDmaBuffer(size_t size) FL_NO_EXCEPT;
    void freeDmaBuffer(u8* buffer) FL_NO_EXCEPT;

private:
    friend class fl::Singleton<Rmt5PeripheralESP>;
    Rmt5PeripheralESP() = default;

    // A cache API argument error disables future sync attempts, while the
    // implementation continues to enforce memory ordering with barriers.
    bool mCacheSyncDisabled = false;
};

} // namespace detail
} // namespace fl

#endif // FASTLED_RMT5
#endif // FL_IS_ESP32
