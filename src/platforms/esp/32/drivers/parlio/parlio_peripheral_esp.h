/// @file parlio_peripheral_esp.h
/// @brief Real ESP32 PARLIO peripheral implementation (thin hardware wrapper)
///
/// This class is a thin delegation layer around ESP-IDF PARLIO APIs.
/// It implements the IParlioPeripheral interface by directly calling
/// ESP-IDF functions with minimal overhead.
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

// Forward declarations (avoid including ESP-IDF headers in .h file)
struct parlio_tx_unit_t;
typedef struct parlio_tx_unit_t *parlio_tx_unit_handle_t;

namespace fl {
namespace detail {

//=============================================================================
// Real Hardware Peripheral Implementation
//=============================================================================

/// @brief Real ESP32 PARLIO peripheral implementation
///
/// Thin wrapper around ESP-IDF PARLIO TX APIs. All methods delegate
/// directly to ESP-IDF with minimal overhead.
///
/// ## Lifecycle
/// ```cpp
/// ParlioPeripheralESP peripheral;
///
/// // Initialize
/// ParlioPeripheralConfig config = {...};
/// peripheral.initialize(config);
///
/// // Register callback
/// peripheral.registerTxDoneCallback(callback, ctx);
///
/// // Transmit
/// peripheral.enable();
/// peripheral.transmit(buffer, bits, idle);
/// peripheral.waitAllDone(timeout);
/// peripheral.disable();
/// ```
///
/// ## Memory Management
///
/// - Constructor: No allocation
/// - initialize(): Allocates ESP-IDF PARLIO TX unit handle
/// - Destructor: Frees TX unit handle automatically
class ParlioPeripheralESP : public IParlioPeripheral {
public:
    /// @brief Constructor - no allocation
    ParlioPeripheralESP();

    /// @brief Destructor - frees ESP-IDF TX unit handle
    ~ParlioPeripheralESP() override;

    // Prevent copying (handle cannot be shared)
    ParlioPeripheralESP(const ParlioPeripheralESP&) = delete;
    ParlioPeripheralESP& operator=(const ParlioPeripheralESP&) = delete;

    //=========================================================================
    // IParlioPeripheral Interface Implementation
    //=========================================================================

    bool initialize(const ParlioPeripheralConfig& config) override;
    bool enable() override;
    bool disable() override;
    bool transmit(const uint8_t* buffer, size_t bit_count, uint16_t idle_value) override;
    bool waitAllDone(uint32_t timeout_ms) override;
    bool registerTxDoneCallback(void* callback, void* user_ctx) override;
    uint8_t* allocateDmaBuffer(size_t size) override;
    void freeDmaBuffer(uint8_t* buffer) override;
    void delay(uint32_t ms) override;
    task_handle_t createTask(const TaskConfig& config) override;
    void deleteTask(task_handle_t task_handle) override;
    void deleteCurrentTask() override;
    timer_handle_t createTimer(const TimerConfig& config) override;
    bool enableTimer(timer_handle_t handle) override;
    bool startTimer(timer_handle_t handle) override;
    bool stopTimer(timer_handle_t handle) override;
    bool disableTimer(timer_handle_t handle) override;
    void deleteTimer(timer_handle_t handle) override;
    uint64_t getMicroseconds() override;
    void freeDmaBuffer(void* ptr) override;

private:
    parlio_tx_unit_handle_t mTxUnit;  ///< ESP-IDF TX unit handle
    bool mEnabled;                     ///< Track enable state (for cleanup)
};

} // namespace detail
} // namespace fl

#endif // FASTLED_ESP32_HAS_PARLIO
#endif // ESP32
