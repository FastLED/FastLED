// IWYU pragma: private

/*

// ok no namespace fl
  FastLED — Parallel Soft-SPI Host Timer Simulation
  --------------------------------------------------
  Timer simulation for host-side testing. Emulates ESP32 hardware timer ISR.
  Uses thread-based automatic ISR execution in separate thread for real-time emulation.

  License: MIT (FastLED)
*/

#include "platforms/shared/spi_bitbang/spi_platform.h"  // Defines FASTLED_SPI_HOST_SIMULATION when STUB_PLATFORM is set

// Compile for host simulation OR stub platform (includes tests)
// STUB_PLATFORM now always gets real ISR implementations instead of stubs
#if defined(FASTLED_SPI_HOST_SIMULATION) || defined(STUB_PLATFORM)

#include "platforms/shared/spi_bitbang/spi_isr_engine.h"
#include "platforms/shared/spi_bitbang/host_sim.h"
#include "fl/stl/stdint.h"

// Thread-based mode: Use real-time ISR emulation
// IWYU pragma: begin_keep
#include <atomic>   // ok include (for std::atomic_thread_fence)
// IWYU pragma: end_keep
#include "fl/stl/thread.h"
#include "fl/stl/chrono.h"
#include "fl/stl/cstdio.h"
#include "fl/stl/stdio.h"  // for fl::printf used by ISR_DBG
#include "fl/stl/atomic.h"
#include "fl/stl/mutex.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

// Simple printf-style debugging for thread mode
#define ISR_DBG(...) fl::printf("[ISR_THREAD] " __VA_ARGS__)

/* ISR context for thread-based execution */
struct ISRContext {
    fl::u32 timer_hz;
    fl::atomic<bool> running;
    fl::atomic<bool> started;  // Signals when thread has begun execution
    fl::thread thread;

    ISRContext(fl::u32 hz) FL_NOEXCEPT : timer_hz(hz), running(false), started(false) {}
};

/* Global ISR registry (for multi-instance support) */
static fl::vector<ISRContext*>& get_isr_contexts() FL_NOEXCEPT {
    static fl::vector<ISRContext*> contexts;
    return contexts;
}

static fl::mutex& get_isr_mutex() FL_NOEXCEPT {
    static fl::mutex mtx;
    return mtx;
}

/* Thread function that runs the ISR at configured frequency */
static void isr_thread_func(ISRContext* ctx) FL_NOEXCEPT {
    ISR_DBG("Thread started, frequency: %u Hz\n", ctx->timer_hz);

    // Signal that thread has started
    ctx->started.store(true, fl::memory_order_release);

    auto interval = std::chrono::nanoseconds(1000000000 / ctx->timer_hz);  // okay std namespace (mixed-period arithmetic needs std::chrono)
    fl::u32 tick_count = 0;

    while (ctx->running.load(fl::memory_order_acquire)) {
        auto start = std::chrono::steady_clock::now();  // okay std namespace (mixed-period arithmetic needs std::chrono)

        // Acquire fence: ensure we see latest doorbell value from main thread
        std::atomic_thread_fence(std::memory_order_acquire);  // okay std namespace

        // Execute ISR (same code as ESP32 hardware)
        fl::u32 flags_before = fl_spi_status_flags();
        fl_parallel_spi_isr();
        fl_gpio_sim_tick();
        fl::u32 flags_after = fl_spi_status_flags();
        tick_count++;

        // Release fence: ensure status_flags write is visible to main thread
        std::atomic_thread_fence(std::memory_order_release);  // okay std namespace

        if (tick_count <= 10 || flags_before != flags_after) {
            ISR_DBG("Tick #%u executed, flags: 0x%x -> 0x%x\n", tick_count, flags_before, flags_after);
        }

        // Compensate for ISR execution time
        auto elapsed = std::chrono::steady_clock::now() - start;  // okay std namespace (mixed-period arithmetic needs std::chrono)
        auto sleep_time = interval - elapsed;

        if (sleep_time > std::chrono::nanoseconds(0)) {  // okay std namespace (mixed-period arithmetic needs std::chrono)
            std::this_thread::sleep_for(sleep_time);  // okay std namespace (std::chrono duration)
        }
    }
    ISR_DBG("Thread stopped after %u ticks\n", tick_count);
}

extern "C" {

/* Start timer (launches ISR thread) */
int fl_spi_platform_isr_start(fl::u32 timer_hz) FL_NOEXCEPT {
    ISR_DBG("fl_spi_platform_isr_start called, frequency: %u Hz\n", timer_hz);
    fl::unique_lock<fl::mutex> lock(get_isr_mutex());

    fl_gpio_sim_init();

    ISRContext* ctx = new ISRContext(timer_hz);  // ok bare allocation
    ctx->running.store(true);

    // Launch thread
    ISR_DBG("Launching thread...\n");
    ctx->thread = fl::thread(isr_thread_func, ctx);

    // Wait for thread to actually start (prevents race condition)
    while (!ctx->started.load(fl::memory_order_acquire)) {
        fl::this_thread::yield();
    }
    ISR_DBG("Thread confirmed started\n");

    get_isr_contexts().push_back(ctx);
    ISR_DBG("Thread launched, contexts count: %zu\n", get_isr_contexts().size());
    return 0;  /* Success */
}

/* Stop timer (joins ISR thread) */
void fl_spi_platform_isr_stop(void) FL_NOEXCEPT {
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
        delete ctx;  // ok bare allocation
    }
    get_isr_contexts().clear();
    ISR_DBG("All threads stopped and cleared\n");
}

/* Query timer state */
bool fl_spi_host_timer_is_running(void) FL_NOEXCEPT {
    fl::unique_lock<fl::mutex> lock(get_isr_mutex());
    return !get_isr_contexts().empty();
}

fl::u32 fl_spi_host_timer_get_hz(void) FL_NOEXCEPT {
    fl::unique_lock<fl::mutex> lock(get_isr_mutex());
    if (!get_isr_contexts().empty()) {
        return get_isr_contexts()[0]->timer_hz;
    }
    return 0;
}

}  // extern "C"

#endif /* FASTLED_SPI_HOST_SIMULATION */
