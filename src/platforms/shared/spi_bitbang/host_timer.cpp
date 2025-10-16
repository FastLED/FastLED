/*
  FastLED — Parallel Soft-SPI Host Timer Simulation
  --------------------------------------------------
  Timer simulation for host-side testing. Emulates ESP32 hardware timer ISR.

  Two modes available:
  1. Manual tick mode (FASTLED_SPI_MANUAL_TICK): Tests drive ISR via fl_spi_host_simulate_tick()
  2. Thread mode (default): Automatic ISR execution in separate thread for real-time emulation

  License: MIT (FastLED)
*/

#ifdef FASTLED_SPI_HOST_SIMULATION

#include "spi_isr_engine.h"
#include "host_sim.h"
#include <stdint.h>

#ifndef FASTLED_SPI_MANUAL_TICK
// Thread-based mode: Use real-time ISR emulation
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include "fl/vector.h"

// Simple printf-style debugging for thread mode
#include <stdio.h>
#define ISR_DBG(...) printf("[ISR_THREAD] " __VA_ARGS__)

namespace {

/* ISR context for thread-based execution */
struct ISRContext {
    uint32_t timer_hz;
    std::atomic<bool> running;
    std::atomic<bool> started;  // Signals when thread has begun execution
    std::thread thread;

    ISRContext(uint32_t hz) : timer_hz(hz), running(false), started(false) {}
};

/* Global ISR registry (for multi-instance support) */
fl::HeapVector<ISRContext*> g_isr_contexts;
std::mutex g_isr_mutex;

/* Thread function that runs the ISR at configured frequency */
void isr_thread_func(ISRContext* ctx) {
    ISR_DBG("Thread started, frequency: %u Hz\n", ctx->timer_hz);

    // Signal that thread has started
    ctx->started.store(true, std::memory_order_release);

    auto interval = std::chrono::nanoseconds(1'000'000'000 / ctx->timer_hz);
    uint32_t tick_count = 0;

    while (ctx->running.load(std::memory_order_acquire)) {
        auto start = std::chrono::steady_clock::now();

        // Acquire fence: ensure we see latest doorbell value from main thread
        std::atomic_thread_fence(std::memory_order_acquire);

        // Execute ISR (same code as ESP32 hardware)
        uint32_t flags_before = fl_spi_status_flags();
        fl_parallel_spi_isr();
        fl_gpio_sim_tick();
        uint32_t flags_after = fl_spi_status_flags();
        tick_count++;

        // Release fence: ensure status_flags write is visible to main thread
        std::atomic_thread_fence(std::memory_order_release);

        if (tick_count <= 10 || flags_before != flags_after) {
            ISR_DBG("Tick #%u executed, flags: 0x%x -> 0x%x\n", tick_count, flags_before, flags_after);
        }

        // Compensate for ISR execution time
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto sleep_time = interval - elapsed;

        if (sleep_time > std::chrono::nanoseconds(0)) {
            std::this_thread::sleep_for(sleep_time);
        }
    }
    ISR_DBG("Thread stopped after %u ticks\n", tick_count);
}

}  // anonymous namespace

extern "C" {

/* Start timer (launches ISR thread) */
int fl_spi_platform_isr_start(uint32_t timer_hz) {
    ISR_DBG("fl_spi_platform_isr_start called, frequency: %u Hz\n", timer_hz);
    std::lock_guard<std::mutex> lock(g_isr_mutex);

    fl_gpio_sim_init();

    ISRContext* ctx = new ISRContext(timer_hz);
    ctx->running.store(true);

    // Launch thread
    ISR_DBG("Launching thread...\n");
    ctx->thread = std::thread(isr_thread_func, ctx);

    // Wait for thread to actually start (prevents race condition)
    while (!ctx->started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    ISR_DBG("Thread confirmed started\n");

    g_isr_contexts.push_back(ctx);
    ISR_DBG("Thread launched, contexts count: %zu\n", g_isr_contexts.size());
    return 0;  /* Success */
}

/* Stop timer (joins ISR thread) */
void fl_spi_platform_isr_stop(void) {
    ISR_DBG("fl_spi_platform_isr_stop called, contexts count: %zu\n", g_isr_contexts.size());
    std::lock_guard<std::mutex> lock(g_isr_mutex);

    for (auto* ctx : g_isr_contexts) {
        ISR_DBG("Stopping thread...\n");
        ctx->running.store(false);
        if (ctx->thread.joinable()) {
            ISR_DBG("Joining thread...\n");
            ctx->thread.join();
            ISR_DBG("Thread joined\n");
        }
        delete ctx;
    }
    g_isr_contexts.clear();
    ISR_DBG("All threads stopped and cleared\n");
}

/* Query timer state */
bool fl_spi_host_timer_is_running(void) {
    std::lock_guard<std::mutex> lock(g_isr_mutex);
    return !g_isr_contexts.empty();
}

uint32_t fl_spi_host_timer_get_hz(void) {
    std::lock_guard<std::mutex> lock(g_isr_mutex);
    if (!g_isr_contexts.empty()) {
        return g_isr_contexts[0]->timer_hz;
    }
    return 0;
}

}  // extern "C"

#else  // FASTLED_SPI_MANUAL_TICK

/* Manual tick mode: Tests drive ISR manually for deterministic timing */

static bool g_timer_running = false;
static uint32_t g_timer_hz = 0;

extern "C" {

/* Start timer (initializes host simulation) */
int fl_spi_platform_isr_start(uint32_t timer_hz) {
    g_timer_hz = timer_hz;
    g_timer_running = true;
    fl_gpio_sim_init();
    return 0;  /* Success */
}

/* Stop timer */
void fl_spi_platform_isr_stop(void) {
    g_timer_running = false;
}

/* Test harness calls this to simulate timer tick (mock ESP32 timer ISR) */
void fl_spi_host_simulate_tick(void) {
    if (g_timer_running) {
        fl_parallel_spi_isr();  /* Call ISR (same code as ESP32) */
        fl_gpio_sim_tick();     /* Advance time in ring buffer */
    }
}

/* Query timer state */
bool fl_spi_host_timer_is_running(void) {
    return g_timer_running;
}

uint32_t fl_spi_host_timer_get_hz(void) {
    return g_timer_hz;
}

}  // extern "C"

#endif  // FASTLED_SPI_MANUAL_TICK

#endif /* FASTLED_SPI_HOST_SIMULATION */
