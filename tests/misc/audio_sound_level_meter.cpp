// Unit tests for SoundLevelMeter
// ok standalone

#include "test.h"
#include "FastLED.h"
#include "fl/audio.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"

using namespace fl;

namespace { // sound_level_meter

} // anonymous namespace

FL_TEST_CASE("SoundLevelMeter - silence gives very negative dBFS") {
    SoundLevelMeter meter;
    fl::vector<fl::i16> silence(512, 0);
    meter.processBlock(silence.data(), silence.size());
    // Silence should give -infinity or very negative dBFS
    FL_CHECK_LT(meter.getDBFS(), -60.0);
}

FL_TEST_CASE("SoundLevelMeter - full scale signal near 0 dBFS") {
    SoundLevelMeter meter;
    // Full-scale square wave (alternating ±32767)
    fl::vector<fl::i16> fullScale;
    fullScale.reserve(512);
    for (int i = 0; i < 512; ++i) {
        fullScale.push_back(static_cast<fl::i16>((i % 2 == 0) ? 32767 : -32767));
    }
    meter.processBlock(fullScale.data(), fullScale.size());
    // Full scale should be near 0 dBFS
    FL_CHECK_GT(meter.getDBFS(), -3.0);
}

FL_TEST_CASE("SoundLevelMeter - SPL calibration") {
    SoundLevelMeter meter(33.0, 0.0);
    // Process a quiet signal to establish floor
    fl::vector<fl::i16> quiet(512, 100);
    meter.processBlock(quiet.data(), quiet.size());
    double spl = meter.getSPL();
    // SPL should be a reasonable value
    FL_CHECK_GT(spl, 0.0);
}

FL_TEST_CASE("SoundLevelMeter - resetFloor clears state") {
    SoundLevelMeter meter;
    fl::vector<fl::i16> signal;
    signal.reserve(512);
    for (int i = 0; i < 512; ++i) {
        signal.push_back(static_cast<fl::i16>(5000 * ((i % 2 == 0) ? 1 : -1)));
    }
    meter.processBlock(signal.data(), signal.size());
    double spl1 = meter.getSPL();
    meter.resetFloor();
    meter.processBlock(signal.data(), signal.size());
    double spl2 = meter.getSPL();
    // Both should be valid numbers
    FL_CHECK_GT(spl1, 0.0);
    FL_CHECK_GT(spl2, 0.0);
}

FL_TEST_CASE("SoundLevelMeter - setFloorSPL changes calibration") {
    SoundLevelMeter meter(33.0, 0.0);
    fl::vector<fl::i16> signal;
    signal.reserve(512);
    for (int i = 0; i < 512; ++i) {
        signal.push_back(static_cast<fl::i16>(3000 * ((i % 2 == 0) ? 1 : -1)));
    }
    meter.processBlock(signal.data(), signal.size());
    double spl1 = meter.getSPL();
    meter.setFloorSPL(50.0);
    meter.processBlock(signal.data(), signal.size());
    double spl2 = meter.getSPL();
    // Different floor SPL should produce different SPL values
    FL_CHECK_NE(spl1, spl2);
}

FL_TEST_CASE("SoundLevelMeter - span overload works") {
    SoundLevelMeter meter;
    fl::vector<fl::i16> signal;
    signal.reserve(512);
    for (int i = 0; i < 512; ++i) {
        signal.push_back(static_cast<fl::i16>(8000 * ((i % 2 == 0) ? 1 : -1)));
    }
    meter.processBlock(fl::span<const fl::i16>(signal.data(), signal.size()));
    FL_CHECK_GT(meter.getDBFS(), -20.0);
}
