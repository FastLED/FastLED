// examples/Validation/ValidationRemote.cpp
//
// Remote RPC control system implementation for Validation sketch.

#include "ValidationRemote.h"
#include "ValidationConfig.h"
#include "Common.h"
#include "ValidationTest.h"
#include "ValidationHelpers.h"
#include "fl/stl/sstream.h"
#include "fl/stl/unique_ptr.h"
#include "fl/json.h"
#include <Arduino.h>

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
    , mpRxChannel()
    , mRxBuffer(nullptr, 0) {
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
    fl::span<uint8_t> rx_buffer) {

    // Store references to external state
    mpDriversAvailable = &drivers_available;
    mpTestMatrix = &test_matrix;
    mpTestCases = &test_cases;
    mpTestResults = &test_results;
    mpStartCommandReceived = &start_command_received;
    mpTestMatrixComplete = &test_matrix_complete;
    mpFrameCounter = &frame_counter;
    mpRxChannel = rx_channel;
    mRxBuffer = rx_buffer;

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
        *mpTestCases = generateTestCases(*mpTestMatrix, 0);  // PIN_TX will be updated per lane

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
    mRemote->registerFunction("start", [this](const fl::Json& args) {
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

        // Run the test case
        FL_PRINT("[RPC] runTestCase(" << index << ") - Running test case");

        // Get timing configuration (WS2812B-V5)
        fl::NamedTimingConfig timing_config(fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(), "WS2812B-V5");

        // Reset result for this test case
        (*mpTestResults)[idx].total_tests = 0;
        (*mpTestResults)[idx].passed_tests = 0;
        (*mpTestResults)[idx].skipped = false;

        // Run the test case
        runSingleTestCase(
            (*mpTestCases)[idx],
            (*mpTestResults)[idx],
            timing_config,
            mpRxChannel,
            mRxBuffer
        );

        // Return result
        response.set("success", true);
        response.set("testCaseIndex", index);
        response.set("result", serializeTestResult((*mpTestResults)[idx]));
        return response;
    });

    // Register "runDriver" function - run all tests for specific driver
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

        FL_PRINT("[RPC] runDriver('" << driver_name.c_str() << "') - Running all tests for driver");

        // Get timing configuration (WS2812B-V5)
        fl::NamedTimingConfig timing_config(fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(), "WS2812B-V5");

        // Find and run all test cases for this driver
        fl::Json results_array = fl::Json::array();
        int tests_run = 0;

        for (fl::size i = 0; i < mpTestCases->size(); i++) {
            if ((*mpTestCases)[i].driver_name == driver_name) {
                // Reset result for this test case
                (*mpTestResults)[i].total_tests = 0;
                (*mpTestResults)[i].passed_tests = 0;
                (*mpTestResults)[i].skipped = false;

                // Run the test case
                runSingleTestCase(
                    (*mpTestCases)[i],
                    (*mpTestResults)[i],
                    timing_config,
                    mpRxChannel,
                    mRxBuffer
                );

                // Add result to array
                results_array.push_back(serializeTestResult((*mpTestResults)[i]));
                tests_run++;
            }
        }

        if (tests_run == 0) {
            response.set("error", "NoTestCases");
            fl::sstream msg;
            msg << "No test cases found for driver '" << driver_name.c_str() << "'";
            response.set("message", msg.str().c_str());
            return response;
        }

        response.set("success", true);
        response.set("driver", driver_name.c_str());
        response.set("testsRun", static_cast<int64_t>(tests_run));
        response.set("results", results_array);
        return response;
    });

    // Register "runAll" function - run full test matrix (equivalent to "start" with JSON results)
    mRemote->registerFunctionWithReturn("runAll", [this, serializeTestResult](const fl::Json& args) -> fl::Json {
        FL_PRINT("[RPC] runAll() - Running full test matrix");

        // Get timing configuration (WS2812B-V5)
        fl::NamedTimingConfig timing_config(fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(), "WS2812B-V5");

        // Reset all test results
        for (fl::size i = 0; i < mpTestResults->size(); i++) {
            (*mpTestResults)[i].total_tests = 0;
            (*mpTestResults)[i].passed_tests = 0;
            (*mpTestResults)[i].skipped = false;
        }

        // Run all test cases
        for (fl::size i = 0; i < mpTestCases->size(); i++) {
            runSingleTestCase(
                (*mpTestCases)[i],
                (*mpTestResults)[i],
                timing_config,
                mpRxChannel,
                mRxBuffer
            );
        }

        // Serialize all results
        fl::Json results_array = fl::Json::array();
        for (fl::size i = 0; i < mpTestResults->size(); i++) {
            results_array.push_back(serializeTestResult((*mpTestResults)[i]));
        }

        // Calculate summary statistics
        int total_cases = mpTestResults->size();
        int passed_cases = 0;
        int skipped_cases = 0;
        for (fl::size i = 0; i < mpTestResults->size(); i++) {
            if ((*mpTestResults)[i].allPassed()) passed_cases++;
            if ((*mpTestResults)[i].skipped) skipped_cases++;
        }

        fl::Json response = fl::Json::object();
        response.set("success", true);
        response.set("totalCases", static_cast<int64_t>(total_cases));
        response.set("passedCases", static_cast<int64_t>(passed_cases));
        response.set("skippedCases", static_cast<int64_t>(skipped_cases));
        response.set("results", results_array);
        return response;
    });

    // Register "getResults" function - return all test results as JSON array
    mRemote->registerFunctionWithReturn("getResults", [this, serializeTestResult](const fl::Json& args) -> fl::Json {
        fl::Json results_array = fl::Json::array();
        for (fl::size i = 0; i < mpTestResults->size(); i++) {
            results_array.push_back(serializeTestResult((*mpTestResults)[i]));
        }
        return results_array;
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

        return serializeTestResult((*mpTestResults)[static_cast<fl::size>(index)]);
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

        fl::Json help_fn = fl::Json::object();
        help_fn.set("name", "help");
        help_fn.set("phase", "Phase 4: Utility");
        help_fn.set("args", "[]");
        help_fn.set("returns", "[{name, phase, args, returns, description}, ...]");
        help_fn.set("description", "List all RPC functions with descriptions");
        functions.push_back(help_fn);

        fl::Json response = fl::Json::object();
        response.set("success", true);
        response.set("totalFunctions", static_cast<int64_t>(16));
        response.set("functions", functions);
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

            if (err == fl::Remote::Error::None) {
                // If function returned a value, print it
                if (result.has_value()) {
                    fl::Remote::printJson(result);
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
                fl::Remote::printJson(errorObj);
            }
        }
    }

    return false;
}
