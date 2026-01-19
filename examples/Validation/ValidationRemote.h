// examples/Validation/ValidationRemote.h
//
// Remote RPC control system for Validation sketch.
// Provides JSON-RPC interface for dynamic test control via serial commands.
//
// ARCHITECTURE:
// - All test output is communicated EXCLUSIVELY via JSON-RPC
// - fl::ScopedLogDisable suppresses FL_DBG/FL_PRINT during test execution
// - RPC responses use printJsonRaw() which bypasses fl::println (direct Serial output)
// - This allows clean, parseable JSON output without debug noise

#pragma once

#include "fl/remote.h"
#include "fl/json.h"
#include "fl/stl/vector.h"
#include "fl/stl/string.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/cstdio.h"  // For fl::ScopedLogDisable

// Forward declarations
namespace fl {
    struct TestMatrixConfig;
    struct TestCaseConfig;
    struct TestCaseResult;
    struct DriverInfo;
    struct RxDevice;
    struct NamedTimingConfig;
}

/// @brief Factory function type for creating RxDevice instances
/// @param pin The GPIO pin to create the RxDevice on
/// @return A shared_ptr to the created RxDevice, or nullptr on failure
using RxDeviceFactory = fl::shared_ptr<fl::RxDevice>(*)(int pin);

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

    /// @brief Register all RPC functions with external state references
    /// @param drivers_available Reference to available drivers vector
    /// @param test_matrix Reference to test matrix configuration
    /// @param test_cases Reference to test cases vector
    /// @param test_results Reference to test results vector
    /// @param start_command_received Reference to start command flag
    /// @param test_matrix_complete Reference to completion flag
    /// @param frame_counter Reference to frame counter
    /// @param rx_channel Reference to RX channel (may be recreated on pin change)
    /// @param rx_buffer RX buffer span
    /// @param pin_tx Reference to TX pin variable (runtime configurable)
    /// @param pin_rx Reference to RX pin variable (runtime configurable)
    /// @param default_pin_tx Default TX pin for this platform
    /// @param default_pin_rx Default RX pin for this platform
    /// @param rx_factory Factory function for creating RxDevice instances
    void registerFunctions(
        fl::vector<fl::DriverInfo>& drivers_available,
        fl::TestMatrixConfig& test_matrix,
        fl::vector<fl::TestCaseConfig>& test_cases,
        fl::vector<fl::TestCaseResult>& test_results,
        bool& start_command_received,
        bool& test_matrix_complete,
        uint32_t& frame_counter,
        fl::shared_ptr<fl::RxDevice>& rx_channel,
        fl::span<uint8_t> rx_buffer,
        int& pin_tx,
        int& pin_rx,
        int default_pin_tx,
        int default_pin_rx,
        RxDeviceFactory rx_factory
    );

    /// @brief Process scheduled RPC commands (call from loop())
    /// @param current_millis Current timestamp in milliseconds
    void tick(uint32_t current_millis);

    /// @brief Process incoming serial data for RPC commands
    /// @return true if START command received, false otherwise
    bool processSerialInput();

    /// @brief Get underlying Remote instance
    fl::Remote* getRemote() { return mRemote.get(); }

private:
    fl::unique_ptr<fl::Remote> mRemote;  // Remote RPC system instance

    // External state references (set by registerFunctions)
    fl::vector<fl::DriverInfo>* mpDriversAvailable;
    fl::TestMatrixConfig* mpTestMatrix;
    fl::vector<fl::TestCaseConfig>* mpTestCases;
    fl::vector<fl::TestCaseResult>* mpTestResults;
    bool* mpStartCommandReceived;
    bool* mpTestMatrixComplete;
    uint32_t* mpFrameCounter;
    fl::shared_ptr<fl::RxDevice>* mpRxChannel;  // Pointer to shared_ptr (for RX recreation)
    fl::span<uint8_t> mRxBuffer;

    // Pin configuration (runtime modifiable via RPC)
    int* mpPinTx;
    int* mpPinRx;
    int mDefaultPinTx;
    int mDefaultPinRx;

    // RxDevice factory for creating/recreating RX channels
    RxDeviceFactory mRxFactory;
};
