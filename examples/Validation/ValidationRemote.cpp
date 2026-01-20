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

// Forward declarations
fl::vector<fl::TestCaseConfig> generateTestCases(
    const fl::TestMatrixConfig& matrix,
    int pin_tx);

// Forward declaration for runSingleTestCase (defined in Validation.ino)
void runSingleTestCase(
    fl::TestCaseConfig& test_case,
    fl::TestCaseResult& test_result,
    const fl::NamedTimingConfig& timing_config,
    fl::shared_ptr<fl::RxDevice> rx_channel,
    fl::span<uint8_t> rx_buffer);

ValidationRemoteControl::ValidationRemoteControl()
    : mRemote(fl::make_unique<fl::Remote>())
    , mpDriversAvailable(nullptr)
    , mpTestMatrix(nullptr)
    , mpTestCases(nullptr)
    , mpTestResults(nullptr)
    , mpStartCommandReceived(nullptr)
    , mpTestMatrixComplete(nullptr)
    , mpFrameCounter(nullptr)
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
    RxDeviceFactory rx_factory) {

    // Store references to external state
    mpDriversAvailable = &drivers_available;
    mpTestMatrix = &test_matrix;
    mpTestCases = &test_cases;
    mpTestResults = &test_results;
    mpStartCommandReceived = &start_command_received;
    mpTestMatrixComplete = &test_matrix_complete;
    mpFrameCounter = &frame_counter;
    mpRxChannel = &rx_channel;  // Pointer to shared_ptr for RX recreation
    mRxBuffer = rx_buffer;
    mpPinTx = &pin_tx;
    mpPinRx = &pin_rx;
    mDefaultPinTx = default_pin_tx;
    mDefaultPinRx = default_pin_rx;
    mRxFactory = rx_factory;

    // Helper to serialize test results
    auto serializeTestResult = [](const fl::TestCaseResult& result) -> fl::Json {
        fl::Json obj = fl::Json::object();
        obj.set("driver", result.driver_name);
        obj.set("lanes", static_cast<int64_t>(result.lane_count));
        obj.set("stripSize", static_cast<int64_t>(result.base_strip_size));
        obj.set("totalTests", static_cast<int64_t>(result.total_tests));
        obj.set("passedTests", static_cast<int64_t>(result.passed_tests));
        obj.set("passed", result.allPassed());
        obj.set("skipped", result.skipped);
        return obj;
    };

    // Helper to regenerate test cases
    auto regenerateTestCasesLocal = [this]() {
        FL_PRINT("[REGEN] Regenerating test cases from modified configuration");

        // Rebuild test cases from current test_matrix
        *mpTestCases = generateTestCases(*mpTestMatrix, *mpPinTx);  // Use configured TX pin

        // Clear and rebuild test results to match new test cases
        mpTestResults->clear();
        for (fl::size i = 0; i < mpTestCases->size(); i++) {
            const auto& test_case = (*mpTestCases)[i];
            mpTestResults->push_back(fl::TestCaseResult(
                test_case.driver_name.c_str(),
                test_case.lane_count,
                test_case.base_strip_size
            ));
        }

        FL_PRINT("[REGEN] Generated " << mpTestCases->size() << " test case(s)");
    };

    // Register "start" function - triggers test matrix execution
    // UPGRADED: Using typed method() API for simple void function
    mRemote->method("start", [this]() {
        FL_PRINT("[RPC] start() - Triggering test matrix execution");
        *mpStartCommandReceived = true;
    });

    // Register "status" function - query current test state
    mRemote->registerFunctionWithReturn("status", [this](const fl::Json& args) -> fl::Json {
        fl::Json status = fl::Json::object();
        status.set("startReceived", *mpStartCommandReceived);
        status.set("testComplete", *mpTestMatrixComplete);
        status.set("frameCounter", static_cast<int64_t>(*mpFrameCounter));
        if (*mpTestMatrixComplete) {
            status.set("state", "complete");
        } else if (*mpStartCommandReceived) {
            status.set("state", "running");
        } else {
            status.set("state", "idle");
        }
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

    // Register "getConfig" function - query current test matrix configuration
    mRemote->registerFunctionWithReturn("getConfig", [this](const fl::Json& args) -> fl::Json {
        fl::Json config = fl::Json::object();

        // Drivers array
        fl::Json drivers_array = fl::Json::array();
        for (fl::size i = 0; i < mpTestMatrix->enabled_drivers.size(); i++) {
            drivers_array.push_back(mpTestMatrix->enabled_drivers[i].c_str());
        }
        config.set("drivers", drivers_array);

        // Lane range
        fl::Json lane_range = fl::Json::array();
        lane_range.push_back(static_cast<int64_t>(mpTestMatrix->min_lanes));
        lane_range.push_back(static_cast<int64_t>(mpTestMatrix->max_lanes));
        config.set("laneRange", lane_range);

        // Strip sizes
        fl::Json strip_sizes = fl::Json::array();
        if (mpTestMatrix->test_small_strips) {
            strip_sizes.push_back(static_cast<int64_t>(SHORT_STRIP_SIZE));
        }
        if (mpTestMatrix->test_large_strips) {
            strip_sizes.push_back(static_cast<int64_t>(LONG_STRIP_SIZE));
        }
        config.set("stripSizes", strip_sizes);

        // Total test cases
        config.set("totalTestCases", static_cast<int64_t>(mpTestMatrix->getTotalTestCases()));

        return config;
    });

    // Register "setDrivers" function - configure enabled drivers
    mRemote->registerFunctionWithReturn("setDrivers", [this, regenerateTestCasesLocal](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        // Validate args is an array
        if (!args.is_array() || args.size() == 0) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected non-empty array of driver names");
            return response;
        }

        // Build new driver list and validate each name
        fl::vector<fl::string> new_drivers;
        for (fl::size i = 0; i < args.size(); i++) {
            if (!args[i].is_string()) {
                response.set("error", "InvalidDriverType");
                response.set("message", "All driver names must be strings");
                return response;
            }

            auto driver_name_opt = args[i].as_string();
            if (!driver_name_opt.has_value()) {
                response.set("error", "InvalidDriverType");
                response.set("message", "Failed to extract driver name as string");
                return response;
            }
            fl::string driver_name = driver_name_opt.value();

            // Validate driver exists in drivers_available
            bool found = false;
            for (fl::size j = 0; j < mpDriversAvailable->size(); j++) {
                if ((*mpDriversAvailable)[j].name == driver_name) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                response.set("error", "InvalidDriverName");
                fl::sstream msg;
                msg << "Driver '" << driver_name.c_str() << "' not found in available drivers";
                response.set("message", msg.str().c_str());
                return response;
            }

            new_drivers.push_back(driver_name);
        }

        // Update test matrix
        mpTestMatrix->enabled_drivers = new_drivers;
        regenerateTestCasesLocal();

        response.set("success", true);
        response.set("driversSet", static_cast<int64_t>(new_drivers.size()));
        response.set("testCases", static_cast<int64_t>(mpTestCases->size()));
        return response;
    });

    // Register "setLaneRange" function - configure lane range
    mRemote->registerFunctionWithReturn("setLaneRange", [this, regenerateTestCasesLocal](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        // Validate args is an array with 2 elements
        if (!args.is_array() || args.size() != 2) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected array with [min_lanes, max_lanes]");
            return response;
        }

        // Extract min and max
        if (!args[0].is_int() || !args[1].is_int()) {
            response.set("error", "InvalidLaneType");
            response.set("message", "Lane values must be integers");
            return response;
        }

        auto min_opt = args[0].as_int();
        auto max_opt = args[1].as_int();
        if (!min_opt.has_value() || !max_opt.has_value()) {
            response.set("error", "InvalidLaneType");
            response.set("message", "Failed to extract lane values as integers");
            return response;
        }

        int min_lanes = static_cast<int>(min_opt.value());
        int max_lanes = static_cast<int>(max_opt.value());

        // Validate range (1-8)
        if (min_lanes < 1 || min_lanes > 8 || max_lanes < 1 || max_lanes > 8) {
            response.set("error", "InvalidLaneRange");
            response.set("message", "Lane values must be between 1 and 8");
            return response;
        }

        if (min_lanes > max_lanes) {
            response.set("error", "InvalidLaneRange");
            response.set("message", "min_lanes must be <= max_lanes");
            return response;
        }

        // Update test matrix
        mpTestMatrix->min_lanes = min_lanes;
        mpTestMatrix->max_lanes = max_lanes;
        regenerateTestCasesLocal();

        response.set("success", true);
        response.set("minLanes", static_cast<int64_t>(min_lanes));
        response.set("maxLanes", static_cast<int64_t>(max_lanes));
        response.set("testCases", static_cast<int64_t>(mpTestCases->size()));
        return response;
    });

    // Register "setStripSizes" function - configure strip sizes
    mRemote->registerFunctionWithReturn("setStripSizes", [this, regenerateTestCasesLocal](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        // Validate args is an array with 1 or 2 elements
        if (!args.is_array() || args.size() == 0 || args.size() > 2) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected array with [size] or [short_size, long_size]");
            return response;
        }

        // Extract and validate sizes
        fl::vector<int> sizes;
        for (fl::size i = 0; i < args.size(); i++) {
            if (!args[i].is_int()) {
                response.set("error", "InvalidSizeType");
                response.set("message", "Strip sizes must be integers");
                return response;
            }

            auto size_opt = args[i].as_int();
            if (!size_opt.has_value()) {
                response.set("error", "InvalidSizeType");
                response.set("message", "Failed to extract size as integer");
                return response;
            }

            int size = static_cast<int>(size_opt.value());

            if (size <= 0) {
                response.set("error", "InvalidSize");
                response.set("message", "Strip sizes must be > 0");
                return response;
            }

            // Check against RX buffer capacity (approximate)
            // Each LED = ~32 symbols worst case
            const int RX_BUFFER_SIZE = mRxBuffer.size();
            if (size > (RX_BUFFER_SIZE / 32)) {
                response.set("error", "SizeTooLarge");
                fl::sstream msg;
                msg << "Strip size " << size << " exceeds RX buffer capacity (max ~"
                    << (RX_BUFFER_SIZE / 32) << " LEDs)";
                response.set("message", msg.str().c_str());
                return response;
            }

            sizes.push_back(size);
        }

        // Update test matrix based on size count
        if (sizes.size() == 1) {
            // Single size - use as both short and long
            mpTestMatrix->test_small_strips = true;
            mpTestMatrix->test_large_strips = false;
        } else {
            // Two sizes - short and long
            mpTestMatrix->test_small_strips = true;
            mpTestMatrix->test_large_strips = true;
        }

        regenerateTestCasesLocal();

        response.set("success", true);
        response.set("stripSizesSet", static_cast<int64_t>(sizes.size()));
        response.set("testCases", static_cast<int64_t>(mpTestCases->size()));
        return response;
    });

    // Register "runTestCase" function - run single test case by index
    mRemote->registerFunctionWithReturn("runTestCase", [this, serializeTestResult](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        // Validate args is an array with 1 element
        if (!args.is_array() || args.size() != 1) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected array with [testCaseIndex]");
            return response;
        }

        // Extract index
        if (!args[0].is_int()) {
            response.set("error", "InvalidIndexType");
            response.set("message", "Test case index must be an integer");
            return response;
        }

        auto index_opt = args[0].as_int();
        if (!index_opt.has_value()) {
            response.set("error", "InvalidIndexType");
            response.set("message", "Failed to extract index as integer");
            return response;
        }

        int64_t index = index_opt.value();

        // Validate index range
        if (index < 0 || static_cast<fl::size>(index) >= mpTestCases->size()) {
            response.set("error", "IndexOutOfRange");
            fl::sstream msg;
            msg << "Test case index " << index << " out of range (0-" << (mpTestCases->size() - 1) << ")";
            response.set("message", msg.str().c_str());
            return response;
        }

        fl::size idx = static_cast<fl::size>(index);

        // Run the test case (no debug print - case_start event provides info)

        // Get timing configuration (WS2812B-V5)
        fl::NamedTimingConfig timing_config(fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(), "WS2812B-V5");

        // Reset result for this test case
        (*mpTestResults)[idx].total_tests = 0;
        (*mpTestResults)[idx].passed_tests = 0;
        (*mpTestResults)[idx].skipped = false;

        // Emit case_start event
        {
            fl::Json data = fl::Json::object();
            data.set("caseIndex", index);
            data.set("driver", (*mpTestCases)[idx].driver_name.c_str());
            data.set("laneCount", static_cast<int64_t>((*mpTestCases)[idx].lane_count));
            printStreamRaw("case_start", data);
        }

        // Run the test case with debug output suppressed
        {
            fl::ScopedLogDisable logGuard;  // Suppress FL_DBG/FL_PRINT during test
            runSingleTestCase(
                (*mpTestCases)[idx],
                (*mpTestResults)[idx],
                timing_config,
                *mpRxChannel,
                mRxBuffer
            );
        }  // logGuard destroyed, logging restored

        // Emit case_result event
        {
            fl::Json result = serializeTestResult((*mpTestResults)[idx]);
            result.set("caseIndex", index);
            printStreamRaw("case_result", result);
        }

        // Return simple success response
        response.set("success", true);
        response.set("streamMode", true);
        return response;
    });

    // Register "runDriver" function - run all tests for specific driver with JSONL streaming
    mRemote->registerFunctionWithReturn("runDriver", [this, serializeTestResult](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        // Validate args is an array with 1 element
        if (!args.is_array() || args.size() != 1) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected array with [driverName]");
            return response;
        }

        // Extract driver name
        if (!args[0].is_string()) {
            response.set("error", "InvalidDriverType");
            response.set("message", "Driver name must be a string");
            return response;
        }

        auto driver_name_opt = args[0].as_string();
        if (!driver_name_opt.has_value()) {
            response.set("error", "InvalidDriverType");
            response.set("message", "Failed to extract driver name as string");
            return response;
        }
        fl::string driver_name = driver_name_opt.value();

        // Count matching test cases (no debug print - rundriver_start event provides info)
        int tests_to_run = 0;
        for (fl::size i = 0; i < mpTestCases->size(); i++) {
            if ((*mpTestCases)[i].driver_name == driver_name) {
                tests_to_run++;
            }
        }

        if (tests_to_run == 0) {
            response.set("error", "NoTestCases");
            fl::sstream msg;
            msg << "No test cases found for driver '" << driver_name.c_str() << "'";
            response.set("message", msg.str().c_str());
            return response;
        }

        // Emit rundriver_start event
        {
            fl::Json data = fl::Json::object();
            data.set("driver", driver_name.c_str());
            data.set("totalCases", static_cast<int64_t>(tests_to_run));
            printStreamRaw("rundriver_start", data);
        }

        // Get timing configuration (WS2812B-V5)
        fl::NamedTimingConfig timing_config(fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(), "WS2812B-V5");

        // Find and run all test cases for this driver with streaming
        int tests_run = 0;
        for (fl::size i = 0; i < mpTestCases->size(); i++) {
            if ((*mpTestCases)[i].driver_name == driver_name) {
                // Reset result for this test case
                (*mpTestResults)[i].total_tests = 0;
                (*mpTestResults)[i].passed_tests = 0;
                (*mpTestResults)[i].skipped = false;

                // Emit case_start event
                {
                    fl::Json data = fl::Json::object();
                    data.set("caseIndex", static_cast<int64_t>(i));
                    data.set("driver", driver_name.c_str());
                    data.set("laneCount", static_cast<int64_t>((*mpTestCases)[i].lane_count));
                    printStreamRaw("case_start", data);
                }

                // Run the test case with debug output suppressed
                {
                    fl::ScopedLogDisable logGuard;  // Suppress FL_DBG/FL_PRINT during test
                    runSingleTestCase(
                        (*mpTestCases)[i],
                        (*mpTestResults)[i],
                        timing_config,
                        *mpRxChannel,
                        mRxBuffer
                    );
                }  // logGuard destroyed, logging restored

                // Emit case_result event
                {
                    fl::Json result = serializeTestResult((*mpTestResults)[i]);
                    result.set("caseIndex", static_cast<int64_t>(i));
                    printStreamRaw("case_result", result);
                }

                tests_run++;
            }
        }

        // Emit rundriver_complete event
        {
            fl::Json data = fl::Json::object();
            data.set("driver", driver_name.c_str());
            data.set("testsRun", static_cast<int64_t>(tests_run));
            printStreamRaw("rundriver_complete", data);
        }

        // Return simple success response
        response.set("success", true);
        response.set("streamMode", true);
        return response;
    });

    // Register "runAll" function - run full test matrix with JSONL streaming
    mRemote->registerFunctionWithReturn("runAll", [this, serializeTestResult](const fl::Json& args) -> fl::Json {
        // Emit runall_start event (no debug print - event provides info)
        {
            fl::Json data = fl::Json::object();
            data.set("totalCases", static_cast<int64_t>(mpTestCases->size()));
            printStreamRaw("runall_start", data);
        }

        // Get timing configuration (WS2812B-V5)
        fl::NamedTimingConfig timing_config(fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(), "WS2812B-V5");

        // Reset all test results
        for (fl::size i = 0; i < mpTestResults->size(); i++) {
            (*mpTestResults)[i].total_tests = 0;
            (*mpTestResults)[i].passed_tests = 0;
            (*mpTestResults)[i].skipped = false;
        }

        // Run all test cases with streaming progress
        for (fl::size i = 0; i < mpTestCases->size(); i++) {
            // Emit case_start event
            {
                fl::Json data = fl::Json::object();
                data.set("caseIndex", static_cast<int64_t>(i));
                data.set("driver", (*mpTestCases)[i].driver_name.c_str());
                data.set("laneCount", static_cast<int64_t>((*mpTestCases)[i].lane_count));
                printStreamRaw("case_start", data);
            }

            // Run the test case with debug output suppressed
            {
                fl::ScopedLogDisable logGuard;  // Suppress FL_DBG/FL_PRINT during test
                runSingleTestCase(
                    (*mpTestCases)[i],
                    (*mpTestResults)[i],
                    timing_config,
                    *mpRxChannel,
                    mRxBuffer
                );
            }  // logGuard destroyed, logging restored

            // Emit case_result event with detailed result
            {
                fl::Json result = serializeTestResult((*mpTestResults)[i]);
                result.set("caseIndex", static_cast<int64_t>(i));
                printStreamRaw("case_result", result);
            }
        }

        // Calculate summary statistics
        int total_cases = mpTestResults->size();
        int passed_cases = 0;
        int skipped_cases = 0;
        for (fl::size i = 0; i < mpTestResults->size(); i++) {
            if ((*mpTestResults)[i].allPassed()) passed_cases++;
            if ((*mpTestResults)[i].skipped) skipped_cases++;
        }

        // Emit runall_complete event
        {
            fl::Json data = fl::Json::object();
            data.set("totalCases", static_cast<int64_t>(total_cases));
            data.set("passedCases", static_cast<int64_t>(passed_cases));
            data.set("skippedCases", static_cast<int64_t>(skipped_cases));
            printStreamRaw("runall_complete", data);
        }

        // Return simple success response
        fl::Json response = fl::Json::object();
        response.set("success", true);
        response.set("streamMode", true);
        return response;
    });

    // Register "getResults" function - stream all test results as JSONL
    mRemote->registerFunctionWithReturn("getResults", [this, serializeTestResult](const fl::Json& args) -> fl::Json {
        // Emit results_start event
        {
            fl::Json data = fl::Json::object();
            data.set("totalResults", static_cast<int64_t>(mpTestResults->size()));
            printStreamRaw("results_start", data);
        }

        // Stream each result
        for (fl::size i = 0; i < mpTestResults->size(); i++) {
            fl::Json result = serializeTestResult((*mpTestResults)[i]);
            result.set("resultIndex", static_cast<int64_t>(i));
            printStreamRaw("result_item", result);
        }

        // Emit results_complete event
        {
            fl::Json data = fl::Json::object();
            data.set("totalResults", static_cast<int64_t>(mpTestResults->size()));
            printStreamRaw("results_complete", data);
        }

        // Return simple success response
        fl::Json response = fl::Json::object();
        response.set("success", true);
        response.set("streamMode", true);
        return response;
    });

    // Register "getResult" function - return specific test case result by index
    mRemote->registerFunctionWithReturn("getResult", [this, serializeTestResult](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        // Validate args is an array with 1 element
        if (!args.is_array() || args.size() != 1) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected array with [testCaseIndex]");
            return response;
        }

        // Extract index
        if (!args[0].is_int()) {
            response.set("error", "InvalidIndexType");
            response.set("message", "Test case index must be an integer");
            return response;
        }

        auto index_opt = args[0].as_int();
        if (!index_opt.has_value()) {
            response.set("error", "InvalidIndexType");
            response.set("message", "Failed to extract index as integer");
            return response;
        }

        int64_t index = index_opt.value();

        // Validate index range
        if (index < 0 || static_cast<fl::size>(index) >= mpTestResults->size()) {
            response.set("error", "IndexOutOfRange");
            fl::sstream msg;
            msg << "Test case index " << index << " out of range (0-" << (mpTestResults->size() - 1) << ")";
            response.set("message", msg.str().c_str());
            return response;
        }

        // Stream the result
        fl::Json result = serializeTestResult((*mpTestResults)[static_cast<fl::size>(index)]);
        result.set("resultIndex", index);
        printStreamRaw("result_item", result);

        // Return simple success response
        response.set("success", true);
        response.set("streamMode", true);
        return response;
    });

    // ========================================================================
    // NEW Phase: Variable Lane Size Support (per LOOP.md design)
    // ========================================================================

    // Register "setLaneSizes" function - PRIMARY lane configuration
    mRemote->registerFunctionWithReturn("setLaneSizes", [this, regenerateTestCasesLocal](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        if (!args.is_array() || args.size() != 1 || !args[0].is_array()) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [[size1, size2, ...]] array of lane sizes");
            return response;
        }

        fl::Json sizes_array = args[0];

        // Validate lane count (1-8)
        if (sizes_array.size() < 1 || sizes_array.size() > 8) {
            response.set("error", "InvalidLaneCount");
            response.set("message", "Lane count must be 1-8");
            return response;
        }

        // Extract and validate sizes
        fl::vector<int> new_sizes;
        int total_leds = 0;
        const int max_total = mRxBuffer.size() / 32;  // RX buffer capacity

        for (fl::size i = 0; i < sizes_array.size(); i++) {
            if (!sizes_array[i].is_int()) {
                response.set("error", "InvalidSizeType");
                response.set("message", "All lane sizes must be integers");
                return response;
            }

            int size = static_cast<int>(sizes_array[i].as_int().value());
            if (size <= 0) {
                response.set("error", "InvalidSize");
                fl::sstream msg;
                msg << "Lane " << i << " size must be > 0, got " << size;
                response.set("message", msg.str().c_str());
                return response;
            }

            total_leds += size;
            new_sizes.push_back(size);
        }

        // Validate total fits in RX buffer
        if (total_leds > max_total) {
            response.set("error", "TotalSizeTooLarge");
            fl::sstream msg;
            msg << "Total LEDs (" << total_leds << ") exceeds RX buffer capacity (" << max_total << ")";
            response.set("message", msg.str().c_str());
            return response;
        }

        // Update configuration
        mpTestMatrix->lane_sizes = new_sizes;
        regenerateTestCasesLocal();

        // Build response with lane sizes array
        fl::Json sizes_response = fl::Json::array();
        for (int size : new_sizes) {
            sizes_response.push_back(static_cast<int64_t>(size));
        }

        response.set("success", true);
        response.set("laneSizes", sizes_response);
        response.set("laneCount", static_cast<int64_t>(new_sizes.size()));
        response.set("totalLeds", static_cast<int64_t>(total_leds));
        return response;
    });

    // Register "setLedCount" function - convenience wrapper for uniform sizes
    mRemote->registerFunctionWithReturn("setLedCount", [this, regenerateTestCasesLocal](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        if (!args.is_array() || args.size() != 1 || !args[0].is_int()) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [ledCount: int]");
            return response;
        }

        int led_count = static_cast<int>(args[0].as_int().value());

        // Validate against RX buffer capacity
        const int max_leds = mRxBuffer.size() / 32;
        if (led_count <= 0 || led_count > max_leds) {
            response.set("error", "InvalidLedCount");
            fl::sstream msg;
            msg << "LED count must be 1-" << max_leds;
            response.set("message", msg.str().c_str());
            return response;
        }

        // Update configuration
        mpTestMatrix->custom_led_count = led_count;

        // Build uniform lane_sizes array based on current lane count
        int lane_count = mpTestMatrix->getLaneCount();
        mpTestMatrix->lane_sizes.clear();
        for (int i = 0; i < lane_count; i++) {
            mpTestMatrix->lane_sizes.push_back(led_count);
        }

        regenerateTestCasesLocal();

        // Build response with lane sizes array
        fl::Json sizes_response = fl::Json::array();
        for (int size : mpTestMatrix->lane_sizes) {
            sizes_response.push_back(static_cast<int64_t>(size));
        }

        response.set("success", true);
        response.set("ledCount", static_cast<int64_t>(led_count));
        response.set("laneSizes", sizes_response);
        response.set("testCases", static_cast<int64_t>(mpTestCases->size()));
        return response;
    });

    // Register "setPattern" function
    mRemote->registerFunctionWithReturn("setPattern", [this](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        if (!args.is_array() || args.size() != 1 || !args[0].is_string()) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [pattern: string]");
            return response;
        }

        fl::string pattern = args[0].as_string().value();

        // For now, just store the pattern name - validation can be added later
        mpTestMatrix->test_pattern = pattern;

        response.set("success", true);
        response.set("pattern", pattern.c_str());
        response.set("description", "Pattern updated");
        return response;
    });

    // Register "setLaneCount" function - adjusts lane count while preserving LED count
    mRemote->registerFunctionWithReturn("setLaneCount", [this, regenerateTestCasesLocal](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        if (!args.is_array() || args.size() != 1 || !args[0].is_int()) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [laneCount: int]");
            return response;
        }

        int lane_count = static_cast<int>(args[0].as_int().value());

        if (lane_count < 1 || lane_count > 8) {
            response.set("error", "InvalidLaneCount");
            response.set("message", "Lane count must be 1-8");
            return response;
        }

        // Update configuration with uniform lane sizes
        int led_count_per_lane = mpTestMatrix->custom_led_count;
        mpTestMatrix->lane_sizes.clear();
        for (int i = 0; i < lane_count; i++) {
            mpTestMatrix->lane_sizes.push_back(led_count_per_lane);
        }

        regenerateTestCasesLocal();

        // Build response with lane sizes array
        fl::Json sizes_response = fl::Json::array();
        for (int size : mpTestMatrix->lane_sizes) {
            sizes_response.push_back(static_cast<int64_t>(size));
        }

        response.set("success", true);
        response.set("laneCount", static_cast<int64_t>(lane_count));
        response.set("laneSizes", sizes_response);
        response.set("testCases", static_cast<int64_t>(mpTestCases->size()));
        return response;
    });

    // Register "setTestIterations" function
    mRemote->registerFunctionWithReturn("setTestIterations", [this](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        if (!args.is_array() || args.size() != 1 || !args[0].is_int()) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [count: int]");
            return response;
        }

        int count = static_cast<int>(args[0].as_int().value());

        if (count <= 0) {
            response.set("error", "InvalidCount");
            response.set("message", "Iteration count must be > 0");
            return response;
        }

        mpTestMatrix->test_iterations = count;

        response.set("success", true);
        response.set("iterations", static_cast<int64_t>(count));
        return response;
    });

    // Register "configure" function - complete configuration in one call
    // Supports both laneSizes array (preferred) and ledCount/laneCount (legacy)
    mRemote->registerFunctionWithReturn("configure", [this, regenerateTestCasesLocal](const fl::Json& args) -> fl::Json {
        fl::Json response = fl::Json::object();

        if (!args.is_array() || args.size() != 1 || !args[0].is_object()) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [{driver, laneSizes, pattern, iterations}]");
            return response;
        }

        fl::Json config = args[0];

        // Apply driver
        if (config.contains("driver")) {
            fl::string driver = config["driver"].as_string().value();
            mpTestMatrix->enabled_drivers.clear();
            mpTestMatrix->enabled_drivers.push_back(driver);
        }

        // Apply lane sizes - PRIORITY: laneSizes takes precedence over ledCount/laneCount
        if (config.contains("laneSizes")) {
            // Per-lane sizing (preferred)
            fl::Json sizes_array = config["laneSizes"];
            if (sizes_array.is_array()) {
                fl::vector<int> new_sizes;
                for (fl::size i = 0; i < sizes_array.size(); i++) {
                    new_sizes.push_back(static_cast<int>(sizes_array[i].as_int().value()));
                }
                mpTestMatrix->lane_sizes = new_sizes;
            }
        } else if (config.contains("ledCount") && config.contains("laneCount")) {
            // Legacy: uniform sizing
            int led_count = static_cast<int>(config["ledCount"].as_int().value());
            int lane_count = static_cast<int>(config["laneCount"].as_int().value());
            mpTestMatrix->lane_sizes.clear();
            for (int i = 0; i < lane_count; i++) {
                mpTestMatrix->lane_sizes.push_back(led_count);
            }
            mpTestMatrix->custom_led_count = led_count;
        } else if (config.contains("ledCount")) {
            // Update LED count but keep lane count
            int led_count = static_cast<int>(config["ledCount"].as_int().value());
            mpTestMatrix->custom_led_count = led_count;
            // Update all lanes to new size
            for (fl::size i = 0; i < mpTestMatrix->lane_sizes.size(); i++) {
                mpTestMatrix->lane_sizes[i] = led_count;
            }
        }

        // Apply pattern
        if (config.contains("pattern")) {
            mpTestMatrix->test_pattern = config["pattern"].as_string().value();
        }

        // Apply iterations
        if (config.contains("iterations")) {
            mpTestMatrix->test_iterations = static_cast<int>(config["iterations"].as_int().value());
        }

        regenerateTestCasesLocal();

        // Build confirmed configuration
        fl::Json confirmed = fl::Json::object();
        if (!mpTestMatrix->enabled_drivers.empty()) {
            confirmed.set("driver", mpTestMatrix->enabled_drivers[0].c_str());
        }

        // Build laneSizes array
        fl::Json sizes_response = fl::Json::array();
        int total_leds = 0;
        for (int size : mpTestMatrix->lane_sizes) {
            sizes_response.push_back(static_cast<int64_t>(size));
            total_leds += size;
        }
        confirmed.set("laneSizes", sizes_response);
        confirmed.set("laneCount", static_cast<int64_t>(mpTestMatrix->lane_sizes.size()));
        confirmed.set("totalLeds", static_cast<int64_t>(total_leds));
        confirmed.set("pattern", mpTestMatrix->test_pattern.c_str());
        confirmed.set("iterations", static_cast<int64_t>(mpTestMatrix->test_iterations));
        confirmed.set("testCases", static_cast<int64_t>(mpTestCases->size()));

        // Emit config_complete event (JSONL streaming)
        printStreamRaw("config_complete", confirmed);

        // Return simple success response
        response.set("success", true);
        response.set("streamMode", true);
        return response;
    });

    // Register "runTest" function - run configured test and return full state
    mRemote->registerFunctionWithReturn("runTest", [this, serializeTestResult](const fl::Json& args) -> fl::Json {
        uint32_t start_ms = millis();

        // Emit test_start event (JSONL streaming)
        {
            fl::Json data = fl::Json::object();
            data.set("timestamp", static_cast<int64_t>(start_ms));
            data.set("testCases", static_cast<int64_t>(mpTestCases->size()));
            data.set("iterations", static_cast<int64_t>(mpTestMatrix->test_iterations));
            printStreamRaw("test_start", data);
        }

        // Get timing configuration (WS2812B-V5)
        fl::NamedTimingConfig timing_config(fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(), "WS2812B-V5");

        // Reset all test results
        for (fl::size i = 0; i < mpTestResults->size(); i++) {
            (*mpTestResults)[i].total_tests = 0;
            (*mpTestResults)[i].passed_tests = 0;
            (*mpTestResults)[i].skipped = false;
        }

        // Run test iterations
        for (int iter = 0; iter < mpTestMatrix->test_iterations; iter++) {
            // Emit iteration_start event
            {
                fl::Json data = fl::Json::object();
                data.set("iteration", static_cast<int64_t>(iter + 1));
                data.set("totalIterations", static_cast<int64_t>(mpTestMatrix->test_iterations));
                printStreamRaw("iteration_start", data);
            }

            for (fl::size i = 0; i < mpTestCases->size(); i++) {
                // Emit test_case_start event
                {
                    fl::Json data = fl::Json::object();
                    data.set("caseIndex", static_cast<int64_t>(i));
                    data.set("driver", (*mpTestCases)[i].driver_name.c_str());
                    data.set("laneCount", static_cast<int64_t>((*mpTestCases)[i].lane_count));
                    printStreamRaw("test_case_start", data);
                }

                runSingleTestCase(
                    (*mpTestCases)[i],
                    (*mpTestResults)[i],
                    timing_config,
                    *mpRxChannel,
                    mRxBuffer
                );

                // Emit test_case_result event
                {
                    fl::Json data = fl::Json::object();
                    data.set("caseIndex", static_cast<int64_t>(i));
                    data.set("passed", (*mpTestResults)[i].allPassed());
                    data.set("totalTests", static_cast<int64_t>((*mpTestResults)[i].total_tests));
                    data.set("passedTests", static_cast<int64_t>((*mpTestResults)[i].passed_tests));
                    printStreamRaw("test_case_result", data);
                }
            }

            // Emit iteration_complete event
            {
                fl::Json data = fl::Json::object();
                data.set("iteration", static_cast<int64_t>(iter + 1));
                printStreamRaw("iteration_complete", data);
            }
        }

        uint32_t end_ms = millis();

        // Calculate totals
        int total_tests = 0, passed_tests = 0;
        for (fl::size i = 0; i < mpTestResults->size(); i++) {
            total_tests += (*mpTestResults)[i].total_tests;
            passed_tests += (*mpTestResults)[i].passed_tests;
        }

        // Emit test_complete event (JSONL streaming)
        {
            fl::Json data = fl::Json::object();
            data.set("timestamp", static_cast<int64_t>(end_ms));
            data.set("totalTests", static_cast<int64_t>(total_tests));
            data.set("passedTests", static_cast<int64_t>(passed_tests));
            data.set("passed", passed_tests == total_tests);
            data.set("durationMs", static_cast<int64_t>(end_ms - start_ms));
            printStreamRaw("test_complete", data);
        }

        // Return minimal response (full details streamed above)
        fl::Json response = fl::Json::object();
        response.set("success", true);
        response.set("streamMode", true);
        return response;
    });

    // ========================================================================
    // Phase 5: Fast Single-Test RPC Command (runQuickTest)
    // ========================================================================
    // Optimized for Python orchestration - minimal overhead, no streaming
    // Args: {driver: str, ledCount: int, laneCount: int, pattern: int (optional)}
    // Returns: {success, returnCode, message, data: {passed, mismatches, durationMs}}
    mRemote->registerFunctionWithReturn("runQuickTest", [this](const fl::Json& args) -> fl::Json {
        // Validate args
        if (!args.is_array() || args.size() != 1 || !args[0].is_object()) {
            return makeResponse(false, ReturnCode::INVALID_ARGS,
                               "Expected [{driver, ledCount, laneCount, pattern?}]");
        }

        fl::Json config = args[0];

        // Extract required parameters
        if (!config.contains("driver") || !config["driver"].is_string()) {
            return makeResponse(false, ReturnCode::INVALID_ARGS, "Missing 'driver' (string)");
        }
        if (!config.contains("ledCount") || !config["ledCount"].is_int()) {
            return makeResponse(false, ReturnCode::INVALID_ARGS, "Missing 'ledCount' (int)");
        }
        if (!config.contains("laneCount") || !config["laneCount"].is_int()) {
            return makeResponse(false, ReturnCode::INVALID_ARGS, "Missing 'laneCount' (int)");
        }

        fl::string driver = config["driver"].as_string().value();
        int led_count = static_cast<int>(config["ledCount"].as_int().value());
        int lane_count = static_cast<int>(config["laneCount"].as_int().value());
        int pattern_id = config.contains("pattern") && config["pattern"].is_int()
                         ? static_cast<int>(config["pattern"].as_int().value())
                         : 0;  // Default to pattern A

        // Validate ranges
        if (led_count <= 0 || led_count > 3000) {
            return makeResponse(false, ReturnCode::INVALID_ARGS, "ledCount must be 1-3000");
        }
        if (lane_count <= 0 || lane_count > 8) {
            return makeResponse(false, ReturnCode::INVALID_ARGS, "laneCount must be 1-8");
        }
        if (pattern_id < 0 || pattern_id > 3) {
            return makeResponse(false, ReturnCode::INVALID_ARGS, "pattern must be 0-3");
        }

        // Configure test matrix for this single test
        mpTestMatrix->enabled_drivers.clear();
        mpTestMatrix->enabled_drivers.push_back(driver);
        mpTestMatrix->lane_sizes.clear();
        for (int i = 0; i < lane_count; i++) {
            mpTestMatrix->lane_sizes.push_back(led_count);
        }
        mpTestMatrix->test_iterations = 1;

        // Regenerate test cases (should produce exactly 1 test case)
        *mpTestCases = generateTestCases(*mpTestMatrix, *mpPinTx);

        if (mpTestCases->empty()) {
            return makeResponse(false, ReturnCode::HARDWARE_ERROR,
                               "Failed to generate test case - driver may not be available");
        }

        // Initialize result
        mpTestResults->clear();
        mpTestResults->push_back(fl::TestCaseResult(
            (*mpTestCases)[0].driver_name.c_str(),
            (*mpTestCases)[0].lane_count,
            (*mpTestCases)[0].base_strip_size
        ));

        // Get timing configuration
        fl::NamedTimingConfig timing_config(fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(), "WS2812B-V5");

        uint32_t start_ms = millis();

        // Run the single test case (silenced - no streaming)
        {
            fl::ScopedLogDisable logGuard;
            runSingleTestCase(
                (*mpTestCases)[0],
                (*mpTestResults)[0],
                timing_config,
                *mpRxChannel,
                mRxBuffer
            );
        }

        uint32_t end_ms = millis();

        // Build result data
        fl::Json data = fl::Json::object();
        bool passed = (*mpTestResults)[0].allPassed();
        int mismatches = (*mpTestResults)[0].total_tests - (*mpTestResults)[0].passed_tests;

        data.set("passed", passed);
        data.set("totalTests", static_cast<int64_t>((*mpTestResults)[0].total_tests));
        data.set("passedTests", static_cast<int64_t>((*mpTestResults)[0].passed_tests));
        data.set("mismatches", static_cast<int64_t>(mismatches));
        data.set("durationMs", static_cast<int64_t>(end_ms - start_ms));
        data.set("driver", driver.c_str());
        data.set("ledCount", static_cast<int64_t>(led_count));
        data.set("laneCount", static_cast<int64_t>(lane_count));
        data.set("pattern", static_cast<int64_t>(pattern_id));

        if (passed) {
            return makeResponse(true, ReturnCode::SUCCESS, "Test passed", data);
        } else {
            return makeResponse(false, ReturnCode::TEST_FAILED, "Test failed - mismatches detected", data);
        }
    });

    // Register "getState" function - query without running
    mRemote->registerFunctionWithReturn("getState", [this](const fl::Json& args) -> fl::Json {
        fl::Json state = fl::Json::object();

        if (*mpTestMatrixComplete) {
            state.set("phase", "complete");
        } else if (*mpStartCommandReceived) {
            state.set("phase", "running");
        } else {
            state.set("phase", "idle");
        }

        // Current config
        fl::Json config = fl::Json::object();
        if (!mpTestMatrix->enabled_drivers.empty()) {
            config.set("driver", mpTestMatrix->enabled_drivers[0].c_str());
        }

        // Build laneSizes array
        fl::Json sizes_response = fl::Json::array();
        int total_leds = 0;
        for (int size : mpTestMatrix->lane_sizes) {
            sizes_response.push_back(static_cast<int64_t>(size));
            total_leds += size;
        }
        config.set("laneSizes", sizes_response);
        config.set("laneCount", static_cast<int64_t>(mpTestMatrix->lane_sizes.size()));
        config.set("totalLeds", static_cast<int64_t>(total_leds));
        config.set("pattern", mpTestMatrix->test_pattern.c_str());
        config.set("iterations", static_cast<int64_t>(mpTestMatrix->test_iterations));
        state.set("config", config);

        // Last results (if any)
        if (mpTestResults->size() > 0 && (*mpTestResults)[0].total_tests > 0) {
            fl::Json last_results = fl::Json::object();
            int total = 0, passed = 0;
            for (fl::size i = 0; i < mpTestResults->size(); i++) {
                total += (*mpTestResults)[i].total_tests;
                passed += (*mpTestResults)[i].passed_tests;
            }
            last_results.set("totalTests", static_cast<int64_t>(total));
            last_results.set("passedTests", static_cast<int64_t>(passed));
            last_results.set("passed", passed == total);
            state.set("lastResults", last_results);
        }

        return state;
    });

    // ========================================================================
    // Phase 4 Functions: Utility and Control
    // ========================================================================

    // Register "reset" function - reset test state without device reboot
    mRemote->registerFunctionWithReturn("reset", [this](const fl::Json& args) -> fl::Json {
        FL_PRINT("[RPC] reset() - Resetting test state");

        // Reset start command flag
        *mpStartCommandReceived = false;

        // Reset completion flag
        *mpTestMatrixComplete = false;

        // Reset frame counter
        *mpFrameCounter = 0;

        // Reset all test results
        for (fl::size i = 0; i < mpTestResults->size(); i++) {
            (*mpTestResults)[i].total_tests = 0;
            (*mpTestResults)[i].passed_tests = 0;
            (*mpTestResults)[i].skipped = false;
        }

        fl::Json response = fl::Json::object();
        response.set("success", true);
        response.set("message", "Test state reset successfully");
        response.set("testCasesCleared", static_cast<int64_t>(mpTestResults->size()));
        return response;
    });

    // Register "halt" function - trigger sketch halt
    mRemote->registerFunctionWithReturn("halt", [this](const fl::Json& args) -> fl::Json {
        FL_PRINT("[RPC] halt() - Triggering sketch halt");

        // Mark test matrix as complete to trigger halt in loop()
        *mpTestMatrixComplete = true;

        fl::Json response = fl::Json::object();
        response.set("success", true);
        response.set("message", "Sketch halt triggered (will halt on next loop iteration)");
        return response;
    });

    // Register "ping" function - health check with timestamp
    mRemote->registerFunctionWithReturn("ping", [this](const fl::Json& args) -> fl::Json {
        uint32_t now = millis();

        fl::Json response = fl::Json::object();
        response.set("success", true);
        response.set("message", "pong");
        response.set("timestamp", static_cast<int64_t>(now));
        response.set("uptimeMs", static_cast<int64_t>(now));
        response.set("frameCounter", static_cast<int64_t>(*mpFrameCounter));
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
    mRemote->registerFunctionWithReturn("setTxPin", [this, regenerateTestCasesLocal](const fl::Json& args) -> fl::Json {
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

        // Regenerate test cases with new TX pin
        regenerateTestCasesLocal();

        FL_PRINT("[RPC] setTxPin(" << new_pin << ") - TX pin changed from " << old_pin << " to " << new_pin);

        response.set("success", true);
        response.set("txPin", static_cast<int64_t>(new_pin));
        response.set("previousTxPin", static_cast<int64_t>(old_pin));
        response.set("testCases", static_cast<int64_t>(mpTestCases->size()));
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
    mRemote->registerFunctionWithReturn("setPins", [this, regenerateTestCasesLocal](const fl::Json& args) -> fl::Json {
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

        // Regenerate test cases with new TX pin
        regenerateTestCasesLocal();

        FL_PRINT("[RPC] setPins - TX: " << old_tx_pin << " → " << new_tx_pin
                << ", RX: " << old_rx_pin << " → " << new_rx_pin);

        response.set("success", true);
        response.set("txPin", static_cast<int64_t>(new_tx_pin));
        response.set("rxPin", static_cast<int64_t>(new_rx_pin));
        response.set("previousTxPin", static_cast<int64_t>(old_tx_pin));
        response.set("previousRxPin", static_cast<int64_t>(old_rx_pin));
        response.set("rxChannelRecreated", rx_recreated);
        response.set("testCases", static_cast<int64_t>(mpTestCases->size()));
        return response;
    });

    // Register "findConnectedPins" function - probe adjacent pin pairs to find a jumper wire connection
    // This allows automatic discovery of TX/RX pin pair without requiring user to specify them
    mRemote->registerFunctionWithReturn("findConnectedPins", [this, regenerateTestCasesLocal](const fl::Json& args) -> fl::Json {
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

                // Regenerate test cases
                regenerateTestCasesLocal();

                FL_PRINT("[PIN PROBE] Auto-applied pins: TX=" << found_tx << ", RX=" << found_rx);
                response.set("autoApplied", true);
                response.set("previousTxPin", static_cast<int64_t>(old_tx));
                response.set("previousRxPin", static_cast<int64_t>(old_rx));
                response.set("testCases", static_cast<int64_t>(mpTestCases->size()));
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
    if (!mpStartCommandReceived || !mRemote) {
        return false;  // Not initialized yet
    }

    // Read any available serial data
    while (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();

        // Legacy "START" command
        if (input == "START") {
            *mpStartCommandReceived = true;
            FL_PRINT("\n[START] Received START command - beginning test matrix");
            return true;
        }
        // JSON RPC command (starts with '{')
        else if (input.length() > 0 && input[0] == '{') {
            fl::Json result;
            fl::string jsonStr(input.c_str());
            auto err = mRemote->processRpc(jsonStr, result);

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
