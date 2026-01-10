/*

// ok no namespace fl
  FastLED — Parallel Soft-SPI Host Timer Simulation
  --------------------------------------------------
  Timer simulation for host-side testing. Emulates ESP32 hardware timer ISR.
  Uses thread-based automatic ISR execution in separate thread for real-time emulation.

  License: MIT (FastLED)
*/

#include "spi_platform.h"  // Defines FASTLED_SPI_HOST_SIMULATION when STUB_PLATFORM is set

// Compile for host simulation OR stub platform (includes tests)
// STUB_PLATFORM now always gets real ISR implementations instead of stubs
#if defined(FASTLED_SPI_HOST_SIMULATION) || defined(STUB_PLATFORM)

#include "spi_isr_engine.h"
#include "host_sim.h"
#include "fl/stl/stdint.h"

// Thread-based mode: Use real-time ISR emulation
#include <thread>   // ok include (no fl/thread.h wrapper available)
#include <chrono>   // ok include (no fl/chrono.h available)
#include <atomic>   // ok include (for std::atomic_thread_fence)
#include <cstdio>   // ok include (for std::printf)
#include "fl/stl/atomic.h"
#include "fl/stl/mutex.h"
#include "fl/stl/vector.h"

// Simple printf-style debugging for thread mode
#define ISR_DBG(...) std::printf("[ISR_THREAD] " __VA_ARGS__)  // okay std namespace

/* ISR context for thread-based execution */
struct ISRContext {
    uint32_t timer_hz;
    fl::atomic<bool> running;
    fl::atomic<bool> started;  // Signals when thread has begun execution
    std::thread thread;  // okay std namespace

    ISRContext(uint32_t hz) : timer_hz(hz), running(false), started(false) {}
};

/* Global ISR registry (for multi-instance support) */
static fl::vector<ISRContext*>& get_isr_contexts() {
    static fl::vector<ISRContext*> contexts;
    return contexts;
}

static fl::mutex& get_isr_mutex() {
    static fl::mutex mtx;
    return mtx;
}

/* Thread function that runs the ISR at configured frequency */
static void isr_thread_func(ISRContext* ctx) {
    ISR_DBG("Thread started, frequency: %u Hz\n", ctx->timer_hz);

    // Signal that thread has started
    ctx->started.store(true, fl::memory_order_release);

    auto interval = std::chrono::nanoseconds(1000000000 / ctx->timer_hz);  // okay std namespace
    uint32_t tick_count = 0;

    while (ctx->running.load(fl::memory_order_acquire)) {
        auto start = std::chrono::steady_clock::now();  // okay std namespace

        // Acquire fence: ensure we see latest doorbell value from main thread
        std::atomic_thread_fence(std::memory_order_acquire);  // okay std namespace

        // Execute ISR (same code as ESP32 hardware)
        uint32_t flags_before = fl_spi_status_flags();
        fl_parallel_spi_isr();
        fl_gpio_sim_tick();
        uint32_t flags_after = fl_spi_status_flags();
        tick_count++;

        // Release fence: ensure status_flags write is visible to main thread
        std::atomic_thread_fence(std::memory_order_release);  // okay std namespace

        if (tick_count <= 10 || flags_before != flags_after) {
            ISR_DBG("Tick #%u executed, flags: 0x%x -> 0x%x\n", tick_count, flags_before, flags_after);
        }

        // Compensate for ISR execution time
        auto elapsed = std::chrono::steady_clock::now() - start;  // okay std namespace
        auto sleep_time = interval - elapsed;

        if (sleep_time > std::chrono::nanoseconds(0)) {  // okay std namespace
            std::this_thread::sleep_for(sleep_time);  // okay std namespace
        }
    }
    ISR_DBG("Thread stopped after %u ticks\n", tick_count);
}

extern "C" {

/* Start timer (launches ISR thread) */
int fl_spi_platform_isr_start(uint32_t timer_hz) {
    ISR_DBG("fl_spi_platform_isr_start called, frequency: %u Hz\n", timer_hz);
    fl::unique_lock<fl::mutex> lock(get_isr_mutex());

    fl_gpio_sim_init();

    ISRContext* ctx = new ISRContext(timer_hz);
    ctx->running.store(true);

    // Launch thread
    ISR_DBG("Launching thread...\n");
    ctx->thread = std::thread(isr_thread_func, ctx);  // okay std namespace

    // Wait for thread to actually start (prevents race condition)
    while (!ctx->started.load(fl::memory_order_acquire)) {
        std::this_thread::yield();  // okay std namespace
    }
    ISR_DBG("Thread confirmed started\n");

    get_isr_contexts().push_back(ctx);
    ISR_DBG("Thread launched, contexts count: %zu\n", get_isr_contexts().size());
    return 0;  /* Success */
}

/* Stop timer (joins ISR thread) */
void fl_spi_platform_isr_stop(void) {
    ISR_DBG("fl_spi_platform_isr_stop called, contexts count: %zu\n", get_isr_contexts().size());
    fl::unique_lock<fl::mutex> lock(get_isr_mutex());

    for (auto* ctx : get_isr_contexts()) {
        ISR_DBG("Stopping thread...\n");
        ctx->running.store(false);
        if (ctx->thread.joinable()) {
            ISR_DBG("Joining thread...\n");
            ctx->thread.join();
            ISR_DBG("Thread joined\n");
        }
        delete ctx;
    }
    get_isr_contexts().clear();
    ISR_DBG("All threads stopped and cleared\n");
}

/* Query timer state */
bool fl_spi_host_timer_is_running(void) {
    fl::unique_lock<fl::mutex> lock(get_isr_mutex());
    return !get_isr_contexts().empty();
}

uint32_t fl_spi_host_timer_get_hz(void) {
    fl::unique_lock<fl::mutex> lock(get_isr_mutex());
    if (!get_isr_contexts().empty()) {
        return get_isr_contexts()[0]->timer_hz;
    }
    return 0;
}

}  // extern "C"

#endif /* FASTLED_SPI_HOST_SIMULATION */
