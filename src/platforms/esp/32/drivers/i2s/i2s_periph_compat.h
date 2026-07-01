// IWYU pragma: private

/// @file i2s_periph_compat.h
/// @brief IDF-version compat shim for the classic-ESP32 I2S peripheral
///        clock/reset enable API.
///
/// **Context (FastLED#3509 Phase 3):**
///
/// The classic-ESP32 I2S clockless driver (`i2s_esp32dev.cpp.hpp`) and
/// its I2S-SPI cousin (`i2s_spi_peripheral_esp.cpp.hpp`) both call
/// `periph_module_enable(PERIPH_I2S{0,1}_MODULE)` to power up the
/// peripheral. That API is REMOVED in ESP-IDF 6.0 (`components/esp_hw_support`
/// deprecation — see IDF-migrations-6.x release notes).
///
/// This header wraps the enable/disable/reset calls behind a stable
/// `fl::fl_i2s_periph_enable(port)` API and dispatches based on
/// `ESP_IDF_VERSION`:
///
/// - **IDF 4.x / 5.x**: `periph_module_enable(PERIPH_I2S{n}_MODULE)`
///   (existing working path — no behavior change).
///
/// - **IDF 6.x+**: `i2s_ll_enable_bus_clock(n, true)` +
///   `i2s_ll_reset_register(n)` via `hal/i2s_ll.h`. The LL API is a
///   **private** IDF header but IDF 6 removed the public alternative,
///   so this is the only path forward. Bench validation on real IDF 6
///   silicon is tracked in FastLED#3509 Phase 7.

#pragma once

#include "platforms/is_platform.h"

#ifdef FL_IS_ESP32

#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "platforms/esp/esp_version.h"

FL_EXTERN_C_BEGIN

// IDF 4.x / 5.x path — periph_module_enable is public API.
#if !ESP_IDF_VERSION_6_OR_HIGHER
// IWYU pragma: begin_keep
#include "driver/periph_ctrl.h"  // for periph_module_enable / periph_module_disable
#include "soc/soc_caps.h"
// IWYU pragma: end_keep
#else
// IDF 6.x+ — periph_module_* was removed. Route through the LL API that
// replaces it. `hal/i2s_ll.h` is a private HAL header but the public
// surface for direct-register users on IDF 6 is precisely this file.
//
// Both `i2s_ll_enable_bus_clock` and `i2s_ll_reset_register` are wrapped
// in `PERIPH_RCC_ATOMIC() { ... }` critical sections per the IDF 6
// convention. The macro-form of both functions in `hal/i2s_ll.h`
// requires `__DECLARE_RCC_ATOMIC_ENV` to be declared in scope, which
// `PERIPH_RCC_ATOMIC()` from `esp_private/periph_ctrl.h` provides.
// (Verified against espressif/esp-idf@master/components/esp_driver_i2s/
// i2s_platform.c which uses the same pattern.)
// IWYU pragma: begin_keep
#include "hal/i2s_ll.h"
#include "esp_private/periph_ctrl.h"  // PERIPH_RCC_ATOMIC() critical section
#include "soc/soc_caps.h"
// IWYU pragma: end_keep
#endif

FL_EXTERN_C_END

namespace fl {

/// @brief Enable clock + reset the I2S peripheral for the given port.
///
/// Idempotent — safe to call multiple times. On IDF 4.x/5.x wraps the
/// public `periph_module_enable(PERIPH_I2S{port}_MODULE)`. On IDF 6.x+
/// wraps `i2s_ll_enable_bus_clock` + `i2s_ll_reset_register` (the LL
/// API is the replacement for the removed periph_module surface).
///
/// @param port  I2S port number, 0 or 1 (classic ESP32 has both).
/// @return true if the peripheral was enabled; false if the port is
///         invalid.
inline bool fl_i2s_periph_enable(int port) FL_NO_EXCEPT {
    if (port != 0 && port != 1) {
        return false;
    }

#if !ESP_IDF_VERSION_6_OR_HIGHER
    // IDF 4.x / 5.x — public periph_module API. This is the path the
    // Yves driver has been using in production since restore (#3479)
    // and is the one #3495-era classic-ESP32 users hit today.
    if (port == 0) {
        periph_module_enable(PERIPH_I2S0_MODULE);
    } else {
        periph_module_enable(PERIPH_I2S1_MODULE);
    }
    return true;
#else
    // IDF 6.x — LL API replaces the removed periph_module surface.
    // Enable the bus clock and reset the peripheral register block.
    // Both calls must live inside `PERIPH_RCC_ATOMIC()` because the
    // macro-form of the functions in `hal/i2s_ll.h` requires the
    // `__DECLARE_RCC_ATOMIC_ENV` variable that macro provides.
    // Same pattern as IDF 6's own components/esp_driver_i2s/
    // i2s_platform.c.
    PERIPH_RCC_ATOMIC() {
        i2s_ll_enable_bus_clock(port, true);
        i2s_ll_reset_register(port);
    }
    return true;
#endif
}

/// @brief Disable the I2S peripheral clock. Called from
///        `deinitialize()` paths. Idempotent.
///
/// @param port  I2S port number.
/// @return true on success, false if port is invalid.
inline bool fl_i2s_periph_disable(int port) FL_NO_EXCEPT {
    if (port != 0 && port != 1) {
        return false;
    }

#if !ESP_IDF_VERSION_6_OR_HIGHER
    if (port == 0) {
        periph_module_disable(PERIPH_I2S0_MODULE);
    } else {
        periph_module_disable(PERIPH_I2S1_MODULE);
    }
    return true;
#else
    PERIPH_RCC_ATOMIC() {
        i2s_ll_enable_bus_clock(port, false);
    }
    return true;
#endif
}

}  // namespace fl

#endif  // FL_IS_ESP32
