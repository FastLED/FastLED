// LED driver test/benchmark RPC bindings. Today: Teensy 4.x FlexIO RX
// validation (flexioRxBenchmark, flexioObjectFledTest, flexioRxLoopbackPing).
// Platform-specific driver tests for other MCUs land here in the future —
// the RPC layer is platform-agnostic; #if guards inside each binding gate
// the platform-specific bodies.
// Extracted from AutoResearchRemote.cpp as part of #3132 / meta #3127.

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
#include "AutoResearchParlioStream.h"
#include "AutoResearchLegacyAsync.h"
#include "AutoResearchLegacyAsyncErrors.h"
#include "AutoResearchTimingDrift.h"  // timingDriftTest RPC (#2994 repro)
#include "fl/chipsets/spi.h"
#include "fl/channels/config.h"
#include "fl/stl/span.h"
#include "fl/stl/singleton.h"
#include <Arduino.h>
#if defined(FL_IS_ESP32) // ok platform specific - fl::thread is synchronous on ESP32; this ABI regression requires competing FreeRTOS tasks
#include "freertos/FreeRTOS.h" // ok platform specific - exercise the ESP-IDF local-static guard with real tasks
#include "freertos/semphr.h" // ok platform specific - coordinate the two-task ABI regression without racy volatile counters
#include "freertos/task.h" // ok platform specific - fl::thread does not launch tasks on this target
#endif
#if defined(FL_IS_ESP32) // ok platform specific - the explicit ESP32 SPI host probe needs this chip's peripheral count
#include "platforms/esp/32/drivers/spi/spi_hw_base.h" // ok platform specific - FASTLED_ESP32_HAS_SPI3 rejects one-host chips
#endif

// #3428: FlexIO2 SPI mode bring-up smoke test (`flexioSpiSelfTest` RPC).
// The header is Teensy 4.x-gated, but #include is safe on any platform
// because all declarations sit inside `#if defined(FL_IS_TEENSY_4X)`.
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/flexio_spi_mode.h"

// #3428: ObjectFLED DMA-bit-banged SPI mode bring-up smoke test
// (`objectfledSpiSelfTest` RPC). Same Teensy 4.x-gating pattern.
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/objectfled_spi_mode.h"

#include "fl/net/ble.h"

#include "fl/codec/h264.h"
#include "fl/codec/mp4_parser.h"
#include "fl/stl/detail/memory_file_handle.h"
#include "fl/fx/frame.h"

namespace {

#if defined(FL_IS_ESP32) // ok platform specific - this regression checks ESP-IDF's process-wide C++ ABI guard implementation
class ProbeSemaphore {
public:
    ProbeSemaphore() = default;
    ~ProbeSemaphore() { reset(nullptr); }
    ProbeSemaphore(const ProbeSemaphore&) = delete;
    ProbeSemaphore& operator=(const ProbeSemaphore&) = delete;

    SemaphoreHandle_t get() const { return mHandle; } // ok platform specific - core synchronization cannot supply handles for the real FreeRTOS tasks in this SDK guard probe
    explicit operator bool() const { return mHandle != nullptr; }
    void reset(SemaphoreHandle_t handle) { // ok platform specific - core synchronization cannot own the SDK semaphore used by this real FreeRTOS guard probe
        if (mHandle) {
            vSemaphoreDelete(mHandle); // ok platform specific - core synchronization has no ownership API for the real FreeRTOS semaphore required by this SDK guard probe
        }
        mHandle = handle;
    }

private:
    SemaphoreHandle_t mHandle = nullptr; // ok platform specific - core synchronization cannot represent this real FreeRTOS semaphore used by the SDK guard probe
};

struct StaticInitProbe {
    fl::atomic<int> constructions{0};
    fl::atomic<int> contenders{0};
    fl::atomic<bool> overlapped{false};
    fl::atomic<TaskHandle_t> contender{nullptr}; // ok platform specific - the probe observes the SDK worker's actual blocked state
    ProbeSemaphore entered;
    ProbeSemaphore completed;
    bool used = false;
};

// Tasks retain this state until completion, including on timeout.
StaticInitProbe staticInitProbe;

struct SlowStatic {
    SlowStatic() {
        const bool owner = staticInitProbe.constructions.fetch_add(1) == 0;
        xSemaphoreGive(staticInitProbe.entered.get()); // ok platform specific - synchronize actual SDK tasks; fl::thread is not concurrent on ESP32
        if (!owner) {
            // Make duplicate initialization observable in the failing build.
            vTaskDelay(pdMS_TO_TICKS(200)); // ok platform specific - require an actual FreeRTOS-blocked duplicate initializer for observation
            return;
        }
        const TickType_t wait_start = xTaskGetTickCount(); // ok platform specific - measure the same scheduler clock used by the guard-state polling delay
        const TickType_t wait_ticks = pdMS_TO_TICKS(2000); // ok platform specific - bound the real SDK contention wait in scheduler ticks
        do {
            const TaskHandle_t contender = staticInitProbe.contender.load(fl::memory_order_acquire); // ok platform specific - retain the SDK task identity through the guard-state sample
            // Between arrival and completion, the worker can block only in
            // this guard or this constructor's delay. Finished workers suspend
            // rather than delete themselves, so the sampled handle stays valid.
            const bool blocked = contender && eTaskGetState(contender) == eBlocked; // ok platform specific - no core API observes ESP-IDF's guard waiter state
            if (staticInitProbe.contenders.load(fl::memory_order_acquire) == 2 && blocked) {
                staticInitProbe.overlapped.store(true, fl::memory_order_release);
                return;
            }
            vTaskDelay(1); // ok platform specific - yield to the actual SDK contender until guard entry is observed
        } while (static_cast<TickType_t>(xTaskGetTickCount() - wait_start) < wait_ticks); // ok platform specific - scheduler-tick subtraction handles tick-counter wrap
    }
};

SlowStatic& contendedStatic() {
    static SlowStatic instance;
    return instance;
}

void staticInitWorker(void*) {
    staticInitProbe.contenders.fetch_add(1);
    (void)contendedStatic();
    xSemaphoreGive(staticInitProbe.completed.get()); // ok platform specific - synchronize actual SDK tasks; fl::thread is not concurrent on ESP32
    // Retain the TCB until the RPC has sampled it and received both completions.
    for (;;) {
        vTaskSuspend(nullptr); // ok platform specific - keep the SDK task handle valid until the probe's explicit teardown
    }
}

fl::json staticInitTest() {
    fl::json result = fl::json::object();
    result.set("success", false);
    if (staticInitProbe.used) {
        result.set("error", "ResetBoardBeforeStaticInitTest");
        return result;
    }
    staticInitProbe.used = true;
    staticInitProbe.entered.reset(xSemaphoreCreateBinary()); // ok platform specific - synchronize actual SDK tasks; fl::thread is not concurrent on ESP32
    staticInitProbe.completed.reset(xSemaphoreCreateCounting(2, 0)); // ok platform specific - synchronize actual SDK tasks; fl::thread is not concurrent on ESP32
    if (!staticInitProbe.entered || !staticInitProbe.completed) {
        staticInitProbe.entered.reset(nullptr);
        staticInitProbe.completed.reset(nullptr);
        result.set("error", "SemaphoreAllocationFailed");
        return result;
    }
    TaskHandle_t owner_task = nullptr; // ok platform specific - this RPC owns the SDK worker until explicit teardown
    TaskHandle_t contender_task = nullptr; // ok platform specific - this RPC owns the SDK worker until explicit teardown
    const bool owner_created = xTaskCreate(staticInitWorker, "static-owner", 4096, // ok platform specific - fl::thread has no concurrent ESP32 implementation
                                         nullptr, 1, &owner_task) == pdPASS;
    const bool entered = owner_created &&
        xSemaphoreTake(staticInitProbe.entered.get(), pdMS_TO_TICKS(2000)) == pdTRUE; // ok platform specific - await the real SDK constructor before creating its contender
    const bool contender_created = entered &&
        xTaskCreate(staticInitWorker, "static-contender", 4096, // ok platform specific - fl::thread has no concurrent ESP32 implementation
                    nullptr, 1, &contender_task) == pdPASS;
    staticInitProbe.contender.store(contender_task, fl::memory_order_release);
    int completed = 0;
    const int tasks = (owner_created ? 1 : 0) + (contender_created ? 1 : 0);
    for (int i = 0; i < tasks; ++i) {
        if (xSemaphoreTake(staticInitProbe.completed.get(), pdMS_TO_TICKS(3000)) == pdTRUE) { // ok platform specific - join actual SDK workers without a concurrent fl::thread backend
            ++completed;
        }
    }
    bool workers_suspended = false;
    if (completed == tasks) {
        // On SMP, completion can wake this RPC before a worker reaches its
        // suspension. Wait a bounded time before deleting tasks or semaphores.
        for (int attempt = 0; attempt < 100; ++attempt) {
            const bool owner_suspended = !owner_task || eTaskGetState(owner_task) == eSuspended; // ok platform specific - no core API joins actual ESP32 FreeRTOS workers
            const bool contender_suspended = !contender_task || eTaskGetState(contender_task) == eSuspended; // ok platform specific - no core API joins actual ESP32 FreeRTOS workers
            if (owner_suspended && contender_suspended) {
                workers_suspended = true;
                break;
            }
            vTaskDelay(1); // ok platform specific - let the SDK workers quiesce before their handles are reclaimed
        }
    }
    const int constructions = staticInitProbe.constructions.load(fl::memory_order_acquire);
    const int contenders = staticInitProbe.contenders.load(fl::memory_order_acquire);
    const bool overlapped = staticInitProbe.overlapped.load(fl::memory_order_acquire);
    result.set("constructorEntered", entered);
    result.set("constructions", constructions);
    result.set("contenders", contenders);
    result.set("completed", completed);
    result.set("overlapped", overlapped);
    result.set("workersSuspended", workers_suspended);
    result.set("success", entered && completed == 2 && contenders == 2 &&
                          overlapped && constructions == 1 && workers_suspended);
    // Keep handles alive unless every worker is suspended. This probe runs
    // once per boot, so a timeout cannot accumulate allocations.
    if (workers_suspended) {
        staticInitProbe.contender.store(nullptr, fl::memory_order_release);
        if (owner_task) {
            vTaskDelete(owner_task); // ok platform specific - reclaim the quiescent SDK worker used to exercise ESP-IDF guards
        }
        if (contender_task) {
            vTaskDelete(contender_task); // ok platform specific - reclaim the quiescent SDK worker used to exercise ESP-IDF guards
        }
        staticInitProbe.entered.reset(nullptr);
        staticInitProbe.completed.reset(nullptr);
    }
    return result;
}
#endif

#if defined(FL_IS_TEENSY_4X)
struct FlexIoObjectFledState {
    CRGB leds[100];
};

FlexIoObjectFledState& flexIoObjectFledState() {
    return fl::Singleton<FlexIoObjectFledState>::instance();
}
#endif

}  // namespace

#if defined(FL_AUTORESEARCH_MEMORY_BENCH)
void bindMemoryBench(fl::Remote& remote);
#endif

void AutoResearchRemoteControl::bindDriverMethods(fl::Remote& remote) {
    remote.bind("staticInitTest", [](const fl::json&) -> fl::json {
#if defined(FL_IS_ESP32) // ok platform specific - fl::thread has no concurrent ESP32 implementation; directly test the vendor ABI
        return staticInitTest();
#else
        fl::json result = fl::json::object();
        result.set("success", false);
        result.set("error", "Esp32Required");
        return result;
#endif
    });
#if defined(FL_AUTORESEARCH_MEMORY_BENCH)
    bindMemoryBench(remote);
#endif
    // One physical data jumper checks either SPI host while a second APA102
    // channel exercises the registration-order interaction from #4864.
    remote.bind("dualSpiHostLoopback", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
#if !defined(FL_IS_ESP32) || !FASTLED_ESP32_HAS_SPI3 // ok platform specific - this probe requires two physical ESP32 SPI hosts
        (void)this;
        (void)args;
        response.set("success", false);
        response.set("error", "TwoSpiHostsUnavailable");
        return response;
#else
        if (!args.is_object() || !args.contains("activeHost") ||
            !args["activeHost"].is_int()) {
            response.set("success", false);
            response.set("error", "ExpectedActiveHost");
            return response;
        }
        const int active_host = static_cast<int>(args["activeHost"].as_int().value());
        if (active_host != 2 && active_host != 3) {
            response.set("success", false);
            response.set("error", "InvalidActiveHost");
            return response;
        }
        const bool dummy_first = args.contains("dummyFirst") &&
            args["dummyFirst"].is_bool() && args["dummyFirst"].as_bool().value();
        constexpr int kLedCount = 100;
        static bool already_run = false;
        if (already_run) {
            response.set("success", false);
            response.set("error", "ResetBoardBeforeNextOrder");
            return response;
        }
        already_run = true;
        static CRGB active_leds[kLedCount];
        static CRGB dummy_leds[kLedCount];
        for (int i = 0; i < kLedCount; ++i) {
            active_leds[i] = CRGB(static_cast<uint8_t>(i * 19 + 1),
                                  static_cast<uint8_t>(i * 13 + 2),
                                  static_cast<uint8_t>(i * 7 + 3));
            dummy_leds[i] = CRGB::Black;
        }
        const auto register_active = [&]() {
            FastLED.addLeds<APA102, 1, 3, RGB, DATA_RATE_KHZ(2400)>( // ok platform specific - test the public explicit ESP32 host API
                active_leds, kLedCount, active_host == 2
                    ? fl::Esp32SpiBus::HOST2 : fl::Esp32SpiBus::HOST3); // ok platform specific - select the measured physical host
        };
        const auto register_dummy = [&]() {
            FastLED.addLeds<APA102, 4, 5, RGB, DATA_RATE_KHZ(2400)>( // ok platform specific - register the other host as the dummy
                dummy_leds, kLedCount, active_host == 2
                    ? fl::Esp32SpiBus::HOST3 : fl::Esp32SpiBus::HOST2); // ok platform specific - choose the host opposite the measured output
        };
        if (dummy_first) {
            register_dummy();
            register_active();
        } else {
            register_active();
            register_dummy();
        }
        FastLED.setBrightness(255);
        FastLED.show();
        const bool primed = FastLED.wait(1000);
        size_t captured = 0;
        fl::RunResult diagnostics;
        if (primed) {
            captured = capture(mState->rx_channel, mState->rx_buffer,
                               kLedCount * 3, 1,
                               fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(),
                               "SPI_UNIFIED", &diagnostics);
        }
        response.set("activeHost", active_host);
        response.set("dummyFirst", dummy_first);
        response.set("genericDecodedBytes", static_cast<int>(captured));
        response.set("primed", primed);
        response.set("rxWaitResult", diagnostics.captureWaitResult);
        response.set("rxEdges", diagnostics.rawEdgesAfterWait);
        response.set("rxBeginError", diagnostics.rxBeginError.c_str());
        fl::EdgeTime edge_samples[256];
        const size_t edge_count = mState->rx_channel->getRawEdgeTimes(
            fl::span<fl::EdgeTime>(edge_samples, 256), 0);
        response.set("rawEdges", static_cast<int>(edge_count));
        constexpr size_t kVerifiedLeds = 8;
        uint8_t first_bytes[kVerifiedLeds * 4] = {};
        size_t bit_count = 0;
        for (size_t i = 0; i < edge_count && bit_count < sizeof(first_bytes) * 8; ++i) {
            const size_t bits = (edge_samples[i].ns + 208) / 416;
            for (size_t j = 0; j < bits && bit_count < sizeof(first_bytes) * 8; ++j, ++bit_count) {
                if (edge_samples[i].high) {
                    first_bytes[bit_count / 8] |=
                        static_cast<uint8_t>(1u << (7 - bit_count % 8));
                }
            }
        }
        int wire_mismatches = 0;
        if (bit_count < sizeof(first_bytes) * 8) {
            wire_mismatches = static_cast<int>(sizeof(first_bytes) * 8 - bit_count);
        } else {
            for (size_t i = 0; i < kVerifiedLeds; ++i) {
                const size_t offset = i * 4;
                wire_mismatches += first_bytes[offset] != 0xFF;
                wire_mismatches += first_bytes[offset + 1] != active_leds[i].r;
                wire_mismatches += first_bytes[offset + 2] != active_leds[i].g;
                wire_mismatches += first_bytes[offset + 3] != active_leds[i].b;
            }
        }
        response.set("verifiedLeds", static_cast<int>(kVerifiedLeds));
        response.set("wireBits", static_cast<int>(bit_count));
        response.set("wireMismatches", wire_mismatches);
        response.set("first0", first_bytes[0]);
        response.set("first1", first_bytes[1]);
        response.set("first2", first_bytes[2]);
        response.set("first3", first_bytes[3]);
        response.set("first4", first_bytes[4]);
        response.set("first5", first_bytes[5]);
        response.set("first6", first_bytes[6]);
        response.set("first7", first_bytes[7]);
        if (edge_count >= 4) {
            response.set("edge0Ns", static_cast<int>(edge_samples[0].ns));
            response.set("edge1Ns", static_cast<int>(edge_samples[1].ns));
            response.set("edge2Ns", static_cast<int>(edge_samples[2].ns));
            response.set("edge3Ns", static_cast<int>(edge_samples[3].ns));
            response.set("edge0High", edge_samples[0].high);
            response.set("edge1High", edge_samples[1].high);
        }
        auto driver = fl::ChannelManager::registry().findDriverByName(
            fl::string::from_literal("SPI_UNIFIED"));
        const bool native_driver_found = static_cast<bool>(driver);
        const bool native_driver_enabled = driver &&
            fl::ChannelManager::registry().isDriverEnabled(*driver);
        response.set("nativeDriverFound", native_driver_found);
        response.set("nativeDriverEnabled", native_driver_enabled);
        response.set("success", primed && native_driver_enabled && wire_mismatches == 0);
        return response;
#endif
    });
    // Register "softSpiClockLoopback" — measures the software SPI clock rate
    // (#5124). Build with FASTLED_FORCE_SOFTWARE_SPI; jumper GPIO 0 (APA102
    // clock) to GPIO 1 (RX). Data goes to GPIO 2, unmeasured. Reports clock
    // high/low widths from raw RX edges, skipping the first edge.
    remote.bind("softSpiClockLoopback", [this](const fl::json& args) -> fl::json {
        (void)args;
        fl::json response = fl::json::object();
        constexpr int kLedCount = 16;
        static bool already_run = false;
        if (already_run) {
            response.set("success", false);
            response.set("error", "ResetBoardBeforeNextRun");
            return response;
        }
        already_run = true;
#if !defined(FL_IS_ESP32) // ok platform specific - SPIOutput is the ESP32 SPIBusManager proxy here
        (void)this;
        response.set("success", false);
        response.set("error", "Esp32Only");
        return response;
#else
        // addLeds<APA102> goes through the Channels SPI driver, which never
        // reaches SPIBusManager's software writer, so drive the legacy
        // SPIOutput proxy directly: under FASTLED_FORCE_SOFTWARE_SPI it flushes
        // through SPIBusManager::softwareSPIWrite.
        struct SoftSpiProbe : public CPixelLEDController<RGB> {
            fl::SPIOutput<2, 0, 250000> spi;
            void init() FL_NO_EXCEPT override { spi.init(); }
            void showPixels(PixelController<RGB>& pixels) FL_NO_EXCEPT override {
                spi.select();
                for (int i = 0; i < pixels.size() * 4; ++i) {
                    spi.writeByte(0x55);
                }
                spi.finalizeTransmission();
                spi.release();
            }
        };
        static SoftSpiProbe probe;
        static CRGB leds[kLedCount];
        FastLED.addLeds(&probe, leds, kLedCount);
        fl::RunResult diagnostics;
        capture(mState->rx_channel, mState->rx_buffer, kLedCount * 4, 1,
                fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(),
                "SOFT_SPI_CLOCK", &diagnostics);
        fl::SPIBusManager& manager = fl::getSPIBusManager();
        response.set("buses", static_cast<int>(manager.getNumBuses()));
        if (const fl::SPIBusInfo* bus = manager.getBusInfo(0)) {
            response.set("busType", static_cast<int>(bus->bus_type));
            response.set("busDevices", static_cast<int>(bus->num_devices));
            response.set("busInitialized", bus->is_initialized);
        }
        response.set("rxBeginError", diagnostics.rxBeginError.c_str());
        response.set("rxWaitResult", diagnostics.captureWaitResult);
        fl::EdgeTime edges[256];
        const size_t edge_count = mState->rx_channel->getRawEdgeTimes(
            fl::span<fl::EdgeTime>(edges, 256), 0);
        u32 high_sum = 0, low_sum = 0, high_n = 0, low_n = 0;
        u32 high_min = 0xFFFFFFFFu, low_min = 0xFFFFFFFFu;
        for (size_t i = 1; i < edge_count; ++i) {
            const u32 ns = edges[i].ns;
            if (ns > 100000u) {
                continue;  // frame gap, not a clock half-period
            }
            if (edges[i].high) {
                high_sum += ns; ++high_n; high_min = fl::min(high_min, ns);
            } else {
                low_sum += ns; ++low_n; low_min = fl::min(low_min, ns);
            }
        }
        const u32 high_avg = high_n ? high_sum / high_n : 0;
        const u32 low_avg = low_n ? low_sum / low_n : 0;
        const u32 period = high_avg + low_avg;
        // Only a successful RX begin and wait with real clock edges counts.
        const bool primed = diagnostics.rxBeginError.empty() &&
            diagnostics.captureWaitResult == 0 && high_n >= 8 && low_n >= 8;
        response.set("primed", primed);
        response.set("rawEdges", static_cast<int>(edge_count));
        response.set("highAvgNs", static_cast<int>(high_avg));
        response.set("lowAvgNs", static_cast<int>(low_avg));
        response.set("highMinNs", static_cast<int>(high_n ? high_min : 0));
        response.set("lowMinNs", static_cast<int>(low_n ? low_min : 0));
        response.set("periodNs", static_cast<int>(period));
        response.set("measuredKhz", static_cast<int>(period ? 1000000u / period : 0));
        response.set("requestedKhz", 250);
        // Pass when the 250 kHz clock is no faster than requested (10% tolerance).
        response.set("success", primed && period >= 3600u);
        return response;
#endif
    });
    // Register "flexioRxBenchmark" — square-wave validation for the new
    // FlexIO RX backend (Phase 2 of FastLED#2764).
    //
    // Drives `tx_pin` with a `frequency_hz` square wave via the Teensy core's
    // `analogWriteFrequency` PWM, configures an RxChannel on `rx_pin` using
    // `RxBackend::FLEXIO`, captures for `duration_ms`, and reports per-period
    // statistics: count, mean ns, σ ns, min ns, max ns.
    //
    // Args (positional, all optional with defaults):
    //   {
    //     "frequency_hz": int = 1000,
    //     "duration_ms":  int = 100,
    //     "tx_pin":       int = 3,   // matches GPIO 3↔4 jumper
    //     "rx_pin":       int = 4
    //   }
    //
    // Teensy-4-only because FLEXIO1 is iMXRT1062-specific. Other platforms
    // get a clean "not supported" response so the RPC harness can still
    // round-trip.
    remote.bind("flexioRxBenchmark", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
#if !defined(FL_IS_TEENSY_4X)
        (void)args;
        response.set("success", false);
        response.set("error", "PlatformNotSupported");
        response.set("message",
                     "flexioRxBenchmark is Teensy 4.x-only (FLEXIO1 capture).");
        return response;
#else
        // Parse args
        int frequency_hz = 1000;
        int duration_ms  = 100;
        int tx_pin       = 3;
        int rx_pin       = 4;
        if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            fl::json cfg = args[0];
            if (cfg.contains("frequency_hz") && cfg["frequency_hz"].is_int()) {
                frequency_hz = static_cast<int>(cfg["frequency_hz"].as_int().value());
            }
            if (cfg.contains("duration_ms") && cfg["duration_ms"].is_int()) {
                duration_ms = static_cast<int>(cfg["duration_ms"].as_int().value());
            }
            if (cfg.contains("tx_pin") && cfg["tx_pin"].is_int()) {
                tx_pin = static_cast<int>(cfg["tx_pin"].as_int().value());
            }
            if (cfg.contains("rx_pin") && cfg["rx_pin"].is_int()) {
                rx_pin = static_cast<int>(cfg["rx_pin"].as_int().value());
            }
        }
        if (frequency_hz < 1 || frequency_hz > 5000000 ||
            duration_ms < 1 || duration_ms > 5000) {
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message",
                         "frequency_hz in [1, 5_000_000]; duration_ms in [1, 5000].");
            return response;
        }

        // 1. Drive the TX pin with a square wave.
        analogWriteFrequency(tx_pin, (float)frequency_hz);
        analogWrite(tx_pin, 128);  // 50% duty

        // 2. Create the FlexIO RX channel.
        // Width budget — size the edge buffer for the actual capture window:
        //   expected_edges = 2 * frequency_hz * duration_ms / 1000     (transitions)
        //   target = expected_edges * 1.5  (50% headroom for jitter/skew)
        // Clamped to [1024, 16384] so the DMA buffer stays reasonable and
        // matches the FlexIO RX driver's internal cap.
        fl::RxChannelConfig rx_cfg(rx_pin, fl::RxBackend::FLEXIO);
        const fl::u64 expected_edges =
            (fl::u64)2 * (fl::u64)frequency_hz * (fl::u64)duration_ms / 1000ULL;
        fl::u64 target = expected_edges + expected_edges / 2;  // +50%
        if (target < 1024ULL) target = 1024ULL;
        if (target > 16384ULL) target = 16384ULL;
        rx_cfg.edge_capacity = (size_t)target;
        rx_cfg.start_low = false;  // PWM output idles in either state, just track transitions
        auto rx_channel = fl::RxChannel::create(rx_cfg);
        if (!rx_channel) {
            analogWrite(tx_pin, 0);
            response.set("success", false);
            response.set("error", "RxCreateFailed");
            response.set("message",
                         "Failed to create FlexIO RX channel on pin (no FLEXIO1 mapping).");
            return response;
        }
        if (!rx_channel->begin(rx_cfg)) {
            analogWrite(tx_pin, 0);
            response.set("success", false);
            response.set("error", "RxBeginFailed");
            response.set("message",
                         "RxChannel::begin() returned false (see device WARN log).");
            return response;
        }

        // 3. Let it capture for the requested window.
        rx_channel->wait((u32)duration_ms);

        // 4. Stop the square wave so the line is quiet for stats computation.
        analogWrite(tx_pin, 0);

        // 5. Pull captured edges out.
        fl::vector<fl::EdgeTime> edges;
        edges.assign(rx_cfg.edge_capacity, fl::EdgeTime());
        size_t edges_captured =
            rx_channel->getRawEdgeTimes(fl::span<fl::EdgeTime>(edges), 0);
        edges.resize(edges_captured);

        // 6. Compute period statistics. A "period" = duration_high + duration_low
        //    for adjacent edges, so we iterate in pairs.
        fl::u64 sum_ns = 0;
        fl::u64 sum_sq_ns = 0;
        fl::u32 min_ns = 0xFFFFFFFFu;
        fl::u32 max_ns = 0;
        size_t periods = 0;
        for (size_t i = 0; i + 1 < edges_captured; i += 2) {
            const fl::u32 period_ns =
                (fl::u32)edges[i].ns + (fl::u32)edges[i + 1].ns;
            if (period_ns == 0) continue;
            sum_ns += period_ns;
            sum_sq_ns += (fl::u64)period_ns * (fl::u64)period_ns;
            if (period_ns < min_ns) min_ns = period_ns;
            if (period_ns > max_ns) max_ns = period_ns;
            ++periods;
        }
        fl::u32 mean_ns = 0;
        fl::u32 sigma_ns = 0;
        if (periods > 0) {
            mean_ns = (fl::u32)(sum_ns / (fl::u64)periods);
            const fl::u64 mean64 = (fl::u64)mean_ns;
            const fl::u64 var =
                periods > 0 ? (sum_sq_ns / (fl::u64)periods) - (mean64 * mean64)
                            : 0;
            // Integer sqrt for σ — adequate for the tolerance ranges we
            // assert in Phase 2 (σ < 100 ns at 100 kHz).
            fl::u32 s = 0;
            while ((fl::u64)(s + 1) * (fl::u64)(s + 1) <= var) ++s;
            sigma_ns = s;
        }
        if (min_ns == 0xFFFFFFFFu) min_ns = 0;

        response.set("success", true);
        response.set("frequency_hz", static_cast<int64_t>(frequency_hz));
        response.set("duration_ms", static_cast<int64_t>(duration_ms));
        response.set("tx_pin", static_cast<int64_t>(tx_pin));
        response.set("rx_pin", static_cast<int64_t>(rx_pin));
        response.set("edges_captured", static_cast<int64_t>(edges_captured));
        response.set("periods", static_cast<int64_t>(periods));
        response.set("period_mean_ns", static_cast<int64_t>(mean_ns));
        response.set("period_sigma_ns", static_cast<int64_t>(sigma_ns));
        response.set("period_min_ns", static_cast<int64_t>(min_ns));
        response.set("period_max_ns", static_cast<int64_t>(max_ns));
        return response;
#endif
    });

    // Register "flexioObjectFledTest" — end-to-end ObjectFLED TX → FlexIO RX
    // loopback verification (Phase 3 of FastLED#2764).
    //
    // Drives a small WS2812 pattern through `Bus::FLEX_IO` slot 0 on `tx_pin`,
    // captures the wire signal back through the new `RxBackend::FLEXIO` on
    // `rx_pin`, decodes the bit stream against WS2812B-V5 timing, and reports
    // how many bytes matched the transmitted pattern.
    //
    // Test cases (parent issue Phase 3 table):
    //   0 — Red single LED            (1 LED, 0xFF0000 → wire-order GRB 00,FF,00)
    //   1 — RGB three-LED chain       (3 LEDs)
    //   2 — All zeros                 (1 LED, 0x000000 — T0H/T0L fidelity)
    //   3 — All ones                  (1 LED, 0xFFFFFF — T1H/T1L fidelity)
    //   4 — 100-LED alternating R/G/B (long capture, watchdog-safety)
    //
    // Args (positional, all optional):
    //   { "test_case": int = 0,
    //     "tx_pin":    int = 3,   // matches GPIO 3↔4 jumper
    //     "rx_pin":    int = 4,
    //     "capture_ms": int = 50 }
    //
    // Returns:
    //   { success, test_case, num_leds, expected_bytes, decoded_bytes,
    //     matched, mismatched, edges_captured }
    //
    // Teensy-4-only — FLEXIO1 is iMXRT1062-specific. ObjectFLED also relies
    // on Teensy 4-core APIs, so non-Teensy builds return `PlatformNotSupported`.
    remote.bind("flexioObjectFledTest", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
#if !defined(FL_IS_TEENSY_4X)
        (void)args;
        response.set("success", false);
        response.set("error", "PlatformNotSupported");
        response.set("message",
                     "flexioObjectFledTest is Teensy 4.x-only (FLEXIO1 RX + "
                     "Teensy-core ObjectFLED driver).");
        return response;
#else
        // Parse args
        int test_case = 0;
        int tx_pin    = 3;
        int rx_pin    = 4;
        int capture_ms = 50;
        if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            fl::json cfg = args[0];
            if (cfg.contains("test_case") && cfg["test_case"].is_int()) {
                test_case = static_cast<int>(cfg["test_case"].as_int().value());
            }
            if (cfg.contains("tx_pin") && cfg["tx_pin"].is_int()) {
                tx_pin = static_cast<int>(cfg["tx_pin"].as_int().value());
            }
            if (cfg.contains("rx_pin") && cfg["rx_pin"].is_int()) {
                rx_pin = static_cast<int>(cfg["rx_pin"].as_int().value());
            }
            if (cfg.contains("capture_ms") && cfg["capture_ms"].is_int()) {
                capture_ms = static_cast<int>(cfg["capture_ms"].as_int().value());
            }
        }
        if (test_case < 0 || test_case > 4 ||
            capture_ms < 1 || capture_ms > 5000) {
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message",
                         "test_case in [0,4]; capture_ms in [1, 5000].");
            return response;
        }

        // 1. Build the test pattern. Fixed upper bound (100 LEDs) covers
        //    case 4 — the larger cases reuse the same static buffer.
        CRGB* leds_buf = flexIoObjectFledState().leds;
        int num_leds = 0;
        switch (test_case) {
            case 0:
                num_leds = 1;
                leds_buf[0] = CRGB::Red;
                break;
            case 1:
                num_leds = 3;
                leds_buf[0] = CRGB::Red;
                leds_buf[1] = CRGB::Green;
                leds_buf[2] = CRGB::Blue;
                break;
            case 2:
                num_leds = 1;
                leds_buf[0] = CRGB::Black;
                break;
            case 3:
                num_leds = 1;
                leds_buf[0] = CRGB(0xFF, 0xFF, 0xFF);
                break;
            case 4:
                num_leds = 100;
                for (int i = 0; i < 100; ++i) {
                    leds_buf[i] = (i % 3 == 0) ? CRGB::Red
                                : (i % 3 == 1) ? CRGB::Green
                                               : CRGB::Blue;
                }
                break;
        }

        // 2. Build the expected wire-order byte stream (WS2812 is GRB).
        fl::vector<u8> expected;
        expected.reserve((size_t)num_leds * 3);
        for (int i = 0; i < num_leds; ++i) {
            expected.push_back(leds_buf[i].g);
            expected.push_back(leds_buf[i].r);
            expected.push_back(leds_buf[i].b);
        }

        // 3. Set up FlexIO RX. Size the edge buffer for 24 bits per LED ×
        //    2 transitions per bit, with 25% headroom.
        const size_t expected_edges = (size_t)num_leds * 24u * 2u;
        size_t edge_capacity = expected_edges + expected_edges / 4u + 64u;
        if (edge_capacity > 16384u) edge_capacity = 16384u;

        fl::RxChannelConfig rx_cfg(rx_pin, fl::RxBackend::FLEXIO);
        rx_cfg.edge_capacity = edge_capacity;
        rx_cfg.start_low = true;
        auto rx_channel = fl::RxChannel::create(rx_cfg);
        if (!rx_channel || !rx_channel->begin(rx_cfg)) {
            response.set("success", false);
            response.set("error", "RxBeginFailed");
            response.set("message",
                         "Failed to bring up FlexIO RX on the requested pin "
                         "(check kFlexIo1Pins[] mapping).");
            return response;
        }

        // 4. Configure ObjectFLED TX via FastLED.add().
        fl::ChannelOptions opts;
        opts.mBus = fl::Bus::FLEX_IO;
        opts.mBusWhich = 0;
        auto resolved_timing  = fl::makeTimingConfig<fl::TIMING_WS2812B_V5>();
        auto resolved_encoder = fl::encoder_for<fl::TIMING_WS2812B_V5>();
        fl::NamedTimingConfig timing_cfg(resolved_timing, "WS2812B-V5",
                                         resolved_encoder);
        fl::ChannelConfig tx_cfg(
            tx_pin,
            timing_cfg.timing,
            fl::span<CRGB>(leds_buf, (size_t)num_leds),
            RGB,
            opts);
        auto tx_channel = FastLED.add(tx_cfg);
        if (!tx_channel) {
            response.set("success", false);
            response.set("error", "TxAddFailed");
            response.set("message",
                         "FastLED.add() rejected the ObjectFLED ChannelConfig.");
            return response;
        }

        // 5. Trigger TX. FastLED.show() schedules the DMA and returns once
        //    the frame has been queued. The RX wait must be long enough to
        //    cover BOTH the TX transmission time AND the capture-buffer
        //    fill — otherwise teardown happens mid-frame and the hardware
        //    can be left in a bad state. Compute a minimum from the actual
        //    WS2812 timing (1.25 µs per bit, 24 bits per LED, plus reset
        //    gap) and use the larger of the caller's `capture_ms` and that
        //    minimum.
        FastLED.show();
        const u32 ws2812_tx_us =
            (u32)num_leds * 24u * 13u / 10u + 100u;  // 1.25 µs/bit + reset
        const u32 min_capture_ms = (ws2812_tx_us + 999u) / 1000u + 10u;
        const u32 effective_capture_ms = ((u32)capture_ms > min_capture_ms)
                                              ? (u32)capture_ms
                                              : min_capture_ms;
        rx_channel->wait(effective_capture_ms);

        // 6. Decode the captured edge stream against WS2812 4-phase timing.
        const fl::ChipsetTiming ws2812_timing =
            fl::to_runtime_timing<fl::TIMING_WS2812B_V5>();
        fl::ChipsetTiming4Phase rx_timing =
            fl::make4PhaseTiming(ws2812_timing);
        fl::vector<u8> decoded;
        decoded.assign(expected.size() + 16u, 0u);
        auto decode_result =
            rx_channel->decode(rx_timing, fl::span<u8>(decoded));

        // 7. Compare decoded bytes against expected.
        u32 decoded_bytes = 0u;
        if (decode_result) {
            decoded_bytes = decode_result.value();
        }
        const size_t cmp_n = (decoded_bytes < expected.size())
                                 ? (size_t)decoded_bytes
                                 : expected.size();
        size_t matched = 0;
        size_t mismatched = 0;
        for (size_t i = 0; i < cmp_n; ++i) {
            if (decoded[i] == expected[i]) ++matched; else ++mismatched;
        }
        if (decoded_bytes < expected.size()) {
            mismatched += expected.size() - decoded_bytes;
        }

        // 8. Read raw edge count for diagnostics.
        fl::vector<fl::EdgeTime> edges;
        edges.assign(edge_capacity, fl::EdgeTime());
        const size_t edges_captured =
            rx_channel->getRawEdgeTimes(fl::span<fl::EdgeTime>(edges), 0);

        // 9. Tear the TX channel back down so subsequent tests can use the
        //    same pin (FastLED.clear(ClearFlags::CHANNELS) matches the
        //    pattern used by runSingleTestImpl in this file).
        FastLED.clear(ClearFlags::CHANNELS);

        response.set("success", mismatched == 0 && decoded_bytes == expected.size());
        response.set("test_case", static_cast<int64_t>(test_case));
        response.set("tx_pin", static_cast<int64_t>(tx_pin));
        response.set("rx_pin", static_cast<int64_t>(rx_pin));
        response.set("num_leds", static_cast<int64_t>(num_leds));
        response.set("expected_bytes", static_cast<int64_t>(expected.size()));
        response.set("decoded_bytes", static_cast<int64_t>(decoded_bytes));
        response.set("matched", static_cast<int64_t>(matched));
        response.set("mismatched", static_cast<int64_t>(mismatched));
        response.set("edges_captured", static_cast<int64_t>(edges_captured));
        return response;
#endif
    });

    // Register "flexioRxLoopbackPing" — FastLED#3066 phase 1.7 diagnostic.
    //
    // Bypasses every clockless driver entirely and tests whether the
    // FlexIO RX backend can capture HAND-DRIVEN GPIO transitions. Drives
    // `tx_pin` as a plain `digitalWrite` HIGH/LOW square wave while
    // `RxBackend::FLEXIO` is armed on `rx_pin`. If the captured buffer
    // shows any non-zero data, IOMUX ALT4 + PINSEL routing is correct
    // and the WS2812-loopback failure mode is downstream (timer/shifter
    // bandwidth, decoder thresholds, etc.). If the buffer stays all
    // zero with a 4-edge ground-truth pin toggle, the routing itself is
    // broken — debug that BEFORE more timer/shifter config experiments.
    //
    // Wiring: same TX↔RX jumper as flexioObjectFledTest (defaults 3↔4).
    //
    // Args:
    //   { tx_pin?:    3,    // any digital pin
    //     rx_pin?:    4,    // must be in kFlexIo1Pins[]
    //     toggle_us?: 50,   // half-period of the manual square wave
    //     edges?:     8 }   // number of digitalWrite transitions
    // Returns:
    //   { success, tx_pin, rx_pin, toggle_us, edges, edges_captured,
    //     capture_buffer_first8_hex: [...], notes }
    //
    // Teensy-4-only.
    remote.bind("flexioRxLoopbackPing", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
#if !defined(FL_IS_TEENSY_4X)
        (void)args;
        response.set("success", false);
        response.set("error", "PlatformNotSupported");
        response.set("message",
                     "flexioRxLoopbackPing is Teensy 4.x-only (FLEXIO1 RX).");
        return response;
#else
        int tx_pin = 3;
        int rx_pin = 4;
        int toggle_us = 50;
        int edges = 8;
        if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            const fl::json &cfg = args[0];
            if (cfg.contains("tx_pin") && cfg["tx_pin"].is_int()) {
                tx_pin = static_cast<int>(cfg["tx_pin"].as_int().value());
            }
            if (cfg.contains("rx_pin") && cfg["rx_pin"].is_int()) {
                rx_pin = static_cast<int>(cfg["rx_pin"].as_int().value());
            }
            if (cfg.contains("toggle_us") && cfg["toggle_us"].is_int()) {
                toggle_us = static_cast<int>(cfg["toggle_us"].as_int().value());
            }
            if (cfg.contains("edges") && cfg["edges"].is_int()) {
                edges = static_cast<int>(cfg["edges"].as_int().value());
            }
        }
        if (edges < 2 || edges > 64) {
            response.set("success", false);
            response.set("error", "InvalidEdges");
            response.set("message", "edges must be in [2, 64]");
            return response;
        }

        // 1. Arm FlexIO RX on rx_pin.
        fl::RxChannelConfig rx_cfg(rx_pin, fl::RxBackend::FLEXIO);
        rx_cfg.edge_capacity = 1024;   // plenty for ~10 edges
        rx_cfg.start_low = true;
        auto rx_channel = fl::RxChannel::create(rx_cfg);
        if (!rx_channel || !rx_channel->begin(rx_cfg)) {
            response.set("success", false);
            response.set("error", "RxBeginFailed");
            response.set("message",
                         "FlexIO RX begin() failed (kFlexIo1Pins[] mapping?).");
            return response;
        }

        // 2. Drive tx_pin as a plain GPIO output, toggle a known number of
        //    transitions. Start LOW so the first digitalWrite(HIGH) is a
        //    rising edge.
        pinMode(tx_pin, OUTPUT);
        digitalWrite(tx_pin, LOW);
        delayMicroseconds(100);  // let the line settle + RX arm fully

        bool level = true;
        for (int i = 0; i < edges; ++i) {
            digitalWrite(tx_pin, level ? HIGH : LOW);
            delayMicroseconds(toggle_us);
            level = !level;
        }

        // FastLED#3066 iter 7 diagnostic: also probe SHIFTBUF[0] +
        // SHIFTBUFBIS (bit-swapped) + SHIFTBUFBYS (byte-swapped) so we
        // can see whether the captured bit lands in a bit position
        // different from what the canonical SHIFTBUF[0] read produces.
        {
            constexpr uintptr_t kFLEXIO1_BASE_DIAG = 0x401AC000u;
            volatile uint32_t *pin   = (volatile uint32_t *)(kFLEXIO1_BASE_DIAG + 0x00C);
            volatile uint32_t *shftstat = (volatile uint32_t *)(kFLEXIO1_BASE_DIAG + 0x010);
            volatile uint32_t *timstat = (volatile uint32_t *)(kFLEXIO1_BASE_DIAG + 0x018);
            volatile uint32_t *shftbuf  = (volatile uint32_t *)(kFLEXIO1_BASE_DIAG + 0x200);
            volatile uint32_t *shftbufbis = (volatile uint32_t *)(kFLEXIO1_BASE_DIAG + 0x280);
            volatile uint32_t *shftbufbys = (volatile uint32_t *)(kFLEXIO1_BASE_DIAG + 0x300);
            fl::json flexio_diag = fl::json::object();
            flexio_diag.set("pin", static_cast<int64_t>(*pin));
            flexio_diag.set("shiftstat", static_cast<int64_t>(*shftstat));
            flexio_diag.set("timstat", static_cast<int64_t>(*timstat));
            flexio_diag.set("shiftbuf0", static_cast<int64_t>(*shftbuf));
            flexio_diag.set("shiftbufbis0", static_cast<int64_t>(*shftbufbis));
            flexio_diag.set("shiftbufbys0", static_cast<int64_t>(*shftbufbys));
            response.set("flexio1", flexio_diag);
        }

        // 3. Wait briefly for FlexIO RX to flush any pending DMA. The DMA
        //    is configured for completion-on-buffer-full; a partial fill
        //    won't trigger the ISR, so `wait()` will TIMEOUT — which is
        //    fine, we read the captured edges directly.
        rx_channel->wait(50);

        // 4. Read the captured edges via the public API.
        fl::vector<fl::EdgeTime> captured_edges;
        captured_edges.assign(64, fl::EdgeTime());
        size_t edge_count =
            rx_channel->getRawEdgeTimes(fl::span<fl::EdgeTime>(captured_edges), 0);

        // 5. Restore tx_pin so subsequent tests can use it.
        pinMode(tx_pin, INPUT);

        response.set("success", true);
        response.set("tx_pin", static_cast<int64_t>(tx_pin));
        response.set("rx_pin", static_cast<int64_t>(rx_pin));
        response.set("toggle_us", static_cast<int64_t>(toggle_us));
        response.set("edges", static_cast<int64_t>(edges));
        response.set("edges_captured", static_cast<int64_t>(edge_count));

        // Dump the first 8 captured edge durations (ns) so the host can
        // see whether they roughly match the requested toggle_us *
        // 1000 ns expectation.
        fl::json edges_arr = fl::json::array();
        const size_t to_dump = (edge_count < 8u) ? edge_count : 8u;
        for (size_t i = 0; i < to_dump; ++i) {
            fl::json e = fl::json::object();
            e.set("high", captured_edges[i].high);
            e.set("ns", static_cast<int64_t>(captured_edges[i].ns));
            edges_arr.push_back(e);
        }
        response.set("first_edges", edges_arr);
        return response;
#endif
    });

    remote.bind("legacyAsyncProbe", [](const fl::json& args) -> fl::json {
        return autoresearch::legacyAsyncProbe(args);
    });
    remote.bind("legacyAsyncErrorProbe", [](const fl::json& args) -> fl::json {
        return autoresearch::legacyAsyncErrorProbe(args);
    });

    // Register "timingDriftTest" — issue #2994 reproducer for compounded
    // per-sequence timing drift on master vs 3.10.3. Replays the reporter's
    // minimal sketch (35-LED WS2812B ring, setMaxRefreshRate(800),
    // millisDelay-gated 255-step fade, delay(1000) between sequences) and
    // returns per-sequence wall time so the host can build a histogram.
    // Args: [{pin, numLeds, iterations}] (all optional;
    //        default pin=4, numLeds=35, iterations=10; iterations clamped to
    //        autoresearch::timing_drift::kMaxIterations)
    remote.bind("timingDriftTest", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        int pin = 4;
        int num_leds = 35;
        int iterations = 10;

        fl::json config;
        if (args.is_object()) {
            config = args;
        } else if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            config = args[0];
        }
        if (!config.is_null()) {
            if (config.contains("pin") && config["pin"].is_int()) {
                pin = static_cast<int>(config["pin"].as_int().value());
            }
            if (config.contains("numLeds") && config["numLeds"].is_int()) {
                num_leds = static_cast<int>(config["numLeds"].as_int().value());
            }
            if (config.contains("iterations") && config["iterations"].is_int()) {
                iterations = static_cast<int>(config["iterations"].as_int().value());
            }
        }

        auto r = autoresearch::timing_drift::run(pin, num_leds, iterations);

        response.set("success", r.valid_pin);
        if (!r.valid_pin) {
            response.set("error", "InvalidPin");
            response.set("message",
                         "pin is outside LegacyClocklessProxy range");
            response.set("pin", r.pin);
            return response;
        }
        response.set("pin", r.pin);
        response.set("num_leds", r.num_leds);
        response.set("iterations", r.iterations);
        response.set("cpu_mhz", static_cast<int64_t>(r.cpu_mhz));
        response.set("show_count", static_cast<int64_t>(r.show_count));
        response.set("show_min_us", static_cast<int64_t>(r.show_min_us));
        response.set("show_max_us", static_cast<int64_t>(r.show_max_us));
        response.set("show_total_us", static_cast<int64_t>(r.show_total_us));

        fl::json iter_ms_arr = fl::json::array();
        for (int i = 0; i < r.iterations; ++i) {
            iter_ms_arr.push_back(static_cast<int64_t>(r.iter_ms[i]));
        }
        response.set("iter_ms", iter_ms_arr);
        return response;
    });

    // Register "flexioSpiSelfTest" — #3428 bring-up smoke test for the
    // FlexIO2 SPI master mode. Exercises the full
    // lookup → init → show → wait → deinit path on a user-specified
    // (MOSI, SCLK) pin pair with a known byte pattern, then reports back
    // success/failure of each stage plus the resolved FlexIO2 pin indices
    // so a host-side test can diagnose routing and bring-up failures.
    //
    // This test does NOT need a logic analyzer or a connected APA102
    // strip — it confirms the driver runs without crashing the firmware,
    // pins route, init programs the shifter/timer, and DMA completes
    // within the bounded timeout. Full byte-level verification needs a
    // scope (see TODO markers in flexio_spi_mode.cpp.hpp).
    //
    // Args (positional, all optional):
    //   { "mosi_pin":  int = 10,   // FlexIO2 pin 0  (default APA102 MOSI)
    //     "sclk_pin":  int = 12,   // FlexIO2 pin 1  (default APA102 SCLK)
    //     "clock_hz":  int = 1000000,  // 1 MHz default for safety
    //     "num_bytes": int = 4 }       // sent bytes 0xA5 0x5A 0xFF 0x00
    //
    // Returns:
    //   { success, mosi_pin, sclk_pin, clock_hz, num_bytes,
    //     lookup_ok, init_ok, show_ok, wait_ms,
    //     mosi_flexio_pin, sclk_flexio_pin }
    //
    // Teensy 4.x-only.
    remote.bind("flexioSpiSelfTest", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
#if !defined(FL_IS_TEENSY_4X)
        (void)args;
        response.set("success", false);
        response.set("error", "PlatformNotSupported");
        response.set("message",
                     "flexioSpiSelfTest is Teensy 4.x-only (FlexIO2 SPI master mode).");
        return response;
#else
        // Parse args with defaults that work out-of-the-box on Teensy 4.x.
        // Note: the RPC framework signature `fl::json(const fl::json&)` makes
        // the framework strip the outer params array and pass `params[0]`
        // directly as `args`. So `args` is an OBJECT here, not an array --
        // some existing bindings in this file have the wrong `args.is_array()`
        // check (pre-existing bug, fix tracked separately) and silently use
        // their defaults always.
        int mosi_pin = 10;       // FlexIO2 pin 0 (teensy pin 10)
        int sclk_pin = 12;       // FlexIO2 pin 1 (teensy pin 12)
        int clock_hz = 1000000;  // 1 MHz -- safe for bring-up
        int num_bytes = 4;
        if (args.is_object()) {
            const fl::json& cfg = args;
            if (cfg.contains("mosi_pin") && cfg["mosi_pin"].is_int()) {
                mosi_pin = static_cast<int>(cfg["mosi_pin"].as_int().value());
            }
            if (cfg.contains("sclk_pin") && cfg["sclk_pin"].is_int()) {
                sclk_pin = static_cast<int>(cfg["sclk_pin"].as_int().value());
            }
            if (cfg.contains("clock_hz") && cfg["clock_hz"].is_int()) {
                clock_hz = static_cast<int>(cfg["clock_hz"].as_int().value());
            }
            if (cfg.contains("num_bytes") && cfg["num_bytes"].is_int()) {
                num_bytes = static_cast<int>(cfg["num_bytes"].as_int().value());
            }
        }

        response.set("mosi_pin", mosi_pin);
        response.set("sclk_pin", sclk_pin);
        response.set("clock_hz", static_cast<int64_t>(clock_hz));
        response.set("num_bytes", num_bytes);

        // Validate input ranges BEFORE narrowing to u8/u32. Without these
        // guards, mosi_pin=270 wraps to pin 14 in static_cast<u8>(), and
        // a negative clock_hz wraps to a huge u32 that the SPI driver
        // would then clamp to the 25 MHz ceiling -- both silently produce
        // wrong-hardware behavior. Per coderabbitai review on PR #3431.
        if (num_bytes <= 0 || num_bytes > 64) {
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message", "num_bytes must be in [1, 64].");
            return response;
        }
        if (mosi_pin < 0 || mosi_pin > 255 ||
            sclk_pin < 0 || sclk_pin > 255) {
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message", "mosi_pin / sclk_pin must be in [0, 255].");
            return response;
        }
        if (clock_hz <= 0) {
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message", "clock_hz must be > 0.");
            return response;
        }

        // 1. Pin-pair lookup.
        fl::FlexIOSPIPinInfo pin_info{};
        bool lookup_ok = fl::flexio_spi_lookup_pins(
            static_cast<fl::u8>(mosi_pin),
            static_cast<fl::u8>(sclk_pin),
            &pin_info);
        response.set("lookup_ok", lookup_ok);
        if (!lookup_ok) {
            response.set("success", false);
            response.set("error", "PinNotRoutable");
            response.set("message",
                         "(MOSI, SCLK) pair is not FlexIO2-routable; both pins "
                         "must be in {6, 7, 8, 9, 10, 11, 12, 13, 32}.");
            return response;
        }
        response.set("mosi_flexio_pin",
                     static_cast<int64_t>(pin_info.mosi_flexio_pin));
        response.set("sclk_flexio_pin",
                     static_cast<int64_t>(pin_info.sclk_flexio_pin));

        // 2. Init.
        bool init_ok = fl::flexio_spi_init(pin_info,
                                            static_cast<fl::u32>(clock_hz));
        response.set("init_ok", init_ok);
        if (!init_ok) {
            response.set("success", false);
            response.set("error", "InitFailed");
            response.set("message", "flexio_spi_init returned false.");
            return response;
        }

        // 3. Send a known pattern. 0xA5 = 1010 0101 — easy to read on a
        // scope to verify MSB-first ordering (high bit shifts out first).
        // Fill the rest with alternating 0xFF / 0x00 so the wire toggles.
        fl::u8 buffer[64] = {0};
        for (int i = 0; i < num_bytes; ++i) {
            switch (i % 4) {
                case 0: buffer[i] = 0xA5; break;
                case 1: buffer[i] = 0x5A; break;
                case 2: buffer[i] = 0xFF; break;
                case 3: buffer[i] = 0x00; break;
            }
        }

        const fl::u32 show_start_ms = millis();
        bool show_ok = fl::flexio_spi_show(buffer,
                                            static_cast<fl::u32>(num_bytes));
        response.set("show_ok", show_ok);
        if (!show_ok) {
            fl::flexio_spi_deinit();
            response.set("success", false);
            response.set("error", "ShowFailed");
            response.set("message", "flexio_spi_show returned false.");
            return response;
        }

        // 4. Wait for completion.
        fl::flexio_spi_wait();
        const fl::u32 wait_ms = millis() - show_start_ms;
        response.set("wait_ms", static_cast<int64_t>(wait_ms));

        // 5. Tear down.
        fl::flexio_spi_deinit();

        response.set("success", true);
        return response;
#endif
    });

    // Register "objectfledSpiSelfTest" — #3428 bring-up smoke test for the
    // ObjectFLED DMA-bit-banged SPI master mode. Exercises the full
    // lookup → init → show → wait → deinit path on a user-specified
    // (MOSI, SCLK) pin pair (both must be on GPIO6) with a known byte
    // pattern, reports back per-stage success + echoed args.
    //
    // Args (positional, all optional):
    //   { "mosi_pin":  int = 14,   // GPIO6 bit 18 on T4.1
    //     "sclk_pin":  int = 15,   // GPIO6 bit 19 on T4.1
    //     "clock_hz":  int = 1000000,
    //     "num_bytes": int = 4 }
    //
    // Returns:
    //   { success, mosi_pin, sclk_pin, clock_hz, num_bytes,
    //     lookup_ok, init_ok, show_ok, wait_ms,
    //     mosi_bit, sclk_bit, dma_* (10), tmr3_* (5), xbar1_ctrl1 }
    //
    // Teensy 4.x-only.
    remote.bind("objectfledSpiSelfTest", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
#if !defined(FL_IS_TEENSY_4X)
        (void)args;
        response.set("success", false);
        response.set("error", "PlatformNotSupported");
        response.set("message",
                     "objectfledSpiSelfTest is Teensy 4.x-only (FlexPWM2 + eDMA + GPIO6).");
        return response;
#else
        // Parse args. The RPC framework strips the outer params array
        // and passes params[0] directly as `args`, so `args` is an OBJECT
        // here (NOT an array).
        int mosi_pin = 14;        // GPIO6 bit 18 on T4.1 (and T4.0)
        int sclk_pin = 15;        // GPIO6 bit 19
        int clock_hz = 1000000;   // 1 MHz -- safe for bring-up
        int num_bytes = 4;
        if (args.is_object()) {
            const fl::json& cfg = args;
            if (cfg.contains("mosi_pin") && cfg["mosi_pin"].is_int()) {
                mosi_pin = static_cast<int>(cfg["mosi_pin"].as_int().value());
            }
            if (cfg.contains("sclk_pin") && cfg["sclk_pin"].is_int()) {
                sclk_pin = static_cast<int>(cfg["sclk_pin"].as_int().value());
            }
            if (cfg.contains("clock_hz") && cfg["clock_hz"].is_int()) {
                clock_hz = static_cast<int>(cfg["clock_hz"].as_int().value());
            }
            if (cfg.contains("num_bytes") && cfg["num_bytes"].is_int()) {
                num_bytes = static_cast<int>(cfg["num_bytes"].as_int().value());
            }
        }

        response.set("mosi_pin", mosi_pin);
        response.set("sclk_pin", sclk_pin);
        response.set("clock_hz", static_cast<int64_t>(clock_hz));
        response.set("num_bytes", num_bytes);

        // Validate input ranges BEFORE narrowing to u8/u32.
        if (num_bytes <= 0 || num_bytes > 64) {
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message", "num_bytes must be in [1, 64].");
            return response;
        }
        if (mosi_pin < 0 || mosi_pin > 255 ||
            sclk_pin < 0 || sclk_pin > 255) {
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message", "mosi_pin / sclk_pin must be in [0, 255].");
            return response;
        }
        if (clock_hz <= 0) {
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message", "clock_hz must be > 0.");
            return response;
        }

        // 1. Pin-pair lookup.
        fl::ObjectFLEDSPIPinInfo pin_info{};
        bool lookup_ok = fl::objectfled_spi_lookup_pins(
            static_cast<fl::u8>(mosi_pin),
            static_cast<fl::u8>(sclk_pin),
            &pin_info);
        response.set("lookup_ok", lookup_ok);
        if (!lookup_ok) {
            response.set("success", false);
            response.set("error", "PinNotRoutable");
            response.set("message",
                         "(MOSI, SCLK) pair is not on GPIO6 or otherwise invalid; "
                         "both pins must be GPIO6-resident (T4.x: 0, 1, 14-27, "
                         "T4.1 adds 38-41).");
            return response;
        }
        response.set("mosi_bit", static_cast<int64_t>(pin_info.mosi_bit));
        response.set("sclk_bit", static_cast<int64_t>(pin_info.sclk_bit));

        // 2. Init.
        bool init_ok = fl::objectfled_spi_init(pin_info,
                                                static_cast<fl::u32>(clock_hz));
        response.set("init_ok", init_ok);
        if (!init_ok) {
            response.set("success", false);
            response.set("error", "InitFailed");
            response.set("message", "objectfled_spi_init returned false.");
            return response;
        }

        // 3. Send a known pattern.
        fl::u8 buffer[64] = {0};
        for (int i = 0; i < num_bytes; ++i) {
            switch (i % 4) {
                case 0: buffer[i] = 0xA5; break;
                case 1: buffer[i] = 0x5A; break;
                case 2: buffer[i] = 0xFF; break;
                case 3: buffer[i] = 0x00; break;
            }
        }

        const fl::u32 show_start_ms = millis();
        bool show_ok = fl::objectfled_spi_show(
            fl::span<const fl::u8>(buffer,
                                   static_cast<fl::size>(num_bytes)));
        response.set("show_ok", show_ok);
        if (!show_ok) {
            fl::objectfled_spi_deinit();
            response.set("success", false);
            response.set("error", "ShowFailed");
            response.set("message", "objectfled_spi_show returned false.");
            return response;
        }

        // 4. Wait for completion.
        fl::objectfled_spi_wait();
        const fl::u32 wait_ms = millis() - show_start_ms;
        response.set("wait_ms", static_cast<int64_t>(wait_ms));

        // 5. Snapshot DMA + QTimer3 + XBAR1 register state.
        fl::ObjectFLEDSPIDiagnostics diag{};
        fl::objectfled_spi_read_diagnostics(&diag);
        response.set("dma_channel", static_cast<int64_t>(diag.dma_channel));
        response.set("dma_erq",     static_cast<int64_t>(diag.dma_erq));
        response.set("dma_int",     static_cast<int64_t>(diag.dma_int));
        response.set("dma_err",     static_cast<int64_t>(diag.dma_err));
        response.set("dma_es",      static_cast<int64_t>(diag.dma_es));
        response.set("dma_citer",   static_cast<int64_t>(diag.dma_citer));
        response.set("dma_biter",   static_cast<int64_t>(diag.dma_biter));
        response.set("tmr3_cntr0",   static_cast<int64_t>(diag.tmr3_cntr0));
        response.set("tmr3_csctrl0", static_cast<int64_t>(diag.tmr3_csctrl0));
        response.set("tmr3_sctrl0",  static_cast<int64_t>(diag.tmr3_sctrl0));
        response.set("tmr3_ctrl0",   static_cast<int64_t>(diag.tmr3_ctrl0));
        response.set("tmr3_enbl",    static_cast<int64_t>(diag.tmr3_enbl));
        response.set("xbar1_ctrl1",  static_cast<int64_t>(diag.xbar1_ctrl1));

        // 6. Tear down.
        fl::objectfled_spi_deinit();

        response.set("success", true);
        return response;
#endif
    });
}

#endif  // FL_PLATFORM_HAS_LARGE_MEMORY
