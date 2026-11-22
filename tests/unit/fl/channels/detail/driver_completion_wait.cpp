#include "FastLED.h"
#include "fl/channels/detail/driver_completion_wait.h"
#include "fl/stl/chrono.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {
namespace driver_completion_wait_test {

struct WaitState {
    fl::MockTimeProvider time{100};
    fl::u32 managedBudget = 0;
    fl::u32 legacyBudget = 0;
    fl::u32 managedElapsed = 0;
    int managedCalls = 0;
    int legacyCalls = 0;
    bool managedResult = true;
    bool legacyResult = true;
};

WaitState &state() {
    static WaitState value;
    return value;
}

bool managedWait(fl::u32 timeout) {
    auto &value = state();
    value.managedBudget = timeout;
    ++value.managedCalls;
    value.time.advance(value.managedElapsed);
    return value.managedResult;
}

bool legacyWait(fl::u32 timeout) {
    auto &value = state();
    value.legacyBudget = timeout;
    ++value.legacyCalls;
    return value.legacyResult;
}

struct Fixture {
    fl::detail::DriverCompletionWait previousManaged;
    fl::detail::DriverCompletionWait previousLegacy;

    Fixture()
        : previousManaged(fl::detail::setManagedCompletionWait(nullptr)),
          previousLegacy(fl::detail::setLegacyCompletionWait(nullptr)) {
        state() = WaitState();
        fl::inject_time_provider([]() { return state().time.current_time(); });
    }
    ~Fixture() {
        fl::detail::setManagedCompletionWait(previousManaged);
        fl::detail::setLegacyCompletionWait(previousLegacy);
        fl::clear_time_provider();
    }
};

FL_TEST_CASE("Completion wait is a no-op without registered consumers") {
    Fixture fixture;
    FL_CHECK(FastLED.wait(1));
    FastLED.wait();
    FL_CHECK_EQ(state().managedCalls, 0);
    FL_CHECK_EQ(state().legacyCalls, 0);
}

FL_TEST_CASE("Completion wait shares one deadline between managed and legacy") {
    Fixture fixture;
    fl::detail::setManagedCompletionWait(&managedWait);
    fl::detail::setLegacyCompletionWait(&legacyWait);
    state().managedElapsed = 7;
    FL_CHECK(FastLED.wait(10));
    FL_CHECK_EQ(state().managedBudget, 10);
    FL_CHECK_EQ(state().legacyBudget, 3);
    FL_CHECK_EQ(state().managedCalls, 1);
    FL_CHECK_EQ(state().legacyCalls, 1);
}

FL_TEST_CASE("Completion wait never turns an exhausted budget into infinity") {
    Fixture fixture;
    fl::detail::setManagedCompletionWait(&managedWait);
    fl::detail::setLegacyCompletionWait(&legacyWait);
    state().managedElapsed = 10;
    FL_CHECK_FALSE(FastLED.wait(10));
    FL_CHECK_EQ(state().legacyCalls, 0);
}

FL_TEST_CASE("Completion wait preserves infinite waits and reports failures") {
    Fixture fixture;
    fl::detail::setManagedCompletionWait(&managedWait);
    fl::detail::setLegacyCompletionWait(&legacyWait);
    state().managedElapsed = 100;
    FL_CHECK(FastLED.wait(0));
    FL_CHECK_EQ(state().managedBudget, 0);
    FL_CHECK_EQ(state().legacyBudget, 0);
    state().legacyResult = false;
    FL_CHECK_FALSE(FastLED.wait(0));
    state().managedResult = false;
    state().legacyCalls = 0;
    FL_CHECK_FALSE(FastLED.wait(0));
    FL_CHECK_EQ(state().legacyCalls, 0);
}

FL_TEST_CASE("Completion wait shares its deadline across millisecond wraparound") {
    Fixture fixture;
    fl::detail::setManagedCompletionWait(&managedWait);
    fl::detail::setLegacyCompletionWait(&legacyWait);
    state().time.set_time(0xfffffffcu);
    state().managedElapsed = 7;
    FL_CHECK(FastLED.wait(10));
    FL_CHECK_EQ(state().legacyBudget, 3);
}

} // namespace driver_completion_wait_test
}
