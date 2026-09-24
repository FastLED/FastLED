// Unit tests for audio::SoundLevelMeter


#include "test.h"
#include "fl/audio/audio.h"
#include "fl/stl/vector.h"
#include "fl/math/math.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("audio::SoundLevelMeter - silence gives very negative dBFS") {
    audio::SoundLevelMeter meter;
    fl::vector<fl::i16> silence(512, 0);
    meter.processBlock(silence.data(), silence.size());
    // Silence should give -infinity or very negative dBFS
    FL_CHECK_LT(meter.getDBFS(), -60.0);
}

FL_TEST_CASE("audio::SoundLevelMeter - full scale signal near 0 dBFS") {
    audio::SoundLevelMeter meter;
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

FL_TEST_CASE("audio::SoundLevelMeter - SPL calibration") {
    audio::SoundLevelMeter meter(33.0, 0.0);
    // Process a quiet signal to establish floor
    fl::vector<fl::i16> quiet(512, 100);
    meter.processBlock(quiet.data(), quiet.size());
    double spl = meter.getSPL();
    // SPL should be a reasonable value
    FL_CHECK_GT(spl, 0.0);
}

FL_TEST_CASE("audio::SoundLevelMeter - resetFloor clears state") {
    audio::SoundLevelMeter meter;
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

FL_TEST_CASE("audio::SoundLevelMeter - setFloorSPL changes calibration") {
    audio::SoundLevelMeter meter(33.0, 0.0);
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

FL_TEST_CASE("audio::SoundLevelMeter - span overload works") {
    audio::SoundLevelMeter meter;
    fl::vector<fl::i16> signal;
    signal.reserve(512);
    for (int i = 0; i < 512; ++i) {
        signal.push_back(static_cast<fl::i16>(8000 * ((i % 2 == 0) ? 1 : -1)));
    }
    meter.processBlock(signal);
    FL_CHECK_GT(meter.getDBFS(), -20.0);
}

FL_TEST_CASE("audio::SoundLevelMeter - smoothing initializes from first block") {
    const fl::i16 sample = 1000;
    const double smoothingValues[] = {0.5, 1.0};

    for (double smoothing : smoothingValues) {
        audio::SoundLevelMeter meter(33.0, smoothing);
        meter.processBlock(&sample, 1);
        FL_CHECK_GT(meter.getDBFS(), -120.0);
        FL_CHECK_LT(meter.getDBFS(), 0.0);
        FL_CHECK_GT(meter.getSPL(), 0.0);
        FL_CHECK_LT(meter.getSPL(), 120.0);

        meter.processBlock(&sample, 1);
        FL_CHECK_GT(meter.getDBFS(), -120.0);
        FL_CHECK_LT(meter.getDBFS(), 0.0);
        FL_CHECK_GT(meter.getSPL(), 0.0);
        FL_CHECK_LT(meter.getSPL(), 120.0);
    }
}

FL_TEST_CASE("audio::SoundLevelMeter - empty blocks preserve readings") {
    audio::SoundLevelMeter meter(33.0, 0.5);
    meter.processBlock(nullptr, 0);
    FL_CHECK_EQ(meter.getDBFS(), 0.0);
    FL_CHECK_EQ(meter.getSPL(), 33.0);

    const fl::i16 sample = 1000;
    meter.processBlock(&sample, 1);
    const double dbfs = meter.getDBFS();
    const double spl = meter.getSPL();
    meter.processBlock(nullptr, 0);
    FL_CHECK_EQ(meter.getDBFS(), dbfs);
    FL_CHECK_EQ(meter.getSPL(), spl);
}

} // FL_TEST_FILE
