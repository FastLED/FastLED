// Known production polarity defect (#5044), isolated from normal trace models.
#include "FastLED.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"
#include "test.h"
#include "generic_software_spi_fixture.h" // ok test include

FL_TEST_FILE(FL_FILEPATH) {
using namespace fl;

FL_TEST_CASE("Software SPI inverted same-port bulk must preserve logical GPIO edges (#5044)") {
    // Model6 implements real active-low hi/lo/hival/loval semantics. This
    // assertion deliberately stays RED while the production bulk path builds
    // physical clock words with unconditional OR-high / AND-low operations.
    SpiTraceFixture<6> trace;
    using Output = SpiTraceFixture<6>::Output;
    trace.reset();
    Output output;
    u8 input[] = {0x00, 0xff, 0x80, 0x01, 0xa5};
    output.writeBytes(input, 5);
    FL_REQUIRE_EQ(trace.bits().size(), 40);
    for (size_t index = 0; index < 5; ++index) {
        for (u8 bit = 0; bit < 8; ++bit) {
            FL_CHECK_EQ(trace.bits()[index * 8 + bit],
                        static_cast<u8>((input[index] >> (7 - bit)) & 1));
        }
    }
    FL_CHECK(trace.dataStable());
    FL_CHECK(trace.clockLow());
    FL_CHECK(trace.unrelatedPreserved());
}
} // FL_TEST_FILE
