#include "FastLED.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_refill_deadline.h"
#include "test.h"

using namespace fl;

namespace {

FL_TEST_CASE("Rmt4RefillDeadline excludes software preload from first IRQ deadline") {
    Rmt4RefillDeadline deadline;
    deadline.reset(120);
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFillUs(), 1000, false));
    deadline.recordHardwareStart(1150);
    FL_CHECK_TRUE(deadline.tryRefill(deadline.previousFillUs(), 1230, true));
    FL_CHECK_EQ(deadline.previousFillUs(), 1230u);
}

FL_TEST_CASE("Rmt4RefillDeadline preserves ISR progress during hardware start publication") {
    Rmt4RefillDeadline deadline;
    deadline.reset(120);
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFillUs(), 1000, false));
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFillUs(), 1080, true));
    deadline.recordHardwareStart(1100);
    FL_CHECK_EQ(deadline.previousFillUs(), 1080u);
    FL_CHECK_TRUE(deadline.tryRefill(deadline.previousFillUs(), 1200, true));
    FL_CHECK_FALSE(deadline.tryRefill(deadline.previousFillUs(), 1321, true));
    FL_CHECK_EQ(deadline.previousFillUs(), 1200u);
}

FL_TEST_CASE("Rmt4RefillDeadline handles timer wrap and resets per frame") {
    Rmt4RefillDeadline deadline;
    deadline.reset(120);
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFillUs(), 0xffffffd0u, false));
    deadline.recordHardwareStart(0xfffffff0u);
    FL_CHECK_TRUE(deadline.tryRefill(deadline.previousFillUs(), 64, true));
    FL_CHECK_FALSE(deadline.tryRefill(deadline.previousFillUs(), 185, true));
    deadline.reset(80);
    FL_CHECK_EQ(deadline.previousFillUs(), 0u);
    FL_REQUIRE_TRUE(deadline.tryRefill(deadline.previousFillUs(), 5000, false));
    deadline.recordHardwareStart(5200);
    FL_CHECK_TRUE(deadline.tryRefill(deadline.previousFillUs(), 5280, true));
    FL_CHECK_FALSE(deadline.tryRefill(deadline.previousFillUs(), 5361, true));
}

} // namespace
