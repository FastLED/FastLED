#include "FastLED.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_refill_deadline.h"
#include "test.h"

using namespace fl;

namespace {

FL_TEST_CASE("Rmt4RefillDeadline excludes software preload from first IRQ deadline") {
    Rmt4RefillDeadline deadline;
    deadline.reset(120);
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFill(), 1000, false));
    deadline.recordHardwareStart(1150);
    FL_CHECK_TRUE(deadline.tryRefill(deadline.previousFill(), 1230, true));
    FL_CHECK_EQ(deadline.previousFill().timeUs, 1230u);
}

FL_TEST_CASE("Rmt4RefillDeadline preserves ISR progress during hardware start publication") {
    Rmt4RefillDeadline deadline;
    deadline.reset(120);
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFill(), 1000, false));
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFill(), 1080, true));
    deadline.recordHardwareStart(1100);
    FL_CHECK_EQ(deadline.previousFill().timeUs, 1080u);
    FL_CHECK_TRUE(deadline.tryRefill(deadline.previousFill(), 1200, true));
    FL_CHECK_FALSE(deadline.tryRefill(deadline.previousFill(), 1321, true));
    FL_CHECK_EQ(deadline.previousFill().timeUs, 1200u);
}

FL_TEST_CASE("Rmt4RefillDeadline handles timer wrap and resets per frame") {
    Rmt4RefillDeadline deadline;
    deadline.reset(120);
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFill(), 0xffffffd0u, false));
    deadline.recordHardwareStart(0xfffffff0u);
    FL_CHECK_TRUE(deadline.tryRefill(deadline.previousFill(), 64, true));
    FL_CHECK_FALSE(deadline.tryRefill(deadline.previousFill(), 185, true));
    deadline.reset(80);
    FL_CHECK_EQ(deadline.previousFill().timeUs, 0u);
    FL_CHECK_FALSE(deadline.previousFill().armed);
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFill(), 5000, false));
    deadline.recordHardwareStart(5200);
    FL_CHECK_TRUE(deadline.tryRefill(deadline.previousFill(), 5280, true));
    FL_CHECK_FALSE(deadline.tryRefill(deadline.previousFill(), 5361, true));
}

FL_TEST_CASE("Rmt4RefillDeadline ignores preload delay when ISR precedes start publication") {
    Rmt4RefillDeadline deadline;
    deadline.reset(120);
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFill(), 1000, false));
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFill(), 10000, true));
    deadline.recordHardwareStart(10020);
    FL_CHECK_EQ(deadline.previousFill().timeUs, 10000u);
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFill(), 10100, true));
    FL_CHECK_FALSE(deadline.tryRefill(deadline.previousFill(), 10221, true));
}

FL_TEST_CASE("Rmt4RefillDeadline rejects late first ISR after known hardware start") {
    Rmt4RefillDeadline deadline;
    deadline.reset(120);
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFill(), 1000, false));
    deadline.recordHardwareStart(10000);
    FL_CHECK_FALSE(deadline.tryRefill(deadline.previousFill(), 10121, true));
    FL_CHECK_EQ(deadline.previousFill().timeUs, 10000u);
}

FL_TEST_CASE("Rmt4RefillDeadline enforces deadline after hardware start at clock zero") {
    Rmt4RefillDeadline deadline;
    deadline.reset(120);
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFill(), 1000, false));
    deadline.recordHardwareStart(0);
    FL_CHECK_FALSE(deadline.tryRefill(deadline.previousFill(), 121, true));
}

FL_TEST_CASE("Rmt4RefillDeadline enforces deadline after first ISR at clock zero") {
    Rmt4RefillDeadline deadline;
    deadline.reset(120);
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFill(), 1000, false));
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFill(), 0, true));
    FL_CHECK_FALSE(deadline.tryRefill(deadline.previousFill(), 121, true));
}

FL_TEST_CASE("Rmt4RefillDeadline uses snapshot before a concurrent start publication") {
    Rmt4RefillDeadline deadline;
    deadline.reset(120);
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFill(), 1000, false));
    const auto previous = deadline.previousFill();
    const u32 now = 10000;
    deadline.recordHardwareStart(10020);
    FL_REQUIRE_TRUE(deadline.tryRefill(previous, now, true));
    FL_CHECK_EQ(deadline.previousFill().timeUs, now);
}

} // namespace
