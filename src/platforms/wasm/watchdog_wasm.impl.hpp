// IWYU pragma: private
// ok no namespace fl — pure routing header, delegates everything to watchdog_noop.hpp

/// @file platforms/wasm/watchdog_wasm.impl.hpp
/// @brief WASM watchdog implementation.
///
/// **Status: Phase 1 — routes to the shared no-op fallback.**
///
/// The WASM build has emulated ISR / timer infrastructure
/// (`coroutine_runtime_wasm.impl.hpp`, `isr.h`) that can support a real
/// emulated WDT, with `localStorage` / IDBFS backing the persist storage.
/// That work is tracked in FastLED#2731 as a Phase 1.5 follow-up — it needs
/// careful design around the JS event-loop integration to avoid blocking the
/// browser tab. For now we fall through to the no-op so WASM builds compile
/// and run.

#include "platforms/shared/watchdog_noop.hpp"
