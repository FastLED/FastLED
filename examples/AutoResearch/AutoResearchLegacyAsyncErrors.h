#pragma once

#include "FastLED.h"
#include "LegacyClocklessProxy.h"
#include "fl/channels/data.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/chrono.h"
#include "fl/stl/json.h"
#include "fl/stl/move.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"

#if AUTORESEARCH_LEGACY_OBSERVES_RMT4_FRAMES && FL_RMT4_ASYNC // ok platform specific - fault fixtures exercise the actual RMT4 worker and shared hardware ledger.
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_async_driver.h" // ok platform headers; ok platform specific - no generic interface can construct an isolated RMT4 worker for failure tests.
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_peripheral_esp.h" // ok platform headers; ok platform specific - these fixtures use the real peripheral, not a mock SDK.
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt_memory_manager_4.h" // ok platform headers; ok platform specific - physical block exhaustion must share the actual TX/RX allocator.
#endif

namespace autoresearch {

#if AUTORESEARCH_LEGACY_OBSERVES_RMT4_FRAMES && FL_RMT4_ASYNC // ok platform specific - these hardware failure fixtures require classic ESP32's RMT4 worker.
namespace legacy_async_errors {

#if defined(FL_RMT4_FAULT_TEST) && FL_RMT4_FAULT_TEST == 1 // ok platform specific - dedicated firmware links one-shot SDK wrappers; production has no fault hooks.
enum class Fault { None, TaskAllocation, MutexAllocation, DriverInstall };
void armFault(Fault fault) FL_NO_EXCEPT;
void disarmFault() FL_NO_EXCEPT;
unsigned consumedFaults() FL_NO_EXCEPT;
#endif

// Reserve only blocks that were free on entry. Existing TX/RX owners keep
// their reservations, and every fixture reservation is restored on exit.
class FreeBlockReservations {
public:
    explicit FreeBlockReservations(fl::RmtMemoryManager4 &manager) // ok platform specific - reserve and audit real RMT4 hardware blocks for exhaustion recovery; no neutral allocator exposes them.
        : mManager(manager) {
        const size_t count = manager.poolSize() / manager.wordsPerBlock(); // ok platform specific - size exhaustion reservations from real RMT4 memory geometry; no neutral allocator exposes it.
        for (size_t block = 0; block < count && block < 32; ++block) {
            if (manager.tryReserveBlocks(static_cast<int>(block), 1)) { // ok platform specific - reserve and audit real RMT4 hardware blocks for exhaustion recovery; no neutral allocator exposes them.
                mReserved |= fl::u32(1) << block;
                ++mCount;
            }
        }
    }
    ~FreeBlockReservations() {
        for (unsigned block = 0; block < 32; ++block) {
            if (mReserved & (fl::u32(1) << block)) {
                mManager.releaseBlocks(static_cast<int>(block), 1); // ok platform specific - reserve and audit real RMT4 hardware blocks for exhaustion recovery; no neutral allocator exposes them.
            }
        }
    }
    unsigned count() const { return mCount; }
    fl::u32 mask() const { return mReserved; }
    FreeBlockReservations(const FreeBlockReservations &) = delete;
    FreeBlockReservations &operator=(const FreeBlockReservations &) = delete;

private:
    fl::RmtMemoryManager4 &mManager; // ok platform specific - reserve and audit real RMT4 hardware blocks for exhaustion recovery; no neutral allocator exposes them.
    fl::u32 mReserved = 0;
    unsigned mCount = 0;
};

inline fl::ChannelDataPtr makeFrame(size_t leds) {
    // GPIO4 is an established classic ESP32 AutoResearch output pad.
    fl::vector_psram<fl::u8> bytes(leds * 3, 0xFF);
    if (bytes.size() != leds * 3) { return {}; }
    return fl::ChannelData::create(4,
        fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>(), fl::move(bytes),
        fl::ChannelPixelFormat::RGB);
}

inline void passiveWindow(fl::u32 durationUs) {
    const fl::u32 started = fl::micros();
    while (fl::micros() - started < durationUs) {
        // No application polling, yielding or waiting services the driver.
    }
}

struct Observation {
    bool leasedAtSubmission = false;
    bool releasedAutonomously = false;
    bool errorReported = false;
    bool completionSucceeded = false;
    fl::u32 translatedBytes = 0;
    fl::u32 workerStackUnusedBytes = 0;
};

inline Observation submit(fl::Rmt4AsyncDriver &driver, // ok platform specific - construct an isolated RMT4 worker for backend-specific failure recovery, unavailable through the neutral factory.
                          const fl::ChannelDataPtr &frame) {
    Observation result;
    const fl::u32 before = driver.txDiagnostics().cumulativeTranslatedBytes; // ok platform specific - observe the selected RMT4 worker internals without adding a generic driver diagnostic API.
    driver.enqueue(frame);
    result.leasedAtSubmission = frame->isInUse();
    driver.show();
    passiveWindow(250000);
    // Observe release before calling either service/completion API.
    result.releasedAutonomously = !frame->isInUse();
    result.errorReported = driver.poll().state ==
        fl::IChannelDriver::DriverState::ERROR;
    result.completionSucceeded = driver.waitDone(1000); // ok platform specific - verify isolated RMT4 worker completion after injected failure; no neutral factory creates this worker.
    result.translatedBytes = driver.txDiagnostics().cumulativeTranslatedBytes - before; // ok platform specific - observe the selected RMT4 worker internals without adding a generic driver diagnostic API.
    result.workerStackUnusedBytes = driver.workerStackHighWaterBytes(); // ok platform specific - observe the selected RMT4 worker internals without adding a generic driver diagnostic API.
    return result;
}

inline void appendObservation(fl::json &result, const Observation &observed) {
    result.set("leasedAtSubmission", observed.leasedAtSubmission);
    result.set("releasedWithoutApplicationPolling", observed.releasedAutonomously);
    result.set("errorReported", observed.errorReported);
    result.set("completionSucceeded", observed.completionSucceeded);
    result.set("translatedBytes", static_cast<int>(observed.translatedBytes));
    result.set("workerStackUnusedBytes", static_cast<int>(observed.workerStackUnusedBytes));
}

inline bool recoverySucceeded(fl::Rmt4AsyncDriver &driver) { // ok platform specific - construct an isolated RMT4 worker for backend-specific failure recovery, unavailable through the neutral factory.
    auto recovery = makeFrame(1);
    if (!recovery) { return false; }
    const auto observed = submit(driver, recovery);
    return observed.leasedAtSubmission && observed.releasedAutonomously &&
        !observed.errorReported && observed.completionSucceeded &&
        (!FL_RMT4_DIAGNOSTICS || observed.translatedBytes == 3); // ok platform specific - this fixture requires RMT4-only worker diagnostics or fault-test configuration, with no neutral equivalent.
}

#if defined(FL_RMT4_FAULT_TEST) && FL_RMT4_FAULT_TEST == 1 // ok platform specific - only fault-injection firmware references the linker wrapper control symbols.
inline fl::json injectedFailureProbe(const fl::string &mode) {
    fl::json result = fl::json::object();
    result.set("mode", mode);
    auto &manager = fl::RmtMemoryManager4::instance(); // ok platform specific - reserve and audit real RMT4 hardware blocks for exhaustion recovery; no neutral allocator exposes them.
    const size_t baseline = manager.allocatedWords(); // ok platform specific - reserve and audit real RMT4 hardware blocks for exhaustion recovery; no neutral allocator exposes them.
    auto frame = makeFrame(1);
    if (!frame) {
        result.set("success", false);
        result.set("error", "Fixture frame allocation failed");
        return result;
    }
    Observation observed;
    bool recovered = false;
    unsigned consumed = 0;
    if (mode == "mutex-allocation") {
        // Only this constructor runs while the next mutex allocation is armed.
        auto peripheral = fl::make_shared<fl::Rmt4PeripheralESP>(); // ok platform specific - construct an isolated RMT4 worker for backend-specific failure recovery, unavailable through the neutral factory.
        armFault(Fault::MutexAllocation);
        {
            fl::Rmt4AsyncDriver driver(peripheral); // ok platform specific - construct an isolated RMT4 worker for backend-specific failure recovery, unavailable through the neutral factory.
            consumed = consumedFaults();
            disarmFault();
            observed = submit(driver, frame);
        }
        // A driver missing its construction-time mutex cannot recover in place.
        fl::Rmt4AsyncDriver replacement(fl::make_shared<fl::Rmt4PeripheralESP>()); // ok platform specific - construct an isolated RMT4 worker for backend-specific failure recovery, unavailable through the neutral factory.
        recovered = recoverySucceeded(replacement);
    } else {
        fl::Rmt4AsyncDriver driver(fl::make_shared<fl::Rmt4PeripheralESP>()); // ok platform specific - construct an isolated RMT4 worker for backend-specific failure recovery, unavailable through the neutral factory.
        armFault(mode == "task-allocation" ? Fault::TaskAllocation : Fault::DriverInstall);
        observed = submit(driver, frame);
        consumed = consumedFaults();
        disarmFault();
        recovered = recoverySucceeded(driver);
    }
    const size_t after = manager.allocatedWords(); // ok platform specific - reserve and audit real RMT4 hardware blocks for exhaustion recovery; no neutral allocator exposes them.
    const bool expectedLease = mode == "mutex-allocation" ?
        !observed.leasedAtSubmission : observed.leasedAtSubmission;
    appendObservation(result, observed);
    result.set("injectedFailuresConsumed", static_cast<int>(consumed));
    result.set("allocatedWordsBefore", static_cast<int>(baseline));
    result.set("allocatedWordsAfter", static_cast<int>(after));
    result.set("ledgerRestored", baseline == after);
    result.set("recovered", recovered);
    result.set("recoveredOnSameDriver", mode != "mutex-allocation" && recovered);
    result.set("success", consumed == 1 && expectedLease &&
        observed.releasedAutonomously && observed.errorReported &&
        !observed.completionSucceeded && recovered && baseline == after);
    return result;
}
#endif

} // namespace legacy_async_errors
#endif

// Modes unavailable/setup use the normal firmware. Timeout/recovery requires
// a dedicated build with FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS=1.
inline fl::json legacyAsyncErrorProbe(const fl::json &args) {
    fl::json result = fl::json::object();
#if AUTORESEARCH_LEGACY_OBSERVES_RMT4_FRAMES && FL_RMT4_ASYNC // ok platform specific - native and other-backend builds return an explicit unsupported result.
    using namespace legacy_async_errors;
    const fl::string mode = args["mode"].as_string().value_or("unavailable");
    result.set("mode", mode);
    const bool injected = mode == "task-allocation" ||
        mode == "mutex-allocation" || mode == "install";
    const bool recoveryMode = mode == "timeout" || mode == "invalid-batch";
    if (mode != "unavailable" && mode != "setup" && !recoveryMode && !injected) {
        result.set("success", false);
        result.set("error", "Unknown legacy asynchronous failure fixture mode");
        return result;
    }
    if (mode == "timeout" &&
        (FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS != 1 || !FL_RMT4_DIAGNOSTICS)) { // ok platform specific - this fixture requires RMT4-only worker diagnostics or fault-test configuration, with no neutral equivalent.
        result.set("success", false);
        result.set("error", "timeout fixture requires FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS=1 and FL_RMT4_DIAGNOSTICS=1");
        return result;
    }
    if (mode == "invalid-batch" && !FL_RMT4_DIAGNOSTICS) { // ok platform specific - this fixture requires RMT4-only worker diagnostics or fault-test configuration, with no neutral equivalent.
        result.set("success", false);
        result.set("error", "invalid-batch fixture requires FL_RMT4_DIAGNOSTICS=1");
        return result;
    }
    if (!FastLED.wait(1000)) {
        result.set("success", false);
        result.set("error", "Existing output did not drain before fixture");
        return result;
    }
    if (injected) {
#if defined(FL_RMT4_FAULT_TEST) && FL_RMT4_FAULT_TEST == 1 // ok platform specific - injected failures require dedicated wrapper-linked test firmware.
        return injectedFailureProbe(mode);
#else
        result.set("success", false);
        result.set("error", "Requires FL_RMT4_FAULT_TEST=1 and the documented SDK linker wrappers");
        return result;
#endif
    }
    auto &manager = fl::RmtMemoryManager4::instance(); // ok platform specific - reserve and audit real RMT4 hardware blocks for exhaustion recovery; no neutral allocator exposes them.
    const size_t baseline = manager.allocatedWords(); // ok platform specific - reserve and audit real RMT4 hardware blocks for exhaustion recovery; no neutral allocator exposes them.
    auto frame = makeFrame(mode == "timeout" ? 4000 : 1);
    if (!frame) {
        result.set("success", false);
        result.set("error", "Fixture frame allocation failed");
        return result;
    }
    Observation observed;
    bool recovered = false;
    unsigned reserved = 0;
    fl::u32 reservationMask = 0;
    if (mode == "unavailable") {
        FreeBlockReservations reservations(manager);
        reserved = reservations.count();
        reservationMask = reservations.mask();
        fl::Rmt4AsyncDriver driver(fl::make_shared<fl::Rmt4PeripheralESP>()); // ok platform specific - construct an isolated RMT4 worker for backend-specific failure recovery, unavailable through the neutral factory.
        observed = submit(driver, frame);
    } else if (mode == "setup") {
        // A missing real peripheral exercises the existing setup-failure path;
        // no SDK return value or production behavior is replaced by a mock.
        fl::Rmt4AsyncDriver driver{fl::shared_ptr<fl::detail::IRMT4Peripheral>()}; // ok platform specific - construct an isolated RMT4 worker for backend-specific failure recovery, unavailable through the neutral factory.
        observed = submit(driver, frame);
    } else {
        fl::Rmt4AsyncDriver driver(fl::make_shared<fl::Rmt4PeripheralESP>()); // ok platform specific - construct an isolated RMT4 worker for backend-specific failure recovery, unavailable through the neutral factory.
        if (mode == "invalid-batch") {
            // The bridge rejects an unencodable strip this way. A subsequent
            // valid strip must not erase that failure from the same batch.
            driver.enqueue(fl::ChannelDataPtr());
        }
        observed = submit(driver, frame);
        recovered = recoverySucceeded(driver);
    }
    const size_t after = manager.allocatedWords(); // ok platform specific - reserve and audit real RMT4 hardware blocks for exhaustion recovery; no neutral allocator exposes them.
    bool reservationsRestored = true;
    for (unsigned block = 0; block < 32; ++block) {
        if (reservationMask & (fl::u32(1) << block)) {
            const bool available = manager.tryReserveBlocks(static_cast<int>(block), 1); // ok platform specific - reserve and audit real RMT4 hardware blocks for exhaustion recovery; no neutral allocator exposes them.
            reservationsRestored &= available;
            if (available) { manager.releaseBlocks(static_cast<int>(block), 1); } // ok platform specific - reserve and audit real RMT4 hardware blocks for exhaustion recovery; no neutral allocator exposes them.
        }
    }
    appendObservation(result, observed);
    result.set("reservedFreeBlocks", static_cast<int>(reserved));
    result.set("allocatedWordsBefore", static_cast<int>(baseline));
    result.set("allocatedWordsAfter", static_cast<int>(after));
    result.set("ledgerRestored", baseline == after);
    result.set("fixtureReservationsRestored", reservationsRestored);
    result.set("recoveryAttempted", recoveryMode);
    result.set("recoveredOnSameDriver", recoveryMode && recovered);
    result.set("success", observed.leasedAtSubmission &&
        observed.releasedAutonomously && observed.errorReported &&
        !observed.completionSucceeded && baseline == after &&
        (mode != "timeout" ||
            (observed.translatedBytes > 0 && observed.translatedBytes < frame->getSize())) &&
        (mode != "invalid-batch" || observed.translatedBytes == 3) &&
        reservationsRestored && (!recoveryMode || recovered));
#else
    (void)args;
    result.set("success", false);
    result.set("error", "Requires asynchronous classic ESP32 RMT4 output");
#endif
    return result;
}

} // namespace autoresearch
