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
#if FL_PLATFORM_HAS_LARGE_MEMORY

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
#include "fl/stl/cstring.h"
#include "fl/task/task.h"
#include "fl/task/executor.h"
#include <Arduino.h>
#include "fl/math/wave/wave_perf_bench.h"
#include "fl/stl/atomic.h"
#include "fl/task/promise.h"

#include "fl/system/pin_probe.h"  // pin discovery skip rules (#3446)
#include "fl/math/simd.h"
#include "AutoResearchSimd.h"
#include "AutoResearchAnimartrixBench.h"  // animartrixPerlinBench RPC (#2628 follow-up)
#include "AutoResearchWave8Expand.h"  // #2526 wave8ExpandBenchmark RPC
#include "AutoResearchParlioEncode.h" // parlioEncodeBenchmark RPC (#2526 follow-up)
#include "AutoResearchTimingDrift.h"  // timingDriftTest RPC (#2994 repro)
#include "AutoResearchParlioStream.h" // parlioStreamValidate RPC (#2548 follow-up)
#include "AutoResearchFlexIo.h"       // flexIoDeviceInfo RPC (#3515 Phase B)
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
    Serial.println(formatted.c_str());  // ok serial - ok autoresearch rpc serial - RPC response boundary
    Serial.flush();                     // ok serial - ok autoresearch rpc serial - host waits for RPC boundary
}

namespace {

void printRemoteResponseRaw(const fl::json& response) {
    fl::string formatted = fl::formatJsonResponse(response, "REMOTE: ");
    Serial.println(formatted.c_str());  // ok serial - ok autoresearch rpc serial - RPC response boundary
    Serial.flush();                     // ok serial - ok autoresearch rpc serial - host waits for RPC boundary
}

void printRemoteStreamRaw(fl::JsonStreamCallback writeJson) {
    Serial.print("REMOTE: ");  // ok serial - ok autoresearch rpc serial - RPC response boundary

    fl::JsonStreamWriter writer([](const char* data, fl::size len) {
        Serial.write(reinterpret_cast<const uint8_t*>(data), static_cast<size_t>(len));  // ok serial - ok autoresearch rpc serial - streamed JSON bytes
    });
    if (writeJson) {
        writeJson(writer);
    }
    writer.flush();
    Serial.println();  // ok serial - ok autoresearch rpc serial - RPC response boundary
    Serial.flush();    // ok serial - ok autoresearch rpc serial - host waits for RPC boundary
}

}  // namespace

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
    Serial.println(formatted.c_str());  // ok serial - ok autoresearch rpc serial - stream response boundary
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
    //
    // FastLED #3446: default range stays at the historically-safe 0-8
    // window so an unparameterised call cannot accidentally drive a
    // boot/strap/USB pin into reset. Callers that need to sweep higher
    // pins (e.g. the user-reported (33, 34) short) should issue
    // multiple `findConnectedPins` calls with overlapping 8-pin windows
    // (0-8, 8-16, 16-24, ...) from the host side. The Python wrapper
    // `run_pin_discovery_segmented` in `ci/autoresearch/gpio.py` does
    // exactly that.
    int start_pin = 0;
    int end_pin = 8;
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
    // FastLED #3446: the platform owns the per-chip list of pins a probe
    // must not touch (SPI flash / PSRAM pads, flash power, USB-JTAG, the
    // console UART) and of pins it may read but not drive (input-only).
    // See fl/system/pin_probe.h; platforms without such pins report none.
    //
    // We surface skipped pins in the response so the host log says
    // exactly which pins were forbidden and why, turning silent
    // "no jumper found" into actionable "pins 6-11 are flash, 20 is
    // USB-JTAG, 34-39 are input-only".

    fl::json skipped_pins = fl::json::array();
    // Each pin is recorded once: the all-pairs scan below visits a pin many
    // times, and an unbounded array exhausts heap on ESP32.
    fl::u64 skipped_mask = 0;
    auto recordSkip = [&](int p, const char* reason) {
        const fl::u64 bit = fl::pinMaskBit(p);
        if (skipped_mask & bit) {
            return;
        }
        skipped_mask |= bit;
        fl::json entry = fl::json::object();
        entry.set("pin", static_cast<int64_t>(p));
        entry.set("reason", reason);
        skipped_pins.push_back(entry);
    };

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

    // Search every pin pair (a, b) in the range, both directions. Bench
    // jumpers are not always on neighbouring pins (a Teensy 4.0 wired across
    // the board was invisible to the old adjacent-only (n, n+1) scan).
    // Cost: 2 probes x ~4 ms per pair; a 0-40 range is ~6 s.
    int found_tx = -1;
    int found_rx = -1;

    // Drive `tx`, read `rx`; skips (and records) a driver that can't output.
    auto tryDirection = [&](int tx, int rx) -> bool {
        if (const char* why = fl::pinProbeDriveSkipReason(tx)) {
            recordSkip(tx, why);
            return false;
        }
        return testPinPair(tx, rx);
    };

    for (int a = start_pin; a <= end_pin && found_tx < 0; a++) {
        // FastLED #3446: hands off the platform's reserved pads (SPI
        // flash 6-11, USB-JTAG, ...) -- driving them locks the chip up or
        // corrupts the live flash transaction -- and off this board's
        // console and in-use PSRAM pins (fl::pinProbeSkipReason).
        if (const char* why = fl::pinProbeSkipReason(a)) {
            recordSkip(a, why);
            continue;
        }
        for (int b = a + 1; b <= end_pin; b++) {
            if (const char* why = fl::pinProbeSkipReason(b)) {
                recordSkip(b, why);
                continue;
            }
            if (tryDirection(a, b)) {
                found_tx = a;
                found_rx = b;
                FL_DBG("[PIN PROBE] Found connected pair: TX=" << found_tx << " -> RX=" << found_rx);
                break;
            }
            if (tryDirection(b, a)) {
                found_tx = b;  // Reversed: b drives a
                found_rx = a;
                FL_DBG("[PIN PROBE] Found connected pair (reversed): TX=" << found_tx << " -> RX=" << found_rx);
                break;
            }
        }
    }

    fl::json search_range = fl::json::array();
    search_range.push_back(static_cast<int64_t>(start_pin));
    search_range.push_back(static_cast<int64_t>(end_pin));
    response.set("searchRange", search_range);

    // FastLED #3446: surface every pin we declined to drive (and why)
    // so the host log can render "Skipped GPIO 6/7/8/9/10/11 (SPI flash),
    // 20 (USB-JTAG), 34-39 (input-only)". A small list — at most the
    // bad-pin count for the platform.
    response.set("skippedPins", skipped_pins);

    if (found_tx >= 0 && found_rx >= 0) {
        response.set("success", true);
        response.set("found", true);
        response.set("txPin", static_cast<int64_t>(found_tx));
        response.set("rxPin", static_cast<int64_t>(found_rx));

        // Auto-apply the found pins if requested
        if (auto_apply) {
            int old_tx = mState->pin_tx;
            int old_rx = mState->pin_rx;
            bool tx_changed = (found_tx != old_tx);
            bool rx_changed = (found_rx != old_rx);

            fl::shared_ptr<fl::RxChannel> replacement_rx;
            if (rx_changed) {
                if (mState->rx_factory) {
                    replacement_rx = mState->rx_factory(found_rx);
                }
                if (!replacement_rx) {
                    response.set("success", false);
                    response.set("error", "RxChannelCreationFailed");
                    response.set("message", "Failed to recreate RX channel on discovered pin");
                    response.set("autoApplied", false);
                    return response;
                }
            }

            // Commit only after the replacement channel is ready, preserving
            // the prior channel and validation state on factory failure.
            if (tx_changed || rx_changed) {
                mState->invalidateLedRxValidation();
            }
            mState->pin_tx = found_tx;
            mState->pin_rx = found_rx;
            if (rx_changed) {
                mState->rx_channel = replacement_rx;
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
        response.set("message", "No connected pin pairs found. Please connect a jumper wire between two GPIO pins.");
    }

    return response;
}

AutoResearchRemoteControl::AutoResearchRemoteControl()
    : mRemote(fl::make_unique<fl::Remote>(
        fl::createSerialRequestSource(),
        printRemoteResponseRaw,
        printRemoteStreamRaw
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
    bindSystemMethods(*mRemote);
    bindDriverMethods(*mRemote);
    bindPinMethods(*mRemote);
    bindMathMethods(*mRemote);
    bindNetworkMethods(*mRemote);
    bindAsyncMethods(*mRemote);
    bindBenchmarkMethods(*mRemote);
    bindRpSpiMethods(*mRemote);
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
    }
}

void AutoResearchRemoteControl::registerAllMethods(fl::Remote* remote) {
    // Register the core methods that BLE remote needs.
    // This registers a subset of methods — enough for ping/pong PoC.

    // Register "ping" function - health check with timestamp
    remote->bind("ping", [](const fl::json& args) -> fl::json {
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

#endif  // FL_PLATFORM_HAS_LARGE_MEMORY
