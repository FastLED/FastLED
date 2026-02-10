// examples/Validation/ValidationRemote.cpp
//
// Remote RPC control system implementation for Validation sketch.
//
// ARCHITECTURE:
// - RPC responses use printJsonRaw()/printStreamRaw() which bypass fl::println
// - Test execution wrapped in fl::ScopedLogDisable to suppress debug noise
// - This provides clean, parseable JSON output without FL_DBG/FL_PRINT spam

#include "ValidationRemote.h"
#include "ValidationConfig.h"
#include "Common.h"
#include "ValidationTest.h"
#include "ValidationHelpers.h"
#include "fl/stl/sstream.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/optional.h"
#include "fl/json.h"
#include "fl/simd.h"
#include <Arduino.h>

// ============================================================================
// Raw Serial Output Functions (bypass fl::println and ScopedLogDisable)
// ============================================================================

void printJsonRaw(const fl::Json& json, const char* prefix) {
    fl::string jsonStr = json.to_string();

    // Ensure single-line (replace any newlines/carriage returns with spaces)
    for (size_t i = 0; i < jsonStr.size(); ++i) {
        if (jsonStr[i] == '\n' || jsonStr[i] == '\r') {
            jsonStr[i] = ' ';
        }
    }

    // Output directly to Serial, bypassing fl::println
    if (prefix && prefix[0] != '\0') {
        Serial.print(prefix);
    }
    Serial.println(jsonStr.c_str());
}

void printStreamRaw(const char* messageType, const fl::Json& data) {
    // Build pure JSONL message: RESULT: {"type":"...", ...data}
    fl::Json output = fl::Json::object();
    output.set("type", messageType);

    // Copy all fields from data into output
    if (data.is_object()) {
        auto keys = data.keys();
        for (fl::size i = 0; i < keys.size(); i++) {
            output.set(keys[i].c_str(), data[keys[i]]);
        }
    }

    // Serialize to compact JSON
    fl::string jsonStr = output.to_string();

    // Ensure single-line
    for (size_t i = 0; i < jsonStr.size(); ++i) {
        if (jsonStr[i] == '\n' || jsonStr[i] == '\r') {
            jsonStr[i] = ' ';
        }
    }

    // Output directly to Serial: RESULT: <json-with-type>
    Serial.print("RESULT: ");
    Serial.println(jsonStr.c_str());
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

fl::Json makeResponse(bool success, ReturnCode returnCode, const char* message,
                      const fl::Json& data = fl::Json()) {
    fl::Json r = fl::Json::object();
    r.set("success", success);
    r.set("returnCode", static_cast<int64_t>(static_cast<int>(returnCode)));
    r.set("message", message);
    if (!data.is_null() && data.has_value()) {
        r.set("data", data);
    }
    return r;
}

// No forward declarations needed - using one-test-per-RPC architecture

ValidationRemoteControl::ValidationRemoteControl()
    : mRemote(fl::make_unique<fl::Remote>())
    , mpDriversAvailable(nullptr)
    , mpRxChannel(nullptr)
    , mRxBuffer(nullptr, 0)
    , mpPinTx(nullptr)
    , mpPinRx(nullptr)
    , mDefaultPinTx(1)
    , mDefaultPinRx(0)
    , mRxFactory(nullptr) {
}

ValidationRemoteControl::~ValidationRemoteControl() = default;

void ValidationRemoteControl::registerFunctions(
    fl::vector<fl::DriverInfo>& drivers_available,
    fl::shared_ptr<fl::RxDevice>& rx_channel,
    fl::span<uint8_t> rx_buffer,
    int& pin_tx,
    int& pin_rx,
    int default_pin_tx,
    int default_pin_rx,
    RxDeviceFactory rx_factory) {

    // Store references to external state
    mpDriversAvailable = &drivers_available;
    mpRxChannel = &rx_channel;  // Pointer to shared_ptr for RX recreation
    mRxBuffer = rx_buffer;
    mpPinTx = &pin_tx;
    mpPinRx = &pin_rx;
    mDefaultPinTx = default_pin_tx;
    mDefaultPinRx = default_pin_rx;
    mRxFactory = rx_factory;

    // Register "status" function - device readiness check
    mRemote->registerFunctionWithReturn("status", [this](const fl::Json& args) -> fl::Json {
        fl::Json status = fl::Json::object();
        status.set("ready", true);
        status.set("pinTx", static_cast<int64_t>(*mpPinTx));
        status.set("pinRx", static_cast<int64_t>(*mpPinRx));
        return status;
    });

    // Register "drivers" function - list available drivers
    mRemote->registerFunctionWithReturn("drivers", [this](const fl::Json& args) -> fl::Json {
        fl::Json drivers = fl::Json::array();
        for (fl::size i = 0; i < mpDriversAvailable->size(); i++) {
            fl::Json driver = fl::Json::object();
            driver.set("name", (*mpDriversAvailable)[i].name.c_str());
            driver.set("priority", static_cast<int64_t>((*mpDriversAvailable)[i].priority));
            driver.set("enabled", (*mpDriversAvailable)[i].enabled);
            drivers.push_back(driver);
        }
        return drivers;
    });

    // Returns: {success, passed, totalTests, passedTests, duration_ms, driver,
    //          laneCount, laneSizes, pattern, firstFailure?}
    mRemote->registerFunctionWithReturn("runSingleTest", [this](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        // Validate args is an array with single config object
        if (!args.is_array() || args.size() != 1 || !args[0].is_object()) {
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [{driver, laneSizes, pattern?, iterations?, pinTx?, pinRx?, timing?}]");
            return response;
        }

        fl::Json config = args[0];

        // ========== REQUIRED PARAMETERS ==========

        // 1. Extract driver (required)
        if (!config.contains("driver") || !config["driver"].is_string()) {
            response.set("success", false);
            response.set("error", "MissingDriver");
            response.set("message", "Required field 'driver' (string) missing");
            return response;
        }
        fl::string driver_name = config["driver"].as_string().value();

        // Validate driver exists
        bool driver_found = false;
        for (fl::size i = 0; i < mpDriversAvailable->size(); i++) {
            if ((*mpDriversAvailable)[i].name == driver_name) {
                driver_found = true;
                break;
            }
        }
        if (!driver_found) {
            response.set("success", false);
            response.set("error", "UnknownDriver");
            fl::sstream msg;
            msg << "Driver '" << driver_name.c_str() << "' not available";
            response.set("message", msg.str().c_str());
            return response;
        }

        // 2. Extract laneSizes (required)
        if (!config.contains("laneSizes") || !config["laneSizes"].is_array()) {
            response.set("success", false);
            response.set("error", "MissingLaneSizes");
            response.set("message", "Required field 'laneSizes' (array) missing");
            return response;
        }

        fl::Json lane_sizes_json = config["laneSizes"];
        if (lane_sizes_json.size() == 0 || lane_sizes_json.size() > 8) {
            response.set("success", false);
            response.set("error", "InvalidLaneCount");
            response.set("message", "laneSizes must have 1-8 elements");
            return response;
        }

        fl::vector<int> lane_sizes;
        const int max_leds_per_lane = mRxBuffer.size() / 32;
        for (fl::size i = 0; i < lane_sizes_json.size(); i++) {
            if (!lane_sizes_json[i].is_int()) {
                response.set("success", false);
                response.set("error", "InvalidLaneSizeType");
                fl::sstream msg;
                msg << "laneSizes[" << i << "] must be integer";
                response.set("message", msg.str().c_str());
                return response;
            }
            int size = static_cast<int>(lane_sizes_json[i].as_int().value());
            if (size <= 0) {
                response.set("success", false);
                response.set("error", "InvalidLaneSize");
                fl::sstream msg;
                msg << "laneSizes[" << i << "] = " << size << " must be > 0";
                response.set("message", msg.str().c_str());
                return response;
            }
            if (size > max_leds_per_lane) {
                response.set("success", false);
                response.set("error", "LaneSizeTooLarge");
                fl::sstream msg;
                msg << "laneSizes[" << i << "] = " << size << " exceeds max " << max_leds_per_lane;
                response.set("message", msg.str().c_str());
                return response;
            }
            lane_sizes.push_back(size);
        }

        // ========== OPTIONAL PARAMETERS ==========

        // 3. Extract pattern (optional, default: "MSB_LSB_A")
        fl::string pattern = "MSB_LSB_A";
        if (config.contains("pattern") && config["pattern"].is_string()) {
            pattern = config["pattern"].as_string().value();
        }

        // 4. Extract iterations (optional, default: 1)
        int iterations = 1;
        if (config.contains("iterations") && config["iterations"].is_int()) {
            iterations = static_cast<int>(config["iterations"].as_int().value());
            if (iterations < 1) {
                response.set("success", false);
                response.set("error", "InvalidIterations");
                response.set("message", "iterations must be >= 1");
                return response;
            }
        }

        // 5. Extract pinTx (optional, default: use mpPinTx)
        int pin_tx = *mpPinTx;
        if (config.contains("pinTx") && config["pinTx"].is_int()) {
            pin_tx = static_cast<int>(config["pinTx"].as_int().value());
            if (pin_tx < 0 || pin_tx > 48) {
                response.set("success", false);
                response.set("error", "InvalidPinTx");
                response.set("message", "pinTx must be 0-48");
                return response;
            }
        }

        // 6. Extract pinRx (optional, default: use mpPinRx)
        int pin_rx = *mpPinRx;
        if (config.contains("pinRx") && config["pinRx"].is_int()) {
            pin_rx = static_cast<int>(config["pinRx"].as_int().value());
            if (pin_rx < 0 || pin_rx > 48) {
                response.set("success", false);
                response.set("error", "InvalidPinRx");
                response.set("message", "pinRx must be 0-48");
                return response;
            }
        }

        // 7. Extract timing (optional, default: "WS2812B-V5")
        fl::string timing_name = "WS2812B-V5";
        if (config.contains("timing") && config["timing"].is_string()) {
            timing_name = config["timing"].as_string().value();
        }

        // ========== EXECUTION ==========

        uint32_t start_ms = millis();

        // Set driver as exclusive
        if (!FastLED.setExclusiveDriver(driver_name.c_str())) {
            response.set("success", false);
            response.set("error", "DriverSetupFailed");
            fl::sstream msg;
            msg << "Failed to set " << driver_name.c_str() << " as exclusive driver";
            response.set("message", msg.str().c_str());
            return response;
        }

        // Get timing configuration (currently hardcoded to WS2812B-V5)
        fl::NamedTimingConfig timing_config(
            fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(),
            timing_name.c_str()
        );

        // Dynamically allocate LED arrays for each lane
        fl::vector<fl::unique_ptr<fl::vector<CRGB>>> led_arrays;
        fl::vector<fl::ChannelConfig> tx_configs;

        for (fl::size i = 0; i < lane_sizes.size(); i++) {
            // Allocate LED array
            auto leds = fl::make_unique<fl::vector<CRGB>>(lane_sizes[i]);

            // Create channel config
            tx_configs.push_back(fl::ChannelConfig(
                pin_tx + i,  // Consecutive pins for multi-lane
                timing_config.timing,
                fl::span<CRGB>(leds->data(), leds->size()),
                RGB  // Default color order
            ));

            // Store array to keep it alive
            led_arrays.push_back(fl::move(leds));
        }

        // Create temporary RX channel if pinRx differs from default
        fl::shared_ptr<fl::RxDevice> rx_channel_to_use = *mpRxChannel;
        bool created_temp_rx = false;

        if (pin_rx != *mpPinRx && mRxFactory) {
            rx_channel_to_use = mRxFactory(pin_rx);
            if (!rx_channel_to_use) {
                response.set("success", false);
                response.set("error", "RxChannelCreationFailed");
                response.set("message", "Failed to create RX channel on custom pin");
                return response;
            }
            created_temp_rx = true;
        }

        // Create validation configuration
        fl::ValidationConfig validation_config(
            timing_config.timing,
            timing_config.name,
            tx_configs,
            driver_name.c_str(),
            rx_channel_to_use,
            mRxBuffer,
            lane_sizes[0],  // base_strip_size (used for logging)
            fl::RxDeviceType::RMT  // Default RX device type
        );

        // Run test with debug output suppressed
        int total_tests = 0;
        int passed_tests = 0;
        bool passed = false;

        {
            fl::ScopedLogDisable logGuard;  // Suppress FL_DBG/FL_PRINT during test

            // Run warm-up iteration (discard results)
            int warmup_total = 0, warmup_passed = 0;
            validateChipsetTiming(validation_config, warmup_total, warmup_passed);

            // Run actual test iterations
            for (int iter = 0; iter < iterations; iter++) {
                int iter_total = 0, iter_passed = 0;
                validateChipsetTiming(validation_config, iter_total, iter_passed);
                total_tests += iter_total;
                passed_tests += iter_passed;
            }

            passed = (total_tests > 0) && (passed_tests == total_tests);
        }  // logGuard destroyed, logging restored

        uint32_t duration_ms = millis() - start_ms;

        // ========== RESPONSE ==========

        response.set("success", true);
        response.set("passed", passed);
        response.set("totalTests", static_cast<int64_t>(total_tests));
        response.set("passedTests", static_cast<int64_t>(passed_tests));
        response.set("duration_ms", static_cast<int64_t>(duration_ms));
        response.set("driver", driver_name.c_str());
        response.set("laneCount", static_cast<int64_t>(lane_sizes.size()));

        // Add laneSizes array to response
        fl::Json sizes_response = fl::Json::array();
        for (int size : lane_sizes) {
            sizes_response.push_back(static_cast<int64_t>(size));
        }
        response.set("laneSizes", sizes_response);
        response.set("pattern", pattern.c_str());

        // Add first failure info if test failed
        if (!passed) {
            fl::Json failure = fl::Json::object();
            failure.set("pattern", pattern.c_str());
            failure.set("details", "Validation mismatch detected");
            response.set("firstFailure", failure);
        }

        return response;
    });

    // ========================================================================
    // Phase 4 Functions: Utility and Control
    // ========================================================================

    // Register "ping" function - health check with timestamp
    mRemote->registerFunctionWithReturn("ping", [this](const fl::Json& args) -> fl::Json {
        uint32_t now = millis();

        fl::Json response = fl::Json::object();
        response.set("success", true);
        response.set("message", "pong");
        response.set("timestamp", static_cast<int64_t>(now));
        response.set("uptimeMs", static_cast<int64_t>(now));
        return response;
    });

    // Register "testGpioConnection" function - test if TX and RX pins are electrically connected
    // This is a pre-test to diagnose hardware connection issues before running validation
    mRemote->registerFunctionWithReturn("testGpioConnection", [](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        // Validate args: expects [txPin, rxPin]
        if (!args.is_array() || args.size() != 2) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [txPin, rxPin]");
            return response;
        }

        if (!args[0].is_int() || !args[1].is_int()) {
            response.set("error", "InvalidPinType");
            response.set("message", "Pin numbers must be integers");
            return response;
        }

        int tx_pin = static_cast<int>(args[0].as_int().value());
        int rx_pin = static_cast<int>(args[1].as_int().value());

        FL_PRINT("[GPIO TEST] Testing connection: TX=" << tx_pin << " → RX=" << rx_pin);

        // Test 1: TX drives LOW, RX has pullup → RX should read LOW if connected
        pinMode(tx_pin, OUTPUT);
        pinMode(rx_pin, INPUT_PULLUP);
        digitalWrite(tx_pin, LOW);
        delay(5);  // Allow signal to settle
        int rx_when_tx_low = digitalRead(rx_pin);

        // Test 2: TX drives HIGH → RX should read HIGH if connected
        digitalWrite(tx_pin, HIGH);
        delay(5);  // Allow signal to settle
        int rx_when_tx_high = digitalRead(rx_pin);

        // Restore pins to safe state
        pinMode(tx_pin, INPUT);
        pinMode(rx_pin, INPUT);

        // Analyze results
        bool connected = (rx_when_tx_low == LOW) && (rx_when_tx_high == HIGH);

        response.set("txPin", static_cast<int64_t>(tx_pin));
        response.set("rxPin", static_cast<int64_t>(rx_pin));
        response.set("rxWhenTxLow", rx_when_tx_low == LOW ? "LOW" : "HIGH");
        response.set("rxWhenTxHigh", rx_when_tx_high == HIGH ? "HIGH" : "LOW");
        response.set("connected", connected);

        if (connected) {
            response.set("success", true);
            response.set("message", "GPIO pins are connected");
            FL_PRINT("[GPIO TEST] ✓ Pins connected: TX=" << tx_pin << " → RX=" << rx_pin);
        } else {
            response.set("success", false);
            if (rx_when_tx_low == HIGH && rx_when_tx_high == HIGH) {
                response.set("message", "RX pin stuck HIGH - no connection detected (check jumper wire)");
                FL_ERROR("[GPIO TEST] ✗ RX stuck HIGH - pins NOT connected");
            } else if (rx_when_tx_low == LOW && rx_when_tx_high == LOW) {
                response.set("message", "RX pin stuck LOW - possible short to ground");
                FL_ERROR("[GPIO TEST] ✗ RX stuck LOW - check for short");
            } else {
                response.set("message", "Unexpected GPIO behavior - check wiring");
                FL_ERROR("[GPIO TEST] ✗ Unexpected behavior");
            }
        }

        return response;
    });

    // ========================================================================
    // Pin Configuration RPC Functions (Dynamic TX/RX Pin Support)
    // ========================================================================

    // Register "getPins" function - query current and default pin configuration
    mRemote->registerFunctionWithReturn("getPins", [this](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();
        response.set("success", true);
        response.set("txPin", static_cast<int64_t>(*mpPinTx));
        response.set("rxPin", static_cast<int64_t>(*mpPinRx));

        fl::Json defaults = fl::Json::object();
        defaults.set("txPin", static_cast<int64_t>(mDefaultPinTx));
        defaults.set("rxPin", static_cast<int64_t>(mDefaultPinRx));
        response.set("defaults", defaults);

        #if defined(FL_IS_ESP_32S3)
            response.set("platform", "ESP32-S3");
        #elif defined(FL_IS_ESP_32S2)
            response.set("platform", "ESP32-S2");
        #elif defined(FL_IS_ESP_32C6)
            response.set("platform", "ESP32-C6");
        #elif defined(FL_IS_ESP_32C3)
            response.set("platform", "ESP32-C3");
        #else
            response.set("platform", "ESP32");
        #endif

        return response;
    });

    // Register "setTxPin" function - set TX pin (regenerates test cases)
    mRemote->registerFunctionWithReturn("setTxPin", [this](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        if (!args.is_array() || args.size() != 1 || !args[0].is_int()) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [pin: int]");
            return response;
        }

        int new_pin = static_cast<int>(args[0].as_int().value());

        // Validate pin range (ESP32 GPIO range)
        if (new_pin < 0 || new_pin > 48) {
            response.set("error", "InvalidPin");
            response.set("message", "Pin must be 0-48");
            return response;
        }

        int old_pin = *mpPinTx;
        *mpPinTx = new_pin;

        FL_PRINT("[RPC] setTxPin(" << new_pin << ") - TX pin changed from " << old_pin << " to " << new_pin);

        response.set("success", true);
        response.set("txPin", static_cast<int64_t>(new_pin));
        response.set("previousTxPin", static_cast<int64_t>(old_pin));
        return response;
    });

    // Register "setRxPin" function - set RX pin (recreates RX channel)
    mRemote->registerFunctionWithReturn("setRxPin", [this](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        if (!args.is_array() || args.size() != 1 || !args[0].is_int()) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [pin: int]");
            return response;
        }

        int new_pin = static_cast<int>(args[0].as_int().value());

        // Validate pin range (ESP32 GPIO range)
        if (new_pin < 0 || new_pin > 48) {
            response.set("error", "InvalidPin");
            response.set("message", "Pin must be 0-48");
            return response;
        }

        int old_pin = *mpPinRx;
        bool pin_changed = (new_pin != old_pin);
        bool rx_recreated = false;

        if (pin_changed) {
            *mpPinRx = new_pin;

            // Recreate RX channel with new pin
            FL_PRINT("[RPC] setRxPin(" << new_pin << ") - Recreating RX channel...");

            // Destroy old RX channel
            mpRxChannel->reset();

            // Create new RX channel on new pin using factory
            *mpRxChannel = mRxFactory(new_pin);

            if (*mpRxChannel) {
                FL_PRINT("[RPC] setRxPin - RX channel recreated on GPIO " << new_pin);
                rx_recreated = true;
            } else {
                FL_ERROR("[RPC] setRxPin - Failed to create RX channel on GPIO " << new_pin);
                response.set("error", "RxChannelCreationFailed");
                response.set("message", "Failed to create RX channel on new pin");
                // Restore old pin value
                *mpPinRx = old_pin;
                return response;
            }
        }

        response.set("success", true);
        response.set("rxPin", static_cast<int64_t>(new_pin));
        response.set("previousRxPin", static_cast<int64_t>(old_pin));
        response.set("rxChannelRecreated", rx_recreated);
        return response;
    });

    // Register "setPins" function - set both TX and RX pins atomically
    mRemote->registerFunctionWithReturn("setPins", [this](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        // Accept either {txPin, rxPin} object or [txPin, rxPin] array
        int new_tx_pin = -1;
        int new_rx_pin = -1;

        if (args.is_array() && args.size() == 1 && args[0].is_object()) {
            // Object form: [{txPin: int, rxPin: int}]
            fl::Json config = args[0];
            if (config.contains("txPin") && config["txPin"].is_int()) {
                new_tx_pin = static_cast<int>(config["txPin"].as_int().value());
            }
            if (config.contains("rxPin") && config["rxPin"].is_int()) {
                new_rx_pin = static_cast<int>(config["rxPin"].as_int().value());
            }
        } else if (args.is_array() && args.size() == 2) {
            // Array form: [txPin, rxPin]
            if (args[0].is_int()) {
                new_tx_pin = static_cast<int>(args[0].as_int().value());
            }
            if (args[1].is_int()) {
                new_rx_pin = static_cast<int>(args[1].as_int().value());
            }
        } else {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [{txPin, rxPin}] or [txPin, rxPin]");
            return response;
        }

        // Validate pin ranges
        if (new_tx_pin < 0 || new_tx_pin > 48) {
            response.set("error", "InvalidTxPin");
            response.set("message", "TX pin must be 0-48");
            return response;
        }
        if (new_rx_pin < 0 || new_rx_pin > 48) {
            response.set("error", "InvalidRxPin");
            response.set("message", "RX pin must be 0-48");
            return response;
        }

        int old_tx_pin = *mpPinTx;
        int old_rx_pin = *mpPinRx;
        bool rx_pin_changed = (new_rx_pin != old_rx_pin);
        bool rx_recreated = false;

        // Update TX pin
        *mpPinTx = new_tx_pin;

        // Update RX pin and recreate channel if changed
        if (rx_pin_changed) {
            *mpPinRx = new_rx_pin;

            FL_PRINT("[RPC] setPins - Recreating RX channel on GPIO " << new_rx_pin << "...");

            // Destroy old RX channel
            mpRxChannel->reset();

            // Create new RX channel using factory
            *mpRxChannel = mRxFactory(new_rx_pin);

            if (*mpRxChannel) {
                FL_PRINT("[RPC] setPins - RX channel recreated successfully");
                rx_recreated = true;
            } else {
                FL_ERROR("[RPC] setPins - Failed to create RX channel on GPIO " << new_rx_pin);
                // Rollback both pins
                *mpPinTx = old_tx_pin;
                *mpPinRx = old_rx_pin;
                response.set("error", "RxChannelCreationFailed");
                response.set("message", "Failed to create RX channel - pins restored to previous values");
                return response;
            }
        } else {
            *mpPinRx = new_rx_pin;
        }

        FL_PRINT("[RPC] setPins - TX: " << old_tx_pin << " → " << new_tx_pin
                << ", RX: " << old_rx_pin << " → " << new_rx_pin);

        response.set("success", true);
        response.set("txPin", static_cast<int64_t>(new_tx_pin));
        response.set("rxPin", static_cast<int64_t>(new_rx_pin));
        response.set("previousTxPin", static_cast<int64_t>(old_tx_pin));
        response.set("previousRxPin", static_cast<int64_t>(old_rx_pin));
        response.set("rxChannelRecreated", rx_recreated);
        return response;
    });

    // Register "findConnectedPins" function - probe adjacent pin pairs to find a jumper wire connection
    // This allows automatic discovery of TX/RX pin pair without requiring user to specify them
    mRemote->registerFunctionWithReturn("findConnectedPins", [this](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        // Parse optional arguments: [{startPin: int, endPin: int, autoApply: bool}]
        int start_pin = 0;
        int end_pin = 21;  // Default range: GPIO 0-21 covers most common pins
        bool auto_apply = true;  // If true, automatically apply found pins

        if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            fl::Json config = args[0];
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

        FL_PRINT("[PIN PROBE] Searching for connected pin pairs in range " << start_pin << "-" << end_pin);

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
        fl::Json tested_pairs = fl::Json::array();

        for (int pin = start_pin; pin < end_pin; pin++) {
            int tx_candidate = pin;
            int rx_candidate = pin + 1;

            // Skip certain pins known to cause issues
            #if defined(FL_IS_ESP_32S3)
            // Skip GPIO 0 as RX (boot mode issues), skip strapping pins
            if (rx_candidate == 0 || tx_candidate == 0) continue;
            if (tx_candidate >= 26 && tx_candidate <= 32) continue;  // Flash pins
            if (rx_candidate >= 26 && rx_candidate <= 32) continue;
            #endif

            fl::Json pair = fl::Json::object();
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
                FL_PRINT("[PIN PROBE] ✓ Found connected pair: TX=" << found_tx << " → RX=" << found_rx);
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
                FL_PRINT("[PIN PROBE] ✓ Found connected pair (reversed): TX=" << found_tx << " → RX=" << found_rx);
                break;
            }

            pair.set("connected", false);
            tested_pairs.push_back(pair);
        }

        response.set("testedPairs", tested_pairs);
        fl::Json search_range = fl::Json::array();
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
                int old_tx = *mpPinTx;
                int old_rx = *mpPinRx;
                bool rx_changed = (found_rx != old_rx);

                *mpPinTx = found_tx;
                *mpPinRx = found_rx;

                // Recreate RX channel if pin changed
                if (rx_changed && mRxFactory) {
                    mpRxChannel->reset();
                    *mpRxChannel = mRxFactory(found_rx);
                    if (!*mpRxChannel) {
                        FL_ERROR("[PIN PROBE] Failed to recreate RX channel on GPIO " << found_rx);
                        // Restore old values
                        *mpPinTx = old_tx;
                        *mpPinRx = old_rx;
                        response.set("error", "RxChannelCreationFailed");
                        response.set("autoApplied", false);
                        return response;
                    }
                }

                FL_PRINT("[PIN PROBE] Auto-applied pins: TX=" << found_tx << ", RX=" << found_rx);
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
    });

    // Register "help" function - list all RPC functions with descriptions
    mRemote->registerFunctionWithReturn("help", [this](const fl::Json& args) -> fl::Json {
        fl::Json functions = fl::Json::array();

        // Phase 1: Basic Control
        fl::Json start_fn = fl::Json::object();
        start_fn.set("name", "start");
        start_fn.set("phase", "Phase 1: Basic Control");
        start_fn.set("args", "[]");
        start_fn.set("returns", "void");
        start_fn.set("description", "Trigger test matrix execution");
        functions.push_back(start_fn);

        fl::Json status_fn = fl::Json::object();
        status_fn.set("name", "status");
        status_fn.set("phase", "Phase 1: Basic Control");
        status_fn.set("args", "[]");
        status_fn.set("returns", "{startReceived, testComplete, frameCounter, state}");
        status_fn.set("description", "Query current test state");
        functions.push_back(status_fn);

        fl::Json drivers_fn = fl::Json::object();
        drivers_fn.set("name", "drivers");
        drivers_fn.set("phase", "Phase 1: Basic Control");
        drivers_fn.set("args", "[]");
        drivers_fn.set("returns", "[{name, priority, enabled}, ...]");
        drivers_fn.set("description", "List available drivers");
        functions.push_back(drivers_fn);

        // Phase 2: Configuration
        fl::Json getConfig_fn = fl::Json::object();
        getConfig_fn.set("name", "getConfig");
        getConfig_fn.set("phase", "Phase 2: Configuration");
        getConfig_fn.set("args", "[]");
        getConfig_fn.set("returns", "{drivers, laneRange, stripSizes, totalTestCases}");
        getConfig_fn.set("description", "Query current test matrix configuration");
        functions.push_back(getConfig_fn);

        fl::Json setDrivers_fn = fl::Json::object();
        setDrivers_fn.set("name", "setDrivers");
        setDrivers_fn.set("phase", "Phase 2: Configuration");
        setDrivers_fn.set("args", "[driver1, driver2, ...]");
        setDrivers_fn.set("returns", "{success, driversSet, testCases}");
        setDrivers_fn.set("description", "Configure enabled drivers");
        functions.push_back(setDrivers_fn);

        fl::Json setLaneRange_fn = fl::Json::object();
        setLaneRange_fn.set("name", "setLaneRange");
        setLaneRange_fn.set("phase", "Phase 2: Configuration");
        setLaneRange_fn.set("args", "[minLanes, maxLanes]");
        setLaneRange_fn.set("returns", "{success, minLanes, maxLanes, testCases}");
        setLaneRange_fn.set("description", "Configure lane range (1-8)");
        functions.push_back(setLaneRange_fn);

        fl::Json setStripSizes_fn = fl::Json::object();
        setStripSizes_fn.set("name", "setStripSizes");
        setStripSizes_fn.set("phase", "Phase 2: Configuration");
        setStripSizes_fn.set("args", "[size] or [shortSize, longSize]");
        setStripSizes_fn.set("returns", "{success, stripSizesSet, testCases}");
        setStripSizes_fn.set("description", "Configure strip sizes");
        functions.push_back(setStripSizes_fn);

        // Phase 3: Selective Execution
        fl::Json runTestCase_fn = fl::Json::object();
        runTestCase_fn.set("name", "runTestCase");
        runTestCase_fn.set("phase", "Phase 3: Selective Execution");
        runTestCase_fn.set("args", "[testCaseIndex]");
        runTestCase_fn.set("returns", "{success, testCaseIndex, result}");
        runTestCase_fn.set("description", "Run single test case by index");
        functions.push_back(runTestCase_fn);

        fl::Json runDriver_fn = fl::Json::object();
        runDriver_fn.set("name", "runDriver");
        runDriver_fn.set("phase", "Phase 3: Selective Execution");
        runDriver_fn.set("args", "[driverName]");
        runDriver_fn.set("returns", "{success, driver, testsRun, results}");
        runDriver_fn.set("description", "Run all tests for specific driver");
        functions.push_back(runDriver_fn);

        fl::Json runAll_fn = fl::Json::object();
        runAll_fn.set("name", "runAll");
        runAll_fn.set("phase", "Phase 3: Selective Execution");
        runAll_fn.set("args", "[]");
        runAll_fn.set("returns", "{success, totalCases, passedCases, skippedCases, results}");
        runAll_fn.set("description", "Run full test matrix with JSON results");
        functions.push_back(runAll_fn);

        fl::Json getResults_fn = fl::Json::object();
        getResults_fn.set("name", "getResults");
        getResults_fn.set("phase", "Phase 3: Selective Execution");
        getResults_fn.set("args", "[]");
        getResults_fn.set("returns", "[{driver, lanes, stripSize, ...}, ...]");
        getResults_fn.set("description", "Return all test results");
        functions.push_back(getResults_fn);

        fl::Json getResult_fn = fl::Json::object();
        getResult_fn.set("name", "getResult");
        getResult_fn.set("phase", "Phase 3: Selective Execution");
        getResult_fn.set("args", "[testCaseIndex]");
        getResult_fn.set("returns", "{driver, lanes, stripSize, ...}");
        getResult_fn.set("description", "Return specific test case result");
        functions.push_back(getResult_fn);

        // Phase 4: Utility and Control
        fl::Json reset_fn = fl::Json::object();
        reset_fn.set("name", "reset");
        reset_fn.set("phase", "Phase 4: Utility");
        reset_fn.set("args", "[]");
        reset_fn.set("returns", "{success, message, testCasesCleared}");
        reset_fn.set("description", "Reset test state without device reboot");
        functions.push_back(reset_fn);

        fl::Json halt_fn = fl::Json::object();
        halt_fn.set("name", "halt");
        halt_fn.set("phase", "Phase 4: Utility");
        halt_fn.set("args", "[]");
        halt_fn.set("returns", "{success, message}");
        halt_fn.set("description", "Trigger sketch halt");
        functions.push_back(halt_fn);

        fl::Json ping_fn = fl::Json::object();
        ping_fn.set("name", "ping");
        ping_fn.set("phase", "Phase 4: Utility");
        ping_fn.set("args", "[]");
        ping_fn.set("returns", "{success, message, timestamp, uptimeMs, frameCounter}");
        ping_fn.set("description", "Health check with timestamp");
        functions.push_back(ping_fn);

        // Phase 5: Pin Configuration
        fl::Json getPins_fn = fl::Json::object();
        getPins_fn.set("name", "getPins");
        getPins_fn.set("phase", "Phase 5: Pin Configuration");
        getPins_fn.set("args", "[]");
        getPins_fn.set("returns", "{txPin, rxPin, defaults: {txPin, rxPin}, platform}");
        getPins_fn.set("description", "Query current and default pin configuration");
        functions.push_back(getPins_fn);

        fl::Json setTxPin_fn = fl::Json::object();
        setTxPin_fn.set("name", "setTxPin");
        setTxPin_fn.set("phase", "Phase 5: Pin Configuration");
        setTxPin_fn.set("args", "[pin]");
        setTxPin_fn.set("returns", "{success, txPin, previousTxPin, testCases}");
        setTxPin_fn.set("description", "Set TX pin (regenerates test cases)");
        functions.push_back(setTxPin_fn);

        fl::Json setRxPin_fn = fl::Json::object();
        setRxPin_fn.set("name", "setRxPin");
        setRxPin_fn.set("phase", "Phase 5: Pin Configuration");
        setRxPin_fn.set("args", "[pin]");
        setRxPin_fn.set("returns", "{success, rxPin, previousRxPin, rxChannelRecreated}");
        setRxPin_fn.set("description", "Set RX pin (recreates RX channel)");
        functions.push_back(setRxPin_fn);

        fl::Json setPins_fn = fl::Json::object();
        setPins_fn.set("name", "setPins");
        setPins_fn.set("phase", "Phase 5: Pin Configuration");
        setPins_fn.set("args", "[{txPin, rxPin}] or [txPin, rxPin]");
        setPins_fn.set("returns", "{success, txPin, rxPin, rxChannelRecreated, testCases}");
        setPins_fn.set("description", "Set both TX and RX pins atomically");
        functions.push_back(setPins_fn);

        fl::Json findConnectedPins_fn = fl::Json::object();
        findConnectedPins_fn.set("name", "findConnectedPins");
        findConnectedPins_fn.set("phase", "Phase 5: Pin Configuration");
        findConnectedPins_fn.set("args", "[{startPin, endPin, autoApply}] (all optional)");
        findConnectedPins_fn.set("returns", "{success, found, txPin, rxPin, autoApplied, testedPairs}");
        findConnectedPins_fn.set("description", "Probe adjacent pin pairs to find jumper wire connection");
        functions.push_back(findConnectedPins_fn);

        fl::Json help_fn = fl::Json::object();
        help_fn.set("name", "help");
        help_fn.set("phase", "Phase 4: Utility");
        help_fn.set("args", "[]");
        help_fn.set("returns", "[{name, phase, args, returns, description}, ...]");
        help_fn.set("description", "List all RPC functions with descriptions");
        functions.push_back(help_fn);

        fl::Json testSimd_fn = fl::Json::object();
        testSimd_fn.set("name", "testSimd");
        testSimd_fn.set("phase", "Phase 4: Utility");
        testSimd_fn.set("args", "[]");
        testSimd_fn.set("returns", "{success, passed, message}");
        testSimd_fn.set("description", "Test SIMD operations (add_sat_u8_16)");
        functions.push_back(testSimd_fn);

        fl::Json response = fl::Json::object();
        response.set("success", true);
        response.set("totalFunctions", static_cast<int64_t>(22));
        response.set("functions", functions);
        return response;
    });

    // Register "testSimd" function - test SIMD operations
    mRemote->registerFunctionWithReturn("testSimd", [](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        // Test data: 16 bytes each
        alignas(16) uint8_t a[16] = {200, 200, 200, 200, 200, 200, 200, 200,
                                      100, 100, 100, 100, 100, 100, 100, 100};
        alignas(16) uint8_t b[16] = {100, 100, 100, 100, 100, 100, 100, 100,
                                      200, 200, 200, 200, 200, 200, 200, 200};
        alignas(16) uint8_t result[16] = {0};

        // Expected: saturating add clamps at 255
        // First 8: 200+100=255 (clamped), Last 8: 100+200=255 (clamped)
        uint8_t expected[16] = {255, 255, 255, 255, 255, 255, 255, 255,
                                255, 255, 255, 255, 255, 255, 255, 255};

        // Perform SIMD saturating add
        fl::simd::simd_u8x16 va = fl::simd::load_u8_16(a);
        fl::simd::simd_u8x16 vb = fl::simd::load_u8_16(b);
        fl::simd::simd_u8x16 vr = fl::simd::add_sat_u8_16(va, vb);
        fl::simd::store_u8_16(result, vr);

        // Verify result
        bool passed = true;
        for (int i = 0; i < 16; i++) {
            if (result[i] != expected[i]) {
                passed = false;
                break;
            }
        }

        response.set("success", true);
        response.set("passed", passed);
        if (passed) {
            response.set("message", "SIMD add_sat_u8_16 test passed");
        } else {
            response.set("message", "SIMD add_sat_u8_16 test FAILED");
            // Include actual vs expected for debugging
            fl::Json actual = fl::Json::array();
            fl::Json exp = fl::Json::array();
            for (int i = 0; i < 16; i++) {
                actual.push_back(static_cast<int64_t>(result[i]));
                exp.push_back(static_cast<int64_t>(expected[i]));
            }
            response.set("actual", actual);
            response.set("expected", exp);
        }
        return response;
    });
}

void ValidationRemoteControl::tick(uint32_t current_millis) {
    if (mRemote) {
        mRemote->tick(current_millis);
    }
}

bool ValidationRemoteControl::processSerialInput() {
    if (!mRemote) {
        return false;  // Not initialized yet
    }

    // Read any available serial data
    while (Serial.available() > 0) {
        String _input = Serial.readStringUntil('\n');
        fl::string input = _input.c_str();
        input.trim();

        // JSON RPC command (starts with '{')
        if (!input.empty() && input[0] == '{') {
            fl::Json result;
            auto err = mRemote->processRpc(input, result);

            FL_PRINT("[RPC] processRpc() returned, error code: " << static_cast<int>(err));
            FL_PRINT("[RPC] result.has_value(): " << result.has_value());

            if (err == fl::Remote::Error::None) {
                // If function returned a value, print it
                if (result.has_value()) {
                    FL_PRINT("[RPC] About to printJson...");
                    printJsonRaw(result);
                    FL_PRINT("[RPC] printJson() completed");
                } else {
                    FL_WARN("[RPC] Function executed but result.has_value() is false");
                }
            } else {
                // Print error response
                fl::Json errorObj = fl::Json::object();
                switch (err) {
                    case fl::Remote::Error::InvalidJson:
                        errorObj.set("error", "InvalidJson");
                        errorObj.set("message", "Failed to parse JSON");
                        break;
                    case fl::Remote::Error::MissingFunction:
                        errorObj.set("error", "MissingFunction");
                        errorObj.set("message", "Missing 'function' field in JSON");
                        break;
                    case fl::Remote::Error::UnknownFunction:
                        errorObj.set("error", "UnknownFunction");
                        errorObj.set("message", "Function not registered");
                        break;
                    case fl::Remote::Error::InvalidTimestamp:
                        errorObj.set("error", "InvalidTimestamp");
                        errorObj.set("message", "Invalid timestamp type");
                        break;
                    default:
                        errorObj.set("error", "Unknown");
                        errorObj.set("message", "Unknown error");
                        break;
                }
                printJsonRaw(errorObj);
            }
        }
    }

    return false;
}
