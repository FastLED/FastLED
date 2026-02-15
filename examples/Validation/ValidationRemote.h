// examples/Validation/ValidationRemote.h
//
// Remote RPC control system for Validation sketch.
// Provides JSON-RPC interface for dynamic test control via serial commands.
//
// ARCHITECTURE:
// - All test output is communicated EXCLUSIVELY via JSON-RPC
// - ScopedLogDisable suppresses FL_DBG/FL_PRINT during test execution
// - RPC responses use printJsonRaw() which bypasses fl::println (direct Serial output)
// - This allows clean, parseable JSON output without debug noise

#pragma once

#include "fl/stl/stdint.h"  // for uint32_t, uint8_t
#include "fl/channels/bus_manager.h"  // for DriverInfo
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/vector.h"

// Forward declarations
namespace fl {
    class Json;
    class Remote;
    struct RxDevice;
}

/// @brief Factory function type for creating RxDevice instances
/// @param pin The GPIO pin to create the RxDevice on
/// @return A shared_ptr to the created RxDevice, or nullptr on failure
using RxDeviceFactory = fl::shared_ptr<fl::RxDevice>(*)(int pin);

/// @brief Validation runtime state (shared between main loop and RPC handlers)
struct ValidationState {
    fl::vector<fl::DriverInfo> drivers_available;
    fl::shared_ptr<fl::RxDevice> rx_channel;
    fl::span<uint8_t> rx_buffer;
    int pin_tx;
    int pin_rx;
    int default_pin_tx;
    int default_pin_rx;
    RxDeviceFactory rx_factory;
    bool gpio_baseline_test_done = false;  // Track whether GPIO baseline test has run in loop()
    bool debug_enabled = false;  // Runtime debug logging toggle (default: off)
};

/// @brief Print JSON directly to Serial, bypassing fl::println and ScopedLogDisable
/// This ensures RPC responses are always visible regardless of log level
/// @param json JSON object to output
/// @param prefix Optional prefix (default: "REMOTE: ")
void printJsonRaw(const fl::Json& json, const char* prefix = "REMOTE: ");

/// @brief Print JSONL stream message directly to Serial, bypassing fl::println
/// @param messageType Type of message (e.g., "test_result", "config_complete")
/// @param data JSON object containing message data
void printStreamRaw(const char* messageType, const fl::Json& data);

/// @brief Remote RPC control system for test validation
/// Encapsulates all RPC function registration and processing logic
class ValidationRemoteControl {
public:
    /// @brief Constructor
    ValidationRemoteControl();

    /// @brief Destructor
    ~ValidationRemoteControl();

    /// @brief Register all RPC functions with shared validation state
    /// @param state Shared pointer to validation runtime state
    void registerFunctions(fl::shared_ptr<ValidationState> state);

    /// @brief Process RPC system (pull + tick + push)
    /// @param current_millis Current timestamp in milliseconds
    /// @note This function is automatically called by the async task registered in setup()
    void tick(uint32_t current_millis);

    /// @brief Get underlying Remote instance
    fl::Remote* getRemote() { return mRemote.get(); }

private:
    fl::unique_ptr<fl::Remote> mRemote;  // Remote RPC system instance
    fl::shared_ptr<ValidationState> mState;  // Shared validation state

    // Private helper functions for RPC implementation
    fl::Json runSingleTestImpl(const fl::Json& args);
    fl::Json findConnectedPinsImpl(const fl::Json& args);
};
