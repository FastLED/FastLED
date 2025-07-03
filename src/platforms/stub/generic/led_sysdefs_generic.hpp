#ifdef FASTLED_STUB_IMPL  // Only use this if explicitly defined.

#include "platforms/stub/led_sysdefs_stub.h"
#include "fl/unused.h"
#include "fl/compiler_control.h"


#include <chrono>
#include <thread>
#if defined(FASTLED_USE_PTHREAD_DELAY) || defined(FASTLED_USE_PTHREAD_YIELD)
#include <time.h>
#include <errno.h>
#include <sched.h>
#endif


FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS

static const auto start_time = std::chrono::system_clock::now();

FL_DISABLE_WARNING_POP

void pinMode(uint8_t pin, uint8_t mode) {
    // Empty stub as we don't actually ever write anything
    FASTLED_UNUSED(pin);
    FASTLED_UNUSED(mode);
}

uint32_t millis() {
    auto current_time = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
}

uint32_t micros() {
    auto current_time = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(current_time - start_time).count();
}

void delay(int ms) {
#ifdef FASTLED_USE_PTHREAD_DELAY
    if (ms <= 0) {
        return; // nothing to wait for
    }
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;
    // nanosleep may be interrupted by a signal; retry until the full time has elapsed
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
        // continue sleeping for the remaining time
    }
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

void yield() {
#ifdef FASTLED_USE_PTHREAD_YIELD
    // POSIX thread yield to allow other threads to run
    sched_yield();
#else
    std::this_thread::yield();
#endif
}

#endif  // FASTLED_STUB_IMPL
