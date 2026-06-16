// examples/AutoResearch/AutoResearchRemote.cpp
//
// Remote RPC control system implementation for AutoResearch sketch.
//
// ARCHITECTURE:
// - RPC responses use printJsonRaw()/printStreamRaw() which bypass fl::println
// - Test execution wrapped in fl::ScopedLogDisable to suppress debug noise
// - This provides clean, parseable JSON output without FL_DBG/FL_PRINT spam

// Gate out under low-memory mode -- the LowMemory bring-up surface
// (AutoResearchLowMemory.h) binds its own minimal pinToggleRx / echo /
// ws2812SctTest handlers directly; the full remote-control wiring here
// references the AutoResearchTest / AutoResearchBle helpers and pulls in
// the entire RPC dispatch surface which doesn't fit Low-memory budgets.
// Matches the conditional structure in AutoResearch.ino itself.
#include "fl/system/sketch_macros.h"
#if !defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && !FL_PLATFORM_HAS_LARGE_MEMORY
#define FASTLED_AUTORESEARCH_LOW_MEMORY 1
#endif
#if !(defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && FASTLED_AUTORESEARCH_LOW_MEMORY)

// Legacy debug macros (no-ops, kept for debugTest RPC function)
#define DEBUG_PRINT(x) do {} while(0)
#define DEBUG_PRINTLN(x) do {} while(0)

#include "AutoResearchRemote.h"
#include "AutoResearchBle.h"
#include "AutoResearchNet.h"
#include "AutoResearchOta.h"
#include "fl/remote/transport/serial.h"
#include "fl/system/heap.h"
#include "Common.h"
#include "AutoResearchTest.h"
#include "AutoResearchHelpers.h"
#include "fl/stl/sstream.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/optional.h"
#include "fl/stl/json.h"
#include "fl/task/task.h"
#include "fl/task/executor.h"
#include "fl/math/wave/wave_perf_bench.h"
#include "fl/stl/atomic.h"
#include "fl/task/promise.h"
#include "fl/math/simd.h"
#include "AutoResearchSimd.h"
#include "AutoResearchAnimartrixBench.h"  // animartrixPerlinBench RPC (#2628 follow-up)
#include "AutoResearchWave8Expand.h"  // #2526 wave8ExpandBenchmark RPC
#include "AutoResearchParlioEncode.h" // parlioEncodeBenchmark RPC (#2526 follow-up)
#include "AutoResearchParlioStream.h" // parlioStreamValidate RPC (#2548 follow-up)
#include "fl/system/heap.h"
#include "fl/chipsets/spi.h"
#include "fl/channels/config.h"
#include <Arduino.h>

#include "fl/net/ble.h"

// Codec headers for decodeFile RPC
#include "fl/codec/h264.h"
#include "fl/codec/mp4_parser.h"
#include "fl/stl/detail/memory_file_handle.h"
#include "fl/fx/frame.h"

// ============================================================================
// Raw Serial Output Functions (bypass fl::println and ScopedLogDisable)
// ============================================================================

void printJsonRaw(const fl::json& json, const char* prefix) {
    // Serialize and print response
    fl::string formatted = fl::formatJsonResponse(json, prefix);
    fl::println(formatted.c_str());
    fl::flush();
}

void printStreamRaw(const char* messageType, const fl::json& data) {
    // Build pure JSONL message: RESULT: {"type":"...", ...data}
    fl::json output = fl::json::object();
    output.set("type", messageType);

    // Copy all fields from data into output
    if (data.is_object()) {
        auto keys = data.keys();
        for (fl::size i = 0; i < keys.size(); i++) {
            output.set(keys[i].c_str(), data[keys[i]]);
        }
    }

    // Use fl:: serial transport for consistent formatting
    fl::string formatted = fl::formatJsonResponse(output, "RESULT: ");
    fl::println(formatted.c_str());
}

// ============================================================================
// Standard JSON-RPC Response Format (Phase 4 Refactoring)
// ============================================================================
// Return codes:
//   0 = SUCCESS
//   1 = TEST_FAILED
//   2 = HARDWARE_ERROR (GPIO not connected)
//   3 = INVALID_ARGS

enum class ReturnCode : int {
    SUCCESS = 0,
    TEST_FAILED = 1,
    HARDWARE_ERROR = 2,
    INVALID_ARGS = 3
};

fl::json makeResponse(bool success, ReturnCode returnCode, const char* message,
                      const fl::json& data = fl::json()) {
    fl::json r = fl::json::object();
    r.set("success", success);
    r.set("returnCode", static_cast<int64_t>(static_cast<int>(returnCode)));
    r.set("message", message);
    if (!data.is_null() && data.has_value()) {
        r.set("data", data);
    }
    return r;
}

// No forward declarations needed - using one-test-per-RPC architecture

fl::json AutoResearchRemoteControl::findConnectedPinsImpl(const fl::json& args) {
    fl::json response = fl::json::object();

    // Parse optional arguments: [{startPin: int, endPin: int, autoApply: bool}]
    int start_pin = 0;
    int end_pin = 8;  // Default range: GPIO 0-8 (safe range, avoids USB/flash/strapping pins)
    bool auto_apply = true;  // If true, automatically apply found pins

    if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
        fl::json config = args[0];
        if (config.contains("startPin") && config["startPin"].is_int()) {
            start_pin = static_cast<int>(config["startPin"].as_int().value());
        }
        if (config.contains("endPin") && config["endPin"].is_int()) {
            end_pin = static_cast<int>(config["endPin"].as_int().value());
        }
        if (config.contains("autoApply") && config["autoApply"].is_bool()) {
            auto_apply = config["autoApply"].as_bool().value();
        }
    }

    // Validate range
    if (start_pin < 0 || start_pin > 48 || end_pin < 0 || end_pin > 48 || start_pin >= end_pin) {
        response.set("error", "InvalidRange");
        response.set("message", "Pin range must be 0-48 with startPin < endPin");
        return response;
    }

    FL_DBG("[PIN PROBE] Searching for connected pin pairs in range " << start_pin << "-" << end_pin);

    // Helper lambda to test if two pins are connected
    auto testPinPair = [](int tx, int rx) -> bool {
        // Test 1: TX drives LOW, RX has pullup → RX should read LOW if connected
        pinMode(tx, OUTPUT);
        pinMode(rx, INPUT_PULLUP);
        digitalWrite(tx, LOW);
        delay(2);  // Allow signal to settle
        int rx_when_tx_low = digitalRead(rx);

        // Test 2: TX drives HIGH → RX should read HIGH if connected
        digitalWrite(tx, HIGH);
        delay(2);  // Allow signal to settle
        int rx_when_tx_high = digitalRead(rx);

        // Restore pins to safe state
        pinMode(tx, INPUT);
        pinMode(rx, INPUT);

        return (rx_when_tx_low == LOW) && (rx_when_tx_high == HIGH);
    };

    // Search for connected adjacent pin pairs (n, n+1)
    int found_tx = -1;
    int found_rx = -1;
    fl::json tested_pairs = fl::json::array();

    for (int pin = start_pin; pin < end_pin; pin++) {
        int tx_candidate = pin;
        int rx_candidate = pin + 1;

        // No pin skip logic needed - default range (0-8) is safe for all platforms
        // Higher pins (USB, flash, strapping) are excluded by the reduced default range

        fl::json pair = fl::json::object();
        pair.set("tx", static_cast<int64_t>(tx_candidate));
        pair.set("rx", static_cast<int64_t>(rx_candidate));

        // Test TX→RX direction
        bool connected_forward = testPinPair(tx_candidate, rx_candidate);
        if (connected_forward) {
            pair.set("connected", true);
            pair.set("direction", "forward");
            tested_pairs.push_back(pair);
            found_tx = tx_candidate;
            found_rx = rx_candidate;
            FL_DBG("[PIN PROBE] Found connected pair: TX=" << found_tx << " -> RX=" << found_rx);
            break;
        }

        // Test RX→TX direction (reversed)
        bool connected_reverse = testPinPair(rx_candidate, tx_candidate);
        if (connected_reverse) {
            pair.set("connected", true);
            pair.set("direction", "reverse");
            tested_pairs.push_back(pair);
            found_tx = rx_candidate;  // Swap since reversed
            found_rx = tx_candidate;
            FL_DBG("[PIN PROBE] Found connected pair (reversed): TX=" << found_tx << " -> RX=" << found_rx);
            break;
        }

        pair.set("connected", false);
        tested_pairs.push_back(pair);
    }

    // NOTE: testedPairs array omitted - causes heap exhaustion on ESP32 (21+ objects = ~1500 bytes)
    // Validation script doesn't use this data, only needs {found, txPin, rxPin}
    // response.set("testedPairs", tested_pairs);
    fl::json search_range = fl::json::array();
    search_range.push_back(static_cast<int64_t>(start_pin));
    search_range.push_back(static_cast<int64_t>(end_pin));
    response.set("searchRange", search_range);

    if (found_tx >= 0 && found_rx >= 0) {
        response.set("success", true);
        response.set("found", true);
        response.set("txPin", static_cast<int64_t>(found_tx));
        response.set("rxPin", static_cast<int64_t>(found_rx));

        // Auto-apply the found pins if requested
        if (auto_apply) {
            int old_tx = mState->pin_tx;
            int old_rx = mState->pin_rx;
            bool rx_changed = (found_rx != old_rx);

            mState->pin_tx = found_tx;
            mState->pin_rx = found_rx;

            // Recreate RX channel if pin changed
            if (rx_changed && mState->rx_factory) {
                mState->rx_channel.reset();
                mState->rx_channel = mState->rx_factory(found_rx);
                if (!mState->rx_channel) {
                    FL_ERROR("[PIN PROBE] Failed to recreate RX channel on GPIO " << found_rx);
                    // Restore old values
                    mState->pin_tx = old_tx;
                    mState->pin_rx = old_rx;
                    response.set("error", "RxChannelCreationFailed");
                    response.set("autoApplied", false);
                    return response;
                }
            }

            FL_DBG("[PIN PROBE] Auto-applied pins: TX=" << found_tx << ", RX=" << found_rx);
            response.set("autoApplied", true);
            response.set("previousTxPin", static_cast<int64_t>(old_tx));
            response.set("previousRxPin", static_cast<int64_t>(old_rx));
        } else {
            response.set("autoApplied", false);
            response.set("message", "Use setPins to apply the found pins");
        }
    } else {
        response.set("success", true);  // Function succeeded, just no pins found
        response.set("found", false);
        response.set("message", "No connected pin pairs found. Please connect a jumper wire between adjacent GPIO pins.");
    }

    return response;
}

AutoResearchRemoteControl::AutoResearchRemoteControl()
    : mRemote(fl::make_unique<fl::Remote>(
        fl::createSerialRequestSource(),
        fl::createSerialResponseSink("REMOTE: ")
    )) {
    // mState will be set by registerFunctions()
}

AutoResearchRemoteControl::~AutoResearchRemoteControl() = default;


void AutoResearchRemoteControl::registerFunctions(fl::shared_ptr<AutoResearchState> state) {
    // Store shared state
    mState = state;

    // NOTE: All RPC callbacks use const fl::json& for efficient parameter passing.
    // The RPC system strips const/reference qualifiers and stores values in the tuple,
    // then passes them as references to the function. This avoids copies while
    // maintaining clean const-correct API.
    //
    // Bindings are partitioned across sibling .cpp files (#3132 / meta #3127) so
    // each file stays under 1 K LOC.
    bindBasicMethods();
    bindFlexioMethods();
    bindGpioMethods();
    bindBenchmarkMethods();
    bindAsyncNetMethods();
    bindCoroutineMethods();
    bindPerfMethods();
}


void AutoResearchRemoteControl::tick(uint32_t current_millis) {
    if (mRemote) {
        // Remote::update() does pull + tick + push
        mRemote->update(current_millis);
    }
    if (mBleRemote) {
        mBleRemote->update(current_millis);
    }
    // Deferred BLE teardown: stopBle RPC sets this flag so the response
    // is sent (via push() above) before we call ble::destroyTransport().
    if (mPendingBleStop) {
        mPendingBleStop = false;
        mBleRemote.reset();  // destroy lambdas before freeing state they capture
        fl::net::ble::destroyTransport(mBleState);
        mBleState = nullptr;
        mState->ble_server_active = false;
        getBleState().ble_server_active = false;
        FL_WARN("[BLE] Deferred teardown complete");
    }
}

void AutoResearchRemoteControl::registerAllMethods(fl::Remote* remote) {
    // Register the core methods that BLE remote needs.
    // This registers a subset of methods — enough for ping/pong PoC.

    // Register "ping" function - health check with timestamp
    remote->bind("ping", [this](const fl::json& args) -> fl::json {
        uint32_t now = millis();
        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("message", "pong");
        response.set("timestamp", static_cast<int64_t>(now));
        response.set("uptimeMs", static_cast<int64_t>(now));
        response.set("transport", "ble");
        return response;
    });

    // Register "status" function - device readiness check
    remote->bind("status", [this](const fl::json& args) -> fl::json {
        fl::json status = fl::json::object();
        status.set("ready", true);
        status.set("pinTx", static_cast<int64_t>(mState->pin_tx));
        status.set("pinRx", static_cast<int64_t>(mState->pin_rx));
        status.set("transport", "ble");
        return status;
    });
}

fl::json AutoResearchRemoteControl::startBleRemote() {
    if (mBleRemote) {
        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("message", "BLE remote already active");
        response.set("device_name", AUTORESEARCH_BLE_DEVICE_NAME);
        return response;
    }

    // Create BLE GATT server (heap-allocates transport state)
    // On stub platforms, createTransport returns nullptr and logs FL_ERROR.
    mBleState = fl::net::ble::createTransport(AUTORESEARCH_BLE_DEVICE_NAME);
    if (!mBleState) {
        fl::json response = fl::json::object();
        response.set("success", false);
        response.set("error", "BLE not available on this platform");
        return response;
    }

    // Get transport lambdas that capture mBleState
    auto callbacks = fl::net::ble::getTransportCallbacks(mBleState);

    // Create BLE Remote instance with BLE transport
    mBleRemote = fl::make_unique<fl::Remote>(callbacks.first, callbacks.second);

    // Register RPC methods on the BLE remote
    registerAllMethods(mBleRemote.get());

    mState->ble_server_active = true;
    getBleState().ble_server_active = true;

    fl::json response = fl::json::object();
    response.set("success", true);
    response.set("device_name", AUTORESEARCH_BLE_DEVICE_NAME);
    response.set("service_uuid", FL_BLE_SERVICE_UUID);
    response.set("rx_uuid", FL_BLE_CHAR_RX_UUID);
    response.set("tx_uuid", FL_BLE_CHAR_TX_UUID);
    FL_WARN("[BLE] Remote created and advertising");
    return response;
}

fl::json AutoResearchRemoteControl::stopBleRemote() {
    // Defer actual BLE teardown to tick() so the RPC response is sent first.
    // BLEDevice::deinit(true) blocks long enough to prevent the response
    // from being transmitted over serial before the device resets BLE state.
    mPendingBleStop = true;
    fl::json response = fl::json::object();
    response.set("success", true);
    return response;
}

#endif  // !FASTLED_AUTORESEARCH_LOW_MEMORY
