// ok cpp include
#include "fl/system/instruction_counter.h"

#include "test.h"

// FastLED#5376: hosts have no instruction counter. Callers check kSupported
// before reporting, and stop() must be harmless without one.
FL_TEST_CASE("InstructionCounter - host has the null counter") {
    FL_CHECK_FALSE(fl::InstructionCounter::kSupported);
    fl::InstructionCounter counter;
    counter.start();
    FL_CHECK_EQ(counter.stop(1000u), 0u);
}
