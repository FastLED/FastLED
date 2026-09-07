// Decode-stage coverage for color pipeline P6 (#4040).

#include "fl/gfx/transfer.h"
#include "fl/gfx/color_profile.h"
#include "fl/stl/int.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("Decode maps both endpoints exactly for every transfer") {
    // 0 and full scale must be exact under all three, or a full-white frame
    // would not reach full drive and black would not be black.
    for (TransferFunction t : {TransferFunction::Linear, TransferFunction::Srgb,
                               TransferFunction::Bt709}) {
        FL_CHECK_EQ(decodeTransferU16(t, 0), u16(0));
        FL_CHECK_EQ(decodeTransferU16(t, 255), u16(65535));
    }
}

FL_TEST_CASE("Linear decode is the exact 8->16 bit widening") {
    // 257, not 256: it puts 255 on 65535 rather than 65280.
    for (int code = 0; code <= 255; ++code) {
        FL_CHECK_EQ(decodeTransferU16(TransferFunction::Linear, u8(code)),
                    u16(code * 257));
    }
}

FL_TEST_CASE("Decode is monotonic non-decreasing for every transfer") {
    for (TransferFunction t : {TransferFunction::Linear, TransferFunction::Srgb,
                               TransferFunction::Bt709}) {
        u16 previous = decodeTransferU16(t, 0);
        for (int code = 1; code <= 255; ++code) {
            const u16 current = decodeTransferU16(t, u8(code));
            FL_CHECK(current >= previous);
            previous = current;
        }
    }
}

FL_TEST_CASE("sRGB and BT.709 are distinct transfers, not aliases") {
    // The spec keeps them separate on purpose. They diverge most at low
    // codes, where sRGB's linear segment has a 12.92 slope and BT.709's 4.5.
    FL_CHECK_EQ(decodeTransferU16(TransferFunction::Srgb, 1), u16(20));
    FL_CHECK_EQ(decodeTransferU16(TransferFunction::Bt709, 1), u16(57));
    FL_CHECK(decodeTransferU16(TransferFunction::Bt709, 16) >
             decodeTransferU16(TransferFunction::Srgb, 16));
}

FL_TEST_CASE("Decode matches the P5 float64 reference within one u16 step") {
    // Spot values taken from the P5 golden corpus (ci/golden/
    // color-reference-v1.json), which records linear_rgb as float64 in [0,1].
    struct Case { TransferFunction transfer; u8 code; float expected; };
    const Case cases[] = {
        {TransferFunction::Bt709, 1,   0.0008714596949891067f},
        {TransferFunction::Srgb,  1,   0.0003035269835488375f},
        {TransferFunction::Srgb,  128, 0.2158605001139926f},
        {TransferFunction::Srgb,  255, 1.0f},
        {TransferFunction::Bt709, 255, 1.0f},
    };
    for (const Case& c : cases) {
        // Compare in float: `u16(want + 1)` wraps to 0 at the 65535 endpoint.
        const float got = static_cast<float>(decodeTransferU16(c.transfer, c.code));
        const float want = c.expected * 65535.0f;
        const float error = got > want ? got - want : want - got;
        FL_CHECK(error <= 1.0f);
    }
}

}  // FL_TEST_FILE
