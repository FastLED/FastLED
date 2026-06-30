/// @file ilpuart_peripheral.h
/// @brief Abstract interface for Teensy 4.x iMXRT1062 LPUART peripheral
///        (issue #3023 workstream A).
///
/// `Bus::UART` is the Teensy 4.x LPUART clockless peripheral slot
/// (alongside the `Bus::FLEX_IO` slots). The driver uses the
/// LPUART hardware in inverted-TX mode with eDMA backing so a single byte
/// frame on the wire decomposes into 8 freely-programmable pulses sandwiched
/// between a HIGH leading idle and a LOW trailing transition — the exact
/// topology of every WS281x-family / SK6812 / TM181x chipset (verified by
/// the `wave8` framing invariant test in `tests/fl/channels/wave8.cpp`).
///
/// This header is the platform-agnostic seam between:
///
///   - `LPUARTPeripheralReal` (concrete iMXRT1062 register driver — landed in
///     a follow-up PR; needs hardware verification), and
///   - `LPUARTPeripheralMock` (host-side stub used by unit tests that exercise
///     `ChannelEngineLPUART` without requiring real hardware).
///
/// The interface is intentionally narrow — single TX pin per instance, a
/// single contiguous DMA buffer, and a synchronous `show()` for Phase 1.
/// Async streaming (#3023 workstream D) will fold in `isBusy()` / `poll()`
/// follow-ups via interface extension or a sibling interface, *not* by
/// reshaping the existing surface.
///
/// **No engine code in this PR.** This header only defines the abstract
/// seam plus the mock — registration with `ChannelManager`, the wave8
/// encoder, the real iMXRT register driver, and `canHandle()` accept/reject
/// logic land in subsequent PRs against the meta issue.

#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Result of pin validation against the iMXRT1062 LPUART TX pin set.
///
/// `valid == false` and `error_message != nullptr` always travel together —
/// the engine surfaces `error_message` via `ChannelManager::reportError` so
/// users see a specific reason (e.g. "pin not LPUART-TX capable") rather
/// than a generic refusal.
struct LPUARTPinResult {
    bool valid;
    const char* error_message;
};

/// @brief Compile-time list of Teensy 4.0 / 4.1 LPUART TX-capable pins.
///
/// Sourced from the iMXRT1062 reference manual's IOMUXC pad-mux table:
///   pin 1  → LPUART6_TX (Serial1 default — also Teensyduino's default)
///   pin 8  → LPUART4_TX (Serial2 default on T4.0)
///   pin 14 → LPUART2_TX (Serial3 default)
///   pin 17 → LPUART3_TX (Serial4 default on T4.0)
///   pin 20 → LPUART8_TX (Serial5 default)
///   pin 24 → LPUART1_TX (Serial6 default — fixed-route, no SELECT_INPUT daisy)
///   pin 29 → LPUART7_TX (Serial7 default)
///   pin 35 → LPUART5_TX (T4.1 only — ALT 1, not 2)
///   pin 47 → LPUART8_TX_ALT (T4.1 only — second SD-pad-row LPUART8)
///   pin 53 → LPUART6_TX_ALT (T4.1 only — Serial1 alt on bottom EMC pad row)
///
/// Note: the original `{1,8,14,17,20,24,29,35,47,48}` list (#3023
/// Phase 1) listed pin 48 as a TX pin. Re-audit against
/// Teensyduino HardwareSerial8.cpp showed pin 48 is the T4.1
/// LPUART5_RX_ALT, not a TX pin -- the correct T4.1 third alt-TX is
/// pin 53 (LPUART6_TX_ALT). Driver and contract were corrected
/// together in PR #3421.
///
/// Mirrors workstream A Phase 1 of issue #3023. Validating against this
/// array is the basis of `LPUARTPeripheralReal::validatePin`; the mock
/// accepts the same set by default but lets tests override via
/// `setInvalidPin()` to exercise error paths.
constexpr u8 kLPUARTTxPins[] = {1, 8, 14, 17, 20, 24, 29, 35, 47, 53};

/// @brief Number of LPUART-TX capable pins exposed on Teensy 4.x.
constexpr u32 kLPUARTTxPinCount = sizeof(kLPUARTTxPins) / sizeof(kLPUARTTxPins[0]);

/// @brief Abstract handle for a single LPUART TX instance bound to one TX pin.
///
/// Owns a single contiguous TX buffer that `ChannelEngineLPUART` writes
/// pre-encoded (wave8 → UART byte) data into. `show()` triggers a
/// synchronous eDMA transmission on the real implementation and a
/// capture-only no-op on the mock.
///
/// Lifetime is owned by the channel engine (via `unique_ptr`). Tearing
/// down the instance releases the LPUART hardware lock back to the
/// peripheral so a different pin can bind on the next `addLeds()` cycle.
class ILPUARTInstance {
public:
    virtual ~ILPUARTInstance() = default;

    /// @brief Get a writable pointer to the TX buffer (wave8-encoded UART bytes).
    virtual u8* getTxBuffer() FL_NO_EXCEPT = 0;

    /// @brief Get the TX buffer size in bytes.
    virtual u32 getTxBufferSize() const FL_NO_EXCEPT = 0;

    /// @brief Trigger a synchronous eDMA transmission. Blocks until done.
    virtual void show() FL_NO_EXCEPT = 0;

protected:
    ILPUARTInstance() = default;
    ILPUARTInstance(const ILPUARTInstance&) = delete;
    ILPUARTInstance& operator=(const ILPUARTInstance&) = delete;
};

/// @brief Abstract factory/validator for the LPUART peripheral.
///
/// Concrete implementations:
///   - `LPUARTPeripheralReal`: real iMXRT1062 register + eDMA driver. Lands
///     in a follow-up PR that adds the engine and its hardware backend.
///   - `LPUARTPeripheralMock`: host-side stub used by unit tests. Captures
///     `createInstance()` records and the per-pin invalid-pin list.
///
/// `create()` is the single ODR-use seam — the engine takes the
/// `shared_ptr` it returns and never re-resolves. Tests construct a mock
/// directly via `make_shared<LPUARTPeripheralMock>()` and inject it
/// without going through `create()`.
class ILPUARTPeripheral {
public:
    virtual ~ILPUARTPeripheral() = default;

    /// @brief Validate whether a pin can carry LPUART TX on the target board.
    /// @param pin Teensy digital pin number.
    /// @return Validation result with error message if invalid.
    virtual LPUARTPinResult validatePin(u8 pin) const FL_NO_EXCEPT = 0;

    /// @brief Create an LPUART TX instance bound to a single pin.
    /// @param tx_pin Digital pin number (must be in `kLPUARTTxPins`).
    /// @param total_leds Number of LEDs on this strip.
    /// @param is_rgbw `true` if the chipset is RGBW (4 bytes/LED).
    /// @param t1_ns   T0H phase duration (nanoseconds).
    /// @param t2_ns   T1H-T0H phase duration (nanoseconds).
    /// @param t3_ns   T0L tail-LOW duration (nanoseconds).
    /// @param reset_us Reset/latch pulse duration (microseconds).
    /// @return Instance handle, or `nullptr` on failure.
    virtual fl::unique_ptr<ILPUARTInstance> createInstance(
        u8 tx_pin, u32 total_leds, bool is_rgbw,
        u32 t1_ns, u32 t2_ns, u32 t3_ns, u32 reset_us) FL_NO_EXCEPT = 0;

    /// @brief Get the platform-specific peripheral instance. Implemented in
    ///        the real driver TU (Teensy 4.x) and the host stub TU (tests).
    static fl::shared_ptr<ILPUARTPeripheral> create() FL_NO_EXCEPT;

protected:
    ILPUARTPeripheral() = default;
    ILPUARTPeripheral(const ILPUARTPeripheral&) = delete;
    ILPUARTPeripheral& operator=(const ILPUARTPeripheral&) = delete;
};

} // namespace fl
