// Tests for the bidirectional `fromFled` factories added in PR4 of #3311.
// Each factory is a one-line forwarder to the matching Fled accessor; the
// test surface here is "the spelling works and returns the same shape".

#include "fl/channels/config.h"
#include "fl/fled/fled.h"
#include "fl/fx/video.h"
#include "fl/math/screenmap.h"
#include "fl/stl/int.h"
#include "fl/stl/move.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

// Minimal valid v1 .fled (no map / video sections); used to exercise
// "factory returns the same thing as the accessor for the stub paths".
fl::vector<fl::u8> buildEmptyBundle() {
    const char env[] = "{}";
    const fl::u32 jsonLen = static_cast<fl::u32>(sizeof(env) - 1);
    fl::vector<fl::u8> out;
    out.resize(12 + jsonLen);
    out[0] = 'F'; out[1] = 'L'; out[2] = 'E'; out[3] = 'D';
    out[4] = 1;
    out[5] = 0;
    out[6] = 0; out[7] = 0;
    out[8]  = static_cast<fl::u8>(jsonLen & 0xff);
    out[9]  = static_cast<fl::u8>((jsonLen >> 8) & 0xff);
    out[10] = static_cast<fl::u8>((jsonLen >> 16) & 0xff);
    out[11] = static_cast<fl::u8>((jsonLen >> 24) & 0xff);
    for (fl::size i = 0; i < jsonLen; ++i) {
        out[12 + i] = static_cast<fl::u8>(env[i]);
    }
    return out;
}

}  // namespace

FL_TEST_CASE("fromFled - null Fled returns nullptr for all three") {
    Fled f;
    FL_CHECK(Video::fromFled(f) == nullptr);
    FL_CHECK(ScreenMap::fromFled(f) == nullptr);
    FL_CHECK(MultiChannelConfig::fromFled(f) == nullptr);
}

FL_TEST_CASE("fromFled - empty bundle returns nullptr for all three") {
    fl::vector<fl::u8> buf = buildEmptyBundle();
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    // None of the three sections exist in an empty {} envelope.
    FL_CHECK(Video::fromFled(f) == nullptr);
    FL_CHECK(ScreenMap::fromFled(f) == nullptr);
    FL_CHECK(MultiChannelConfig::fromFled(f) == nullptr);
}

FL_TEST_CASE("fromFled - returns same shape as fled.accessor() for stub paths") {
    fl::vector<fl::u8> buf = buildEmptyBundle();
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(Video::fromFled(f).get() == f.video().get());
    FL_CHECK(ScreenMap::fromFled(f).get() == f.screenMap().get());
    FL_CHECK(MultiChannelConfig::fromFled(f).get() == f.channels().get());
}

}  // FL_TEST_FILE
