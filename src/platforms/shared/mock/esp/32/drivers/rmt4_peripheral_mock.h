/// @file rmt4_peripheral_mock.h
/// @brief Mock RMT4 peripheral for host-side unit testing (#3462).
///
/// Follows the `Rmt5PeripheralMock` / `UartPeripheralMock` pattern:
///
/// - Abstract public class. Concrete `Rmt4PeripheralMockImpl` lives in
///   the `.cpp.hpp` so test TUs never see ESP-IDF types.
/// - Singleton via `fl::Singleton<Impl>` — there is only one RMT
///   peripheral on real silicon, so the mock matches that.
/// - **Single-threaded by design.** No `std::thread`, no
///   `std::condition_variable`, no async callbacks. Tests advance
///   simulation manually via `simulateThresholdInterrupt()` /
///   `simulateTxDoneInterrupt()` which invoke the stored ISR handler
///   synchronously on the test thread.
/// - Error-injection knobs for each lifecycle entry point so the
///   engine's negative-path branches can be exercised.
///
/// ## Why no threads
///
/// The engine's ISR is `void(*)(void* arg)` and the FastLED unit-test
/// framework runs tests sequentially. Spawning a thread inside the
/// mock to "deliver" the interrupt would (a) introduce races between
/// the test thread and the simulation thread, (b) defeat the
/// deterministic ordering FL_CHECK assertions depend on, and (c) bind
/// every test to a `join()` or `sleep()` for timing. The peer mocks
/// (`Rmt5PeripheralMock`, `UartPeripheralMock`, the FlexIO one, etc.)
/// all deliberately stay on the test thread; this one does too.
///
/// ## Usage
///
/// ```cpp
/// auto& mock = Rmt4PeripheralMock::instance();
/// mock.reset();
///
/// detail::Rmt4ChannelConfig cfg(0, 18, 2, 2, 64);
/// FL_CHECK(mock.configureChannel(cfg));
/// FL_CHECK(mock.installDriver(0));
///
/// void* cookie = nullptr;
/// FL_CHECK(mock.installIsr(&my_handler, &my_ctx, &cookie));
/// FL_CHECK(cookie != nullptr);
///
/// // Fire the ISR synchronously from the test thread.
/// mock.simulateThresholdInterrupt(0);
/// ```
///
/// ## What this mock does NOT cover
///
/// The engine's IRAM ISR (`ChannelEngineRMT4Impl::handleInterrupt`)
/// reads `RMT.int_st.val` and writes `RMT.int_clr.val` directly —
/// those globals don't exist on the host. So integration tests that
/// instantiate `ChannelEngineRMT4Impl` against this mock are blocked
/// on a separate "host-build support for RMT register stubs" task,
/// not on the mock. This file covers the **mock's own contract** in
/// full; engine-level integration is a follow-up.

#pragma once

// IWYU pragma: private

// Mock implementation has no platform guards — runs on all platforms
// for testing. Mirrors `Rmt5PeripheralMock`.

#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/irmt4_peripheral.h"

namespace fl {
namespace detail {

//=============================================================================
// Mock Peripheral Interface (abstract — concrete impl is in .cpp.hpp)
//=============================================================================

/// @brief Recorded state for one RMT channel in the mock.
///
/// Tests use this to assert that the engine made the right lifecycle
/// calls with the right arguments. Mirrors the per-channel hardware
/// state that the real `Rmt4PeripheralESP` would push into silicon.
struct Rmt4ChannelMockState {
    Rmt4ChannelConfig last_config;
    bool driver_installed;
    bool threshold_intr_enabled;
    u16 last_threshold;
    bool tx_intr_enabled;
    int last_gpio_pin;
    bool last_gpio_invert;
    bool any_call_recorded;

    Rmt4ChannelMockState() FL_NO_EXCEPT : last_config(),
                                          driver_installed(false),
                                          threshold_intr_enabled(false),
                                          last_threshold(0),
                                          tx_intr_enabled(false),
                                          last_gpio_pin(-1),
                                          last_gpio_invert(false),
                                          any_call_recorded(false) {}
};

/// @brief Mock RMT4 peripheral. Abstract — use `instance()`.
class Rmt4PeripheralMock : public IRMT4Peripheral {
  public:
    //=========================================================================
    // Singleton Access
    //=========================================================================

    /// @brief Get the singleton mock peripheral instance.
    ///
    /// Mirrors the hardware constraint that there is only one RMT
    /// peripheral on real silicon. Instance is constructed on first
    /// access and persists for the program lifetime.
    static Rmt4PeripheralMock &instance() FL_NO_EXCEPT;

    //=========================================================================
    // Lifecycle
    //=========================================================================

    ~Rmt4PeripheralMock() override = default;

    //=========================================================================
    // IRMT4Peripheral interface — implementations record into the mock state.
    //=========================================================================

    bool
    configureChannel(const Rmt4ChannelConfig &cfg) FL_NO_EXCEPT override = 0;
    bool installDriver(int channel) FL_NO_EXCEPT override = 0;
    bool uninstallDriver(int channel) FL_NO_EXCEPT override = 0;
    bool setTxThresholdIntrEnable(int channel, bool enable,
                                  u16 threshold) FL_NO_EXCEPT override = 0;
    bool setTxIntrEnable(int channel, bool enable) FL_NO_EXCEPT override = 0;
    bool setGpio(int channel, int gpio_pin,
                 bool invert) FL_NO_EXCEPT override = 0;
    bool installIsr(Rmt4IsrHandler handler, void *arg,
                    void **out_handle) FL_NO_EXCEPT override = 0;
    void freeIsr(void *handle) FL_NO_EXCEPT override = 0;

    //=========================================================================
    // Mock-only Simulation API (no threads — runs on the calling thread)
    //=========================================================================

    /// @brief Invoke the stored ISR handler synchronously on the
    ///        calling thread, simulating a half-buffer threshold
    ///        interrupt for `channel`.
    ///
    /// The real engine's ISR (`ChannelEngineRMT4Impl::handleInterrupt`)
    /// reads `RMT.int_st.val` to decide which channels fired. The mock
    /// records `channel` for inspection but the handler itself is
    /// what gets called — the handler decides what to do with the bit
    /// based on the captured channel.
    ///
    /// Use `simulateThresholdInterrupt(ch)` followed by inspecting
    /// `lastSimulatedChannel()` and `lastSimulatedKind()` from the
    /// handler under test.
    virtual void simulateThresholdInterrupt(int channel) FL_NO_EXCEPT = 0;

    /// @brief Simulate a TX-done interrupt for `channel`. Same
    ///        delivery semantics as `simulateThresholdInterrupt()`.
    virtual void simulateTxDoneInterrupt(int channel) FL_NO_EXCEPT = 0;

    /// @brief Kind of the most recent simulated interrupt.
    enum class SimulatedKind { NONE, THRESHOLD, TX_DONE };

    /// @brief Return the kind of the last `simulate*Interrupt()` call.
    virtual SimulatedKind lastSimulatedKind() const FL_NO_EXCEPT = 0;

    /// @brief Return the channel number passed to the last
    ///        `simulate*Interrupt()` call. -1 if no simulation has
    ///        fired since the most recent `reset()`.
    virtual int lastSimulatedChannel() const FL_NO_EXCEPT = 0;

    /// @brief Number of times the stored ISR handler has been invoked
    ///        since the most recent `reset()`.
    virtual u32 isrInvocationCount() const FL_NO_EXCEPT = 0;

    //=========================================================================
    // Error-injection knobs
    //=========================================================================

    /// @brief Make the next `configureChannel()` call return false.
    /// @param fail true = next call fails; false = clear the flag.
    virtual void setConfigureChannelFailure(bool fail) FL_NO_EXCEPT = 0;

    /// @brief Make the next `installDriver()` call return false.
    virtual void setInstallDriverFailure(bool fail) FL_NO_EXCEPT = 0;

    /// @brief Make the next `setTxThresholdIntrEnable()` call return false.
    virtual void setTxThresholdIntrEnableFailure(bool fail) FL_NO_EXCEPT = 0;

    /// @brief Make the next `setTxIntrEnable()` call return false.
    virtual void setTxIntrEnableFailure(bool fail) FL_NO_EXCEPT = 0;

    /// @brief Make the next `setGpio()` call return false.
    virtual void setGpioFailure(bool fail) FL_NO_EXCEPT = 0;

    /// @brief Make the next `installIsr()` call return false (and
    ///        leave `*out_handle = nullptr`).
    virtual void setInstallIsrFailure(bool fail) FL_NO_EXCEPT = 0;

    //=========================================================================
    // Read-only state inspection
    //=========================================================================

    /// @brief Get the recorded state for `channel`. Channel index is
    ///        the same value the engine passed to the lifecycle calls.
    virtual const Rmt4ChannelMockState &
    getChannelState(int channel) const FL_NO_EXCEPT = 0;

    /// @brief Return true iff `installIsr()` has been called and the
    ///        cookie has not been freed.
    virtual bool isIsrRegistered() const FL_NO_EXCEPT = 0;

    /// @brief Return the handler registered via `installIsr()`. Null
    ///        if no ISR is currently registered.
    virtual Rmt4IsrHandler getRegisteredIsrHandler() const FL_NO_EXCEPT = 0;

    /// @brief Return the `arg` registered via `installIsr()`.
    virtual void *getRegisteredIsrArg() const FL_NO_EXCEPT = 0;

    //=========================================================================
    // Reset
    //=========================================================================

    /// @brief Reset mock to initial state. Clears all per-channel
    ///        state, error-injection flags, and ISR registration.
    virtual void reset() FL_NO_EXCEPT = 0;
};

} // namespace detail
} // namespace fl
