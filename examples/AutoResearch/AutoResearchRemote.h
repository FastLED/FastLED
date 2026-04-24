// examples/AutoResearch/AutoResearchRemote.h
//
// Remote RPC control system for AutoResearch sketch.
// Provides JSON-RPC interface for dynamic test control via serial commands.
//
// ARCHITECTURE:
// - All test output is communicated EXCLUSIVELY via JSON-RPC
// - ScopedLogDisable suppresses FL_DBG/FL_PRINT during test execution
// - RPC responses use printJsonRaw() which bypasses fl::println (direct Serial output)
// - This allows clean, parseable JSON output without debug noise

#pragma once

#include "fl/stl/stdint.h"  // for uint32_t, uint8_t
#include "fl/channels/manager.h"  // for DriverInfo
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/vector.h"

// Forward declarations
namespace fl {
class json;
class Remote;
class RxChannel;
namespace net { namespace ble { struct TransportState; } }
}

/// @brief Factory function type for creating RxChannel instances
/// @param pin The GPIO pin to create the RxChannel on
/// @return A shared_ptr to the created RxChannel, or nullptr on failure
using RxDeviceFactory = fl::shared_ptr<fl::RxChannel>(*)(int pin);

/// @brief AutoResearch runtime state (shared between main loop and RPC handlers)
struct AutoResearchState {
    fl::vector<fl::DriverInfo> drivers_available;
    fl::shared_ptr<fl::RxChannel> rx_channel;
    fl::span<uint8_t> rx_buffer;
    int pin_tx;
    int pin_rx;
    int default_pin_tx;
    int default_pin_rx;
    RxDeviceFactory rx_factory;
    bool gpio_baseline_test_done = false;  // Track whether GPIO baseline test has run in loop()
    bool debug_enabled = false;  // Runtime debug logging toggle (default: off)
    bool net_server_active = false;  // Network server autoresearch active
    bool net_client_active = false;  // Network client autoresearch active
    bool ble_server_active = false;  // BLE GATT server autoresearch active
};

/// @brief Print JSON directly to Serial, bypassing fl::println and ScopedLogDisable
/// This ensures RPC responses are always visible regardless of log level
/// @param json JSON object to output
/// @param prefix Optional prefix (default: "REMOTE: ")
void printJsonRaw(const fl::json& json, const char* prefix = "REMOTE: ");

/// @brief Print JSONL stream message directly to Serial, bypassing fl::println
/// @param messageType Type of message (e.g., "test_result", "config_complete")
/// @param data JSON object containing message data
void printStreamRaw(const char* messageType, const fl::json& data);

/// @brief Remote RPC control system for test autoresearch
/// Encapsulates all RPC function registration and processing logic
class AutoResearchRemoteControl {
public:
    /// @brief Constructor
    AutoResearchRemoteControl();

    /// @brief Destructor
    ~AutoResearchRemoteControl();

    /// @brief Register all RPC functions with shared autoresearch state
    /// @param state Shared pointer to autoresearch runtime state
    void registerFunctions(fl::shared_ptr<AutoResearchState> state);

    /// @brief Process RPC system (pull + tick + push)
    /// @param current_millis Current timestamp in milliseconds
    /// @note This function is automatically called by the async task registered in setup()
    void tick(uint32_t current_millis);

    /// @brief Get underlying Remote instance
    fl::Remote* getRemote() { return mRemote.get(); }

    /// @brief Start BLE remote (creates BLE GATT server + second Remote instance)
    /// @return JSON result from startBle()
    fl::json startBleRemote();

    /// @brief Stop BLE remote (destroys BLE Remote + GATT server)
    /// @return JSON result from stopBle()
    fl::json stopBleRemote();

private:
    fl::unique_ptr<fl::Remote> mRemote;      // Serial Remote RPC system instance
    fl::unique_ptr<fl::Remote> mBleRemote;   // BLE Remote RPC system instance (created by startBle)
    fl::net::ble::TransportState* mBleState = nullptr; // Opaque BLE state (heap-allocated by ble::createTransport, nullptr on stub platforms)
    fl::shared_ptr<AutoResearchState> mState;  // Shared autoresearch state
    bool mPendingBleStop = false;            // Deferred BLE teardown flag (set by stopBle RPC)

    /// @brief Register all RPC methods on a given Remote instance
    void registerAllMethods(fl::Remote* remote);

    // Private helper functions for RPC implementation
    fl::json runSingleTestImpl(const fl::json& args);
    fl::json runParallelTestImpl(const fl::json& args);
    fl::json findConnectedPinsImpl(const fl::json& args);
};
