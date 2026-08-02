#include "AutoResearchRpConcurrency.h"

#include "platforms/is_platform.h"

#if defined(FL_IS_RP)

#include <Arduino.h>

#include "fl/stl/atomic.h"
#include "fl/stl/mutex.h"
#include "fl/stl/semaphore.h"
#include "fl/stl/static_assert.h"

namespace {

constexpr int kRpConcurrencyIterations = 100;
constexpr uint32_t kRpConcurrencyDeadlineMs = 1500;
constexpr uint32_t kRpCore1ReadyTimeoutMs = 500;
constexpr uint32_t kRpCore1DoneTimeoutMs = 2000;
// AutoResearch's loop watchdog is 5 seconds. Leave a full second for JSON
// response construction and transport after every bounded diagnostic wait.
constexpr uint32_t kRpConcurrencyWatchdogBudgetMs = 4000;
FL_STATIC_ASSERT(kRpConcurrencyDeadlineMs + kRpCore1ReadyTimeoutMs +
                         kRpCore1DoneTimeoutMs <=
                     kRpConcurrencyWatchdogBudgetMs,
                 "RP concurrency diagnostics exceed the watchdog-safe budget");

fl::mutex gRpCounterMutex;
fl::binary_semaphore gRpCore1Request(0);
fl::counting_semaphore<1000> gRpCore1Done(0);
fl::atomic<bool> gRpCore1Ready(false);
fl::atomic<bool> gRpRequestInFlight(false);
fl::atomic<uint32_t> gRpCore1LoopCount(0);
fl::atomic<uint32_t> gRpRequestGeneration(0);
fl::atomic<uint32_t> gRpCompletedGeneration(0);
fl::atomic<int> gRpCore1Iterations(0);
fl::atomic<int> gRpCore1Completed(0);
int gRpSharedCounter = 0;

int incrementRpSharedCounter(int iterations) {
    int completed = 0;
    const uint32_t deadline = millis() + kRpConcurrencyDeadlineMs;
    while (completed < iterations &&
           static_cast<int32_t>(millis() - deadline) < 0) {
        if ((completed & 1) != 0) {
            if (!gRpCounterMutex.try_lock()) {
                continue;
            }
        } else {
            gRpCounterMutex.lock();
        }
        ++gRpSharedCounter;
        gRpCounterMutex.unlock();
        ++completed;
    }
    return completed;
}

} // namespace

// Arduino-Pico runs these entry points on the RP's second physical core.
void setup1() { gRpCore1Ready.store(true); }

void loop1() {
    gRpCore1LoopCount.fetch_add(1);
    if (!gRpCore1Request.try_acquire()) {
        // Avoid a hot atomic/spinlock loop starving shared RP bus resources
        // while retaining an observable core-1 heartbeat.
        delay(1);
        return;
    }

    const uint32_t generation = gRpRequestGeneration.load();
    gRpCore1Completed.store(incrementRpSharedCounter(gRpCore1Iterations.load()));
    gRpCompletedGeneration.store(generation);
    gRpRequestInFlight.store(false);
    gRpCore1Done.release();
}

namespace autoresearch {

fl::json runRpConcurrencyTest() {
    fl::json response = fl::json::object();
    response.set("supported", true);
    response.set("backend", "pico-sdk-mutex+arduino-core1");

    const uint32_t readyDeadline = millis() + kRpCore1ReadyTimeoutMs;
    while (!gRpCore1Ready.load() && static_cast<int32_t>(millis() - readyDeadline) < 0) {
        delay(1);
    }
    if (!gRpCore1Ready.load()) {
        response.set("success", false);
        response.set("error", "RP core1 did not start");
        return response;
    }

    while (gRpCore1Done.try_acquire()) {}
    if (gRpRequestInFlight.exchange(true)) {
        response.set("success", false);
        response.set("error", "RP core1 still owns the prior concurrency request");
        return response;
    }
    if (!gRpCounterMutex.try_lock()) {
        gRpRequestInFlight.store(false);
        response.set("success", false);
        response.set("error", "RP mutex unavailable before contention test");
        return response;
    }
    gRpSharedCounter = 0;
    gRpCounterMutex.unlock();

    gRpCore1Iterations.store(kRpConcurrencyIterations);
    gRpCore1Completed.store(0);
    const uint32_t generation = gRpRequestGeneration.fetch_add(1) + 1;
    const uint32_t core1LoopCountBefore = gRpCore1LoopCount.load();
    gRpCore1Request.release();
    const int core0Completed =
        incrementRpSharedCounter(kRpConcurrencyIterations);

    bool core1Done = false;
    const uint32_t doneDeadline = millis() + kRpCore1DoneTimeoutMs;
    while (static_cast<int32_t>(millis() - doneDeadline) < 0) {
        if (gRpCore1Done.try_acquire()) {
            if (gRpCompletedGeneration.load() == generation) {
                core1Done = true;
                break;
            }
        }
        delay(1);
    }

    if (!gRpCounterMutex.try_lock()) {
        response.set("success", false);
        response.set("error", "RP mutex unavailable after contention test");
        return response;
    }
    const int actual = gRpSharedCounter;
    gRpCounterMutex.unlock();
    const int core1Completed = gRpCore1Completed.load();
    const uint32_t core1LoopCountAfter = gRpCore1LoopCount.load();
    fl::recursive_mutex recursiveMutex;
    recursiveMutex.lock();
    const bool recursiveMutexReady = recursiveMutex.try_lock();
    if (recursiveMutexReady) {
        recursiveMutex.unlock();
    }
    recursiveMutex.unlock();
    const int expected = kRpConcurrencyIterations * 2;
    response.set("success", core1Done &&
                                core0Completed == kRpConcurrencyIterations &&
                                core1Completed == kRpConcurrencyIterations &&
                                actual == expected && recursiveMutexReady);
    response.set("core1Ready", gRpCore1Ready.load());
    response.set("core1Done", core1Done);
    response.set("iterationsPerCore", static_cast<int64_t>(kRpConcurrencyIterations));
    response.set("core0Completed", static_cast<int64_t>(core0Completed));
    response.set("core1Completed", static_cast<int64_t>(core1Completed));
    response.set("core1LoopCountBefore", static_cast<int64_t>(core1LoopCountBefore));
    response.set("core1LoopCountAfter", static_cast<int64_t>(core1LoopCountAfter));
    response.set("generation", static_cast<int64_t>(generation));
    response.set("recursiveMutexReady", recursiveMutexReady);
    response.set("expected", static_cast<int64_t>(expected));
    response.set("actual", static_cast<int64_t>(actual));
    return response;
}

} // namespace autoresearch

#else

namespace autoresearch {

fl::json runRpConcurrencyTest() {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("supported", false);
    response.set("backend", "unsupported");
    response.set("reason", "RP dual-core concurrency is available only on RP2xxx");
    return response;
}

} // namespace autoresearch

#endif
