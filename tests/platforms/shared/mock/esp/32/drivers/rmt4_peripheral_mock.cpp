/// @file rmt4_peripheral_mock.cpp
/// @brief Tests for `Rmt4PeripheralMock` (#3462).
///
/// Covers the mock's own contract:
/// - Per-channel lifecycle recording (configure / install / uninstall /
///   threshold-intr / tx-intr / gpio).
/// - ISR install / free cookie symmetry.
/// - Synchronous `simulate*Interrupt()` delivery on the test thread.
/// - Error-injection knobs for every fallible call site.
/// - `reset()` returns the mock to a clean state.
///
/// Engine-integration tests (`ChannelEngineRMT4Impl` + mock) are NOT
/// covered here because the engine's IRAM ISR references
/// `RMT.int_st.val` / `RMT.int_clr.val` which don't exist on the host
/// build. Once host-build RMT register stubs land that integration can
/// be added in a follow-up.

#ifdef FASTLED_STUB_IMPL // Host-only — the mock has no platform guard
                         // but production builds shouldn't pull in tests.

#include "platforms/shared/mock/esp/32/drivers/rmt4_peripheral_mock.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "test.h"

using namespace fl;
using namespace fl::detail;

namespace {

/// @brief Static counter incremented by `countingHandler` to verify the
///        mock actually invokes the registered ISR.
static u32 s_handler_calls = 0;

/// @brief Static arg pointer captured by the most recent
///        `countingHandler` call. Lets tests verify that the mock
///        passes the right `arg` through to the handler.
static void *s_handler_last_arg = nullptr;

extern "C" void countingHandler(void *arg) {
    ++s_handler_calls;
    s_handler_last_arg = arg;
}

void resetMockState() {
    Rmt4PeripheralMock::instance().reset();
    s_handler_calls = 0;
    s_handler_last_arg = nullptr;
}

} // anonymous namespace

//=============================================================================
// Channel lifecycle recording
//=============================================================================

FL_TEST_CASE("RMT4 mock - configureChannel records exact arguments") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    Rmt4ChannelConfig cfg(/*channel=*/3, /*gpio=*/18, /*clk_div=*/2,
                          /*mem_blocks=*/2, /*threshold=*/64);
    FL_CHECK(mock.configureChannel(cfg));

    const auto &state = mock.getChannelState(3);
    FL_CHECK(state.any_call_recorded);
    FL_CHECK(state.last_config.mChannel == 3);
    FL_CHECK(state.last_config.mGpioPin == 18);
    FL_CHECK(state.last_config.mClockDivider == 2u);
    FL_CHECK(state.last_config.mMemBlocks == 2u);
    FL_CHECK(state.last_config.mTxThreshold == 64u);
}

FL_TEST_CASE("RMT4 mock - install/uninstall toggles driver_installed flag") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    FL_CHECK(mock.installDriver(0));
    FL_CHECK(mock.getChannelState(0).driver_installed);

    FL_CHECK(mock.uninstallDriver(0));
    FL_CHECK_FALSE(mock.getChannelState(0).driver_installed);
}

FL_TEST_CASE(
    "RMT4 mock - setTxThresholdIntrEnable records enable + threshold") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    FL_CHECK(mock.setTxThresholdIntrEnable(2, true, 128));
    const auto &state = mock.getChannelState(2);
    FL_CHECK(state.threshold_intr_enabled);
    FL_CHECK(state.last_threshold == 128u);

    FL_CHECK(mock.setTxThresholdIntrEnable(2, false, 0));
    FL_CHECK_FALSE(mock.getChannelState(2).threshold_intr_enabled);
}

FL_TEST_CASE("RMT4 mock - setTxIntrEnable records the enable bit") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    FL_CHECK(mock.setTxIntrEnable(1, true));
    FL_CHECK(mock.getChannelState(1).tx_intr_enabled);

    FL_CHECK(mock.setTxIntrEnable(1, false));
    FL_CHECK_FALSE(mock.getChannelState(1).tx_intr_enabled);
}

FL_TEST_CASE("RMT4 mock - setGpio records pin and invert") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    FL_CHECK(mock.setGpio(/*channel=*/4, /*gpio_pin=*/19, /*invert=*/true));
    const auto &state = mock.getChannelState(4);
    FL_CHECK(state.last_gpio_pin == 19);
    FL_CHECK(state.last_gpio_invert);
}

FL_TEST_CASE("RMT4 mock - multiple channels record independently") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    mock.setGpio(0, 18, false);
    mock.setGpio(1, 19, true);
    mock.installDriver(0);

    FL_CHECK(mock.getChannelState(0).last_gpio_pin == 18);
    FL_CHECK_FALSE(mock.getChannelState(0).last_gpio_invert);
    FL_CHECK(mock.getChannelState(0).driver_installed);

    FL_CHECK(mock.getChannelState(1).last_gpio_pin == 19);
    FL_CHECK(mock.getChannelState(1).last_gpio_invert);
    FL_CHECK_FALSE(mock.getChannelState(1).driver_installed);
}

//=============================================================================
// ISR install/free cookie symmetry
//=============================================================================

FL_TEST_CASE("RMT4 mock - installIsr returns unique non-null cookie") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    int ctx = 42;
    void *cookie = nullptr;
    FL_CHECK(mock.installIsr(&countingHandler, &ctx, &cookie));
    FL_CHECK(cookie != nullptr);
    FL_CHECK(mock.isIsrRegistered());
    FL_CHECK(mock.getRegisteredIsrHandler() == &countingHandler);
    FL_CHECK(mock.getRegisteredIsrArg() == &ctx);
}

FL_TEST_CASE("RMT4 mock - freeIsr with the issued cookie clears registration") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    int ctx = 0;
    void *cookie = nullptr;
    FL_CHECK(mock.installIsr(&countingHandler, &ctx, &cookie));

    mock.freeIsr(cookie);
    FL_CHECK_FALSE(mock.isIsrRegistered());
    FL_CHECK(mock.getRegisteredIsrHandler() == nullptr);
    FL_CHECK(mock.getRegisteredIsrArg() == nullptr);
}

FL_TEST_CASE("RMT4 mock - free-then-install hands back a fresh cookie") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    int ctx = 0;
    void *cookie_a = nullptr;
    void *cookie_b = nullptr;
    FL_CHECK(mock.installIsr(&countingHandler, &ctx, &cookie_a));
    mock.freeIsr(cookie_a);
    FL_CHECK(mock.installIsr(&countingHandler, &ctx, &cookie_b));

    FL_CHECK(cookie_a != cookie_b);
    FL_CHECK(cookie_b != nullptr);
}

FL_TEST_CASE("RMT4 mock - freeIsr with nullptr or stale cookie is a no-op") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    int ctx = 0;
    void *cookie = nullptr;
    FL_CHECK(mock.installIsr(&countingHandler, &ctx, &cookie));

    // Wrong cookie value: registration must stay intact.
    void *bogus = reinterpret_cast<void *>(0xdeadbeef);
    mock.freeIsr(bogus);
    FL_CHECK(mock.isIsrRegistered());

    // Nullptr cookie: no-op.
    mock.freeIsr(nullptr);
    FL_CHECK(mock.isIsrRegistered());

    // Real cookie: clears.
    mock.freeIsr(cookie);
    FL_CHECK_FALSE(mock.isIsrRegistered());
}

FL_TEST_CASE("RMT4 mock - installIsr with null out_handle returns false") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    int ctx = 0;
    FL_CHECK_FALSE(mock.installIsr(&countingHandler, &ctx, /*out=*/nullptr));
    FL_CHECK_FALSE(mock.isIsrRegistered());
}

//=============================================================================
// Synchronous interrupt simulation (no threads)
//=============================================================================

FL_TEST_CASE("RMT4 mock - simulateThresholdInterrupt invokes the stored ISR") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    int ctx = 0;
    void *cookie = nullptr;
    FL_CHECK(mock.installIsr(&countingHandler, &ctx, &cookie));

    mock.simulateThresholdInterrupt(/*channel=*/3);

    FL_CHECK(s_handler_calls == 1u);
    FL_CHECK(s_handler_last_arg == &ctx);
    FL_CHECK(mock.isrInvocationCount() == 1u);
    FL_CHECK(mock.lastSimulatedKind() ==
             Rmt4PeripheralMock::SimulatedKind::THRESHOLD);
    FL_CHECK(mock.lastSimulatedChannel() == 3);
}

FL_TEST_CASE("RMT4 mock - simulateTxDoneInterrupt invokes the stored ISR") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    int ctx = 0;
    void *cookie = nullptr;
    FL_CHECK(mock.installIsr(&countingHandler, &ctx, &cookie));

    mock.simulateTxDoneInterrupt(/*channel=*/7);

    FL_CHECK(s_handler_calls == 1u);
    FL_CHECK(mock.lastSimulatedKind() ==
             Rmt4PeripheralMock::SimulatedKind::TX_DONE);
    FL_CHECK(mock.lastSimulatedChannel() == 7);
}

FL_TEST_CASE("RMT4 mock - simulate without an installed ISR is a no-op") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    // No installIsr() yet. simulate*Interrupt() must not crash and
    // must not bump the invocation count.
    mock.simulateThresholdInterrupt(0);
    mock.simulateTxDoneInterrupt(0);
    FL_CHECK(s_handler_calls == 0u);
    FL_CHECK(mock.isrInvocationCount() == 0u);

    // Sentinel state is still recorded though — the simulate calls
    // themselves are observable through last*() even without a handler.
    FL_CHECK(mock.lastSimulatedKind() ==
             Rmt4PeripheralMock::SimulatedKind::TX_DONE);
    FL_CHECK(mock.lastSimulatedChannel() == 0);
}

FL_TEST_CASE("RMT4 mock - multiple simulations accumulate invocation count") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    int ctx = 0;
    void *cookie = nullptr;
    mock.installIsr(&countingHandler, &ctx, &cookie);

    mock.simulateThresholdInterrupt(0);
    mock.simulateThresholdInterrupt(0);
    mock.simulateTxDoneInterrupt(0);

    FL_CHECK(s_handler_calls == 3u);
    FL_CHECK(mock.isrInvocationCount() == 3u);
}

//=============================================================================
// Error injection
//=============================================================================

FL_TEST_CASE("RMT4 mock - setConfigureChannelFailure makes configure fail") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    mock.setConfigureChannelFailure(true);
    Rmt4ChannelConfig cfg(0, 18, 2, 2, 64);
    FL_CHECK_FALSE(mock.configureChannel(cfg));
    // Nothing was recorded for the channel.
    FL_CHECK_FALSE(mock.getChannelState(0).any_call_recorded);

    mock.setConfigureChannelFailure(false);
    FL_CHECK(mock.configureChannel(cfg));
    FL_CHECK(mock.getChannelState(0).any_call_recorded);
}

FL_TEST_CASE("RMT4 mock - setInstallDriverFailure makes installDriver fail") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    mock.setInstallDriverFailure(true);
    FL_CHECK_FALSE(mock.installDriver(0));
    FL_CHECK_FALSE(mock.getChannelState(0).driver_installed);

    mock.setInstallDriverFailure(false);
    FL_CHECK(mock.installDriver(0));
    FL_CHECK(mock.getChannelState(0).driver_installed);
}

FL_TEST_CASE("RMT4 mock - setTxThresholdIntrEnableFailure makes call fail") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();
    mock.setTxThresholdIntrEnableFailure(true);
    FL_CHECK_FALSE(mock.setTxThresholdIntrEnable(0, true, 64));
    FL_CHECK_FALSE(mock.getChannelState(0).threshold_intr_enabled);
}

FL_TEST_CASE("RMT4 mock - setTxIntrEnableFailure makes call fail") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();
    mock.setTxIntrEnableFailure(true);
    FL_CHECK_FALSE(mock.setTxIntrEnable(0, true));
    FL_CHECK_FALSE(mock.getChannelState(0).tx_intr_enabled);
}

FL_TEST_CASE("RMT4 mock - setGpioFailure makes call fail") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();
    mock.setGpioFailure(true);
    FL_CHECK_FALSE(mock.setGpio(0, 18, false));
    // last_gpio_pin sentinel is still -1 (unset).
    FL_CHECK(mock.getChannelState(0).last_gpio_pin == -1);
}

FL_TEST_CASE("RMT4 mock - setInstallIsrFailure clears out_handle to nullptr") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    int ctx = 0;
    void *cookie = reinterpret_cast<void *>(0xfeedface);
    mock.setInstallIsrFailure(true);

    FL_CHECK_FALSE(mock.installIsr(&countingHandler, &ctx, &cookie));
    FL_CHECK(cookie == nullptr);
    FL_CHECK_FALSE(mock.isIsrRegistered());
}

//=============================================================================
// reset()
//=============================================================================

FL_TEST_CASE("RMT4 mock - reset clears all per-channel state and flags") {
    resetMockState();
    auto &mock = Rmt4PeripheralMock::instance();

    // Populate everything.
    Rmt4ChannelConfig cfg(0, 18, 2, 2, 64);
    mock.configureChannel(cfg);
    mock.installDriver(0);
    mock.setTxThresholdIntrEnable(0, true, 64);
    mock.setGpio(0, 18, true);

    int ctx = 0;
    void *cookie = nullptr;
    mock.installIsr(&countingHandler, &ctx, &cookie);
    mock.simulateThresholdInterrupt(0);

    mock.setConfigureChannelFailure(true);
    mock.setInstallDriverFailure(true);

    // Sanity: state is populated.
    FL_REQUIRE(mock.getChannelState(0).driver_installed);
    FL_REQUIRE(mock.isIsrRegistered());
    FL_REQUIRE(mock.isrInvocationCount() == 1u);

    // Reset.
    mock.reset();

    FL_CHECK_FALSE(mock.getChannelState(0).any_call_recorded);
    FL_CHECK_FALSE(mock.getChannelState(0).driver_installed);
    FL_CHECK_FALSE(mock.isIsrRegistered());
    FL_CHECK(mock.isrInvocationCount() == 0u);
    FL_CHECK(mock.lastSimulatedKind() ==
             Rmt4PeripheralMock::SimulatedKind::NONE);
    FL_CHECK(mock.lastSimulatedChannel() == -1);

    // Error-injection flags must have been cleared too — otherwise
    // the next test in the same process would see stale failures.
    FL_CHECK(mock.configureChannel(cfg));
    FL_CHECK(mock.installDriver(0));
}

#endif // FASTLED_STUB_IMPL
