// System control RPC bindings: device status, debug toggles, driver
// inventory, RPC ack/health (ping, testNoSerial), and the user-facing
// runSingleTest/runParallelTest dispatchers. "System" = device control,
// not feature exercise (those live in the math/network/async/driver bins).
// Extracted from AutoResearchRemote.cpp as part of #3132 / meta #3127.

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
#include "AutoResearchAnimartrixBench.h"
#include "AutoResearchWave8Expand.h"
#include "AutoResearchParlioEncode.h"
#include "AutoResearchTimingDrift.h"
#include "AutoResearchParlioStream.h"
#include "fl/chipsets/spi.h"
#include "fl/channels/bus_info.json.h"
#include "fl/channels/config.h"
#include <Arduino.h>

#include "fl/net/ble.h"

#include "fl/codec/h264.h"
#include "fl/codec/mp4_parser.h"
#include "fl/stl/detail/memory_file_handle.h"
#include "fl/fx/frame.h"

namespace {

fl::json autoResearchDeviceJson(const fl::string& name) {
    if (name == "RMT") return fl::deviceJson<fl::Bus::RMT>();
    if (name == "SPI" || name == "SPI_UNIFIED") return fl::deviceJson<fl::Bus::SPI>();
    if (name == "UART" || name == "LPUART") return fl::deviceJson<fl::Bus::UART>();
    if (name == "BIT_BANG" || name == "STUB") return fl::deviceJson<fl::Bus::BIT_BANG>();

    if (name == "FLEX_IO" || name == "LCD_RGB") {
        return fl::deviceJson<fl::Bus::FLEX_IO, 1>();
    }
    if (name == "OBJECT_FLED" || name == "PARLIO" || name == "LCD_SPI" ||
        name == "LCD_CLOCKLESS" || name == "I2S" || name == "I2S_SPI") {
        return fl::deviceJson<fl::Bus::FLEX_IO, 0>();
    }

    fl::json info = fl::json::object();
    info.set("bus", static_cast<int64_t>(0));
    info.set("bus_name", name.c_str());
    info.set("vendor_name", name.c_str());
    info.set("device_name", name.c_str());
    info.set("which", static_cast<int64_t>(0));
    info.set("is_noop", false);
    info.set("notes", "registered runtime driver");
    fl::json runtime = fl::json::object();
    runtime.set("available", true);
    runtime.set("initialized", false);
    runtime.set("ready", true);
    runtime.set("busy", false);
    runtime.set("error", "");
    info.set("runtime", runtime);
    return info;
}

}  // namespace


void AutoResearchRemoteControl::bindSystemMethods(fl::Remote& remote) {

    // Register "status" function - device readiness check
    remote.bind("status", [this](const fl::json& args) -> fl::json {
        fl::json status = fl::json::object();
        status.set("ready", true);
        status.set("pinTx", static_cast<int64_t>(mState->pin_tx));
        status.set("pinRx", static_cast<int64_t>(mState->pin_rx));
        return status;
    });

    // NOTE: getSchema is no longer needed - use built-in "rpc.discover" instead
    // The rpc.discover method is automatically available via Remote->Rpc and returns
    // the full OpenRPC schema. Our custom getSchema was causing stack overflow on ESP32-C6.

    // Register "debugTest" function - test RPC argument passing
    remote.bind("debugTest", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("received", args);
        return response;
    });

    // Register "deliberateHang" - force an infinite loop with interrupts
    // disabled to validate the unified Watchdog implementation (#2731).
    // The watchdog should fire within its configured timeout, reset the
    // device, and let the bootloader become reachable again.
    // Returns success BEFORE hanging so the caller sees the ACK; the hang
    // begins in the next iteration of the loop.
    remote.bind("deliberateHang", [this](const fl::json& args) -> fl::json {
        (void)args;
        mState->deliberate_hang_requested = true;
        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("message", "device will hang after RPC returns; watchdog should reset within configured timeout");
        response.set("hangDelayMs", static_cast<int64_t>(200));
        response.set("watchdogExpected", true);
        return response;
    });

    // Register "drivers" function - list available drivers
    remote.bind("drivers", [this](const fl::json& args) -> fl::json {
        fl::json drivers = fl::json::array();
        for (fl::size i = 0; i < mState->drivers_available.size(); i++) {
            fl::json driver = fl::json::object();
            driver.set("name", mState->drivers_available[i].name.c_str());
            driver.set("priority", static_cast<int64_t>(mState->drivers_available[i].priority));
            driver.set("enabled", mState->drivers_available[i].enabled);
            fl::json device = autoResearchDeviceJson(mState->drivers_available[i].name);
            driver.set("bus_name", device["bus_name"]);
            driver.set("vendor_name", device["vendor_name"]);
            driver.set("device_name", device["device_name"]);
            driver.set("which", device["which"]);
            driver.set("is_noop", device["is_noop"]);
            driver.set("runtime", device["runtime"]);
            drivers.push_back(driver);
        }
        return drivers;
    });

    // Returns: {success, passed, totalTests, passedTests, duration_ms, driver,
    //          laneCount, laneSizes, pattern, firstFailure?}
    // ASYNC: Sends ACK immediately, final response sent via sendAsyncResponse()
    // NOTE: runSingleTestImpl() may return early (error cases) without calling
    // sendAsyncResponse(). This wrapper ensures a response is ALWAYS sent so
    // the Python client never times out waiting 120s for a missing response.
    remote.bind("runSingleTest", [this](const fl::json& args) -> fl::json {
        fl::json result = this->runSingleTestImpl(args);
        // If runSingleTestImpl returned a non-null response, it exited early without
        // calling sendAsyncResponse(). Send it now so the client gets a response.
        if (!result.is_null()) {
            mRemote->sendAsyncResponse("runSingleTest", result);
        }
        return fl::json(nullptr);
    }, fl::RpcMode::ASYNC);

    // Register "runParallelTest" - test multiple drivers simultaneously
    // Args: {drivers: [{driver: "PARLIO", laneSizes: [100]}, {driver: "LCD_RGB", laneSizes: [100]}],
    //         pattern?: "MSB_LSB_A", iterations?: 1, timing?: "WS2812B-V5"}
    // Returns: {success, passed, duration_ms, show_duration_us, drivers: [...],
    //           rx_validation_attempted, rx_validation_passed}
    remote.bind("runParallelTest", [this](const fl::json& args) -> fl::json {
        fl::json result = this->runParallelTestImpl(args);
        if (!result.is_null()) {
            mRemote->sendAsyncResponse("runParallelTest", result);
        }
        return fl::json(nullptr);
    }, fl::RpcMode::ASYNC);

    // ========================================================================
    // Phase 4 Functions: Utility and Control
    // ========================================================================

    // Register "ping" function - health check with timestamp
    remote.bind("ping", [this](const fl::json& args) -> fl::json {
        uint32_t now = millis();

        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("message", "pong");
        response.set("timestamp", static_cast<int64_t>(now));
        response.set("uptimeMs", static_cast<int64_t>(now));
        return response;
    });

    // TEST: Simple RPC without Serial to verify task context works
    remote.bind("testNoSerial", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("message", "RPC works from task context");
        response.set("serial_safe", false);
        return response;
    });
}

#endif // !(FASTLED_AUTORESEARCH_LOW_MEMORY)
