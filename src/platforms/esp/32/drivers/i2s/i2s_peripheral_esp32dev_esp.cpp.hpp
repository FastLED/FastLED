// IWYU pragma: private

// FastLED#3512 Phase 5: this file is now part of the i2s unity build.
// The earlier UNITY_BUILD_EXCLUDE marker cited a `driver/gpio.h` collision
// against the restored classic Yves driver; on re-investigation the
// actual `ADC: CONFLICT!` issue was `driver/i2s.h` in the Stage 2
// (#3476) impl. This Stage 4 file drops `driver/i2s.h` entirely (see
// the docblock below) so the conflict no longer applies. The stub
// `transmit()` currently does no DMA — Phase 2 replaces it with real
// register + DMA + ISR machinery reused from the classic driver.

/// @file i2s_peripheral_esp32dev_esp.cpp.hpp
/// @brief Real-hardware `II2sPeripheralEsp32Dev` impl — Stage 4 rewrite
///        avoids IDF's `driver/i2s.h` to escape the `driver_ng` vs
///        legacy-ADC conflict (#3474).
///
/// ## Why not `driver/i2s.h`
///
/// Stage 2 (#3476) used the legacy `driver/i2s.h`. Wiring that TU
/// into the classic-ESP32 unity build tripped a hard `ADC:
/// CONFLICT! driver_ng is not allowed to be used with the legacy
/// driver` panic at IDF init time — the include drags in the
/// `driver_ng` framework, and something in the link (arduino-esp32
/// 3.3.7 core, or FastLED's `pin_esp32_native_impl.hpp` on the same
/// codepath) had already registered the legacy ADC driver.
///
/// Stage 4 dodges the ecosystem conflict by dropping the
/// `driver/i2s.h` include entirely. Only very-low-level headers
/// remain:
///
///   - `esp_heap_caps.h` — DMA-capable buffer alloc.
///   - `driver/gpio.h` — GPIO enable (no ADC interaction).
///
/// These are the same low-level headers that AutoResearch and every
/// other classic-ESP32 driver in the tree use without triggering the
/// conflict. The peripheral wraps this narrow surface so the engine
/// can drive it exactly the way the mock does.
///
/// ## Scope of the Stage 4 v1 impl
///
/// Sufficient to satisfy "peripheral wired end-to-end" and pass the
/// engine tests on hardware:
///
/// - `initialize()` accepts the config, stashes it, sets the
///   initialized flag.
/// - `allocateBuffer()` / `freeBuffer()` back onto DMA-capable heap.
/// - `transmit()` accepts the byte buffer and fires the registered
///   callback synchronously (mock-equivalent semantics — the engine
///   walks through BUSY → poll → READY the same way).
/// - `isBusy()` / `waitTransmitDone()` reflect the ready state.
///
/// The full DMA descriptor chain, ISR install, register-level
/// clock/pin setup on `I2S1`, and the real WS2812 waveform emit are
/// Stage 5. The peripheral surface is stable across that upgrade —
/// only this file's bodies change.

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "fl/stl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_I2S

#include "platforms/esp/32/drivers/i2s/i2s_peripheral_esp32dev_esp.h"
#include "platforms/esp/32/drivers/i2s/i2s_esp32dev.h"  // FastLED#3526 Phase 2b — real hardware init/start/wait

#include "fl/log/log.h"
#include "fl/stl/noexcept.h"
#include "fl/chipsets/led_timing.h"

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {

//=============================================================================
// Ctor / dtor
//=============================================================================

I2sPeripheralEsp32DevEsp::I2sPeripheralEsp32DevEsp() FL_NO_EXCEPT
    : mConfig(),
      mInitialized(false),
      mBusy(false),
      mCallback(nullptr),
      mCallbackUserCtx(nullptr) {}

I2sPeripheralEsp32DevEsp::~I2sPeripheralEsp32DevEsp() {
    if (mInitialized) {
        deinitialize();
    }
}

//=============================================================================
// Lifecycle
//=============================================================================

bool I2sPeripheralEsp32DevEsp::initialize(
    const I2sEsp32DevPeripheralConfig &cfg) FL_NO_EXCEPT {
    if (mInitialized) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: already initialized");
        return false;
    }
    // FastLED#3526 Phase 2b: hardware-ownership check — the classic
    // Yves `ClocklessI2S<>` template on `-DFASTLED_ESP32_I2S=1` uses
    // the same I2S1 peripheral via file-scope globals; both cannot own
    // the hardware simultaneously. First-in-wins.
    if (i2s_is_initialized()) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: I2S1 already claimed by classic ClocklessI2S — refusing to bind");
        return false;
    }
    mConfig = cfg;

    // FastLED#3526 Phase 2b — real hardware init. Uses the tested Yves
    // C-style utilities from `i2s_esp32dev.h` to configure the I2S1
    // peripheral registers + DMA descriptor allocation + ISR install.
    // Once Phase 2e deletes the Yves driver, these utility functions
    // become inline in this file. Chipset timing defaults to canonical
    // WS2812B until the modern engine adds a `configureTiming()`
    // method that plumbs per-channel `ChipsetTimingConfig` through.
    const ChipsetTiming default_timing =
        to_runtime_timing<TIMING_WS2812_800KHZ>();
    i2s_define_bit_patterns(default_timing);
    i2s_init(static_cast<int>(mConfig.mI2sPort));

    mInitialized = true;
    mBusy = false;
    return true;
}

void I2sPeripheralEsp32DevEsp::deinitialize() FL_NO_EXCEPT {
    if (!mInitialized) {
        return;
    }
    // No hardware state to release — Stage 5 wires that in.
    mInitialized = false;
    mBusy = false;
}

bool I2sPeripheralEsp32DevEsp::isInitialized() const FL_NO_EXCEPT {
    return mInitialized;
}

//=============================================================================
// Buffer management
//=============================================================================

u8 *I2sPeripheralEsp32DevEsp::allocateBuffer(size_t size_bytes) FL_NO_EXCEPT {
    if (size_bytes == 0) {
        return nullptr;
    }
    // DMA-capable DRAM. `MALLOC_CAP_DMA` returns 4-byte-aligned
    // blocks by default, which is enough for the I2S FIFO's 32-bit
    // read granularity once Stage 5 turns on the DMA descriptor path.
    void *p = heap_caps_malloc(size_bytes, MALLOC_CAP_DMA);
    return static_cast<u8 *>(p);
}

void I2sPeripheralEsp32DevEsp::freeBuffer(u8 *buffer) FL_NO_EXCEPT {
    if (buffer != nullptr) {
        heap_caps_free(buffer);
    }
}

//=============================================================================
// Transmission
//=============================================================================

bool I2sPeripheralEsp32DevEsp::transmit(const u8 *buffer,
                                        size_t size_bytes) FL_NO_EXCEPT {
    if (!mInitialized || buffer == nullptr || size_bytes == 0) {
        return false;
    }
    if (mBusy) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: transmit while busy");
        return false;
    }

    // Stage 4 v1: accept the buffer and fire the completion callback
    // synchronously — the engine's state machine advances BUSY →
    // poll() → READY exactly as it would with the real ISR path, and
    // the same code path the mock's `simulateTransmitDone()` drives.
    // Stage 5 replaces this fake-async arm with a real DMA descriptor
    // kick + ISR-context callback.
    (void)buffer;
    (void)size_bytes;

    mBusy = false;
    if (mCallback != nullptr) {
        (*mCallback)(mCallbackUserCtx);
    }
    return true;
}

bool I2sPeripheralEsp32DevEsp::waitTransmitDone(u32 timeout_ms) FL_NO_EXCEPT {
    (void)timeout_ms;
    return !mBusy;
}

bool I2sPeripheralEsp32DevEsp::isBusy() const FL_NO_EXCEPT { return mBusy; }

//=============================================================================
// Callback registration
//=============================================================================

bool I2sPeripheralEsp32DevEsp::registerTransmitCallback(
    I2sEsp32DevTxDoneCallback cb, void *user_ctx) FL_NO_EXCEPT {
    mCallback = cb;
    mCallbackUserCtx = user_ctx;
    return true;
}

//=============================================================================
// Introspection
//=============================================================================

const I2sEsp32DevPeripheralConfig &
I2sPeripheralEsp32DevEsp::getConfig() const FL_NO_EXCEPT {
    return mConfig;
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_I2S
#endif // FL_IS_ESP32
