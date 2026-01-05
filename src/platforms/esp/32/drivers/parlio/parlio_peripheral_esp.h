/// @file parlio_peripheral_esp.h
/// @brief Real ESP32 PARLIO peripheral interface (thin header)
///
/// This header provides a thin interface to the ESP32 PARLIO hardware.
/// All implementation details and ESP-IDF dependencies are in the .cpp file.
///
/// ## Design Philosophy
///
/// This implementation follows the "thin wrapper" pattern:
/// - NO business logic (pure delegation to ESP-IDF)
/// - NO state validation beyond what ESP-IDF provides
/// - NO performance overhead (inline-able calls)
/// - ALL logic stays in ParlioEngine (testable via mock)
///
/// ## Thread Safety
///
/// Thread safety is inherited from ESP-IDF PARLIO driver:
/// - initialize() NOT thread-safe (call once during setup)
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
/// The ParlioEngine logs errors internally for debugging.

#pragma once

#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_PARLIO

#include "iparlio_peripheral.h"
#include "fl/singleton.h"

namespace fl {
namespace detail {

// Forward declaration for Singleton friendship
class ParlioPeripheralESPImpl;

//=============================================================================
// Real Hardware Peripheral Interface
//=============================================================================

/// @brief Real ESP32 PARLIO peripheral interface
///
/// Thin wrapper around ESP-IDF PARLIO TX APIs. All methods delegate
/// directly to ESP-IDF with minimal overhead.
///
/// This is an abstract interface - use instance() to access the singleton.
class ParlioPeripheralESP : public IParlioPeripheral {
public:
    /// @brief Get the singleton instance
    /// @return Reference to the singleton peripheral
    ///
    /// Mirrors the hardware constraint that there is only one PARLIO peripheral.
    static ParlioPeripheralESP& instance();

    /// @brief Destructor - frees ESP-IDF TX unit handle
    ~ParlioPeripheralESP() override;

    // Prevent copying (handle cannot be shared)
    ParlioPeripheralESP(const ParlioPeripheralESP&) = delete;
    ParlioPeripheralESP& operator=(const ParlioPeripheralESP&) = delete;

    //=========================================================================
    // IParlioPeripheral Interface Implementation
    //=========================================================================

    bool initialize(const ParlioPeripheralConfig& config) override = 0;
    bool enable() override = 0;
    bool disable() override = 0;
    bool isInitialized() const override = 0;
    bool transmit(const uint8_t* buffer, size_t bit_count, uint16_t idle_value) override = 0;
    bool waitAllDone(uint32_t timeout_ms) override = 0;
    bool registerTxDoneCallback(void* callback, void* user_ctx) override = 0;
    uint8_t* allocateDmaBuffer(size_t size) override = 0;
    void freeDmaBuffer(uint8_t* buffer) override = 0;
    void delay(uint32_t ms) override = 0;
    uint64_t getMicroseconds() override = 0;
    void freeDmaBuffer(void* ptr) override = 0;

protected:
    /// @brief Protected constructor for singleton
    ParlioPeripheralESP() = default;
};

} // namespace detail
} // namespace fl

#endif // FASTLED_ESP32_HAS_PARLIO
#endif // ESP32
