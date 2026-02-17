// ok standalone
/// @file fx/audio.cpp
/// @brief Unity build for fx/audio middleware tests

#include "test.h"
#include "FastLED.h"

FL_TEST_CASE("FX Audio - Test registration works") {
    FL_CHECK(true);
}

#include "fl/fx/audio/auto_gain.hpp"
#include "fl/fx/audio/frequency_bin_mapper.hpp"
#include "fl/fx/audio/noise_floor_tracker.hpp"
#include "fl/fx/audio/signal_conditioner.hpp"
#include "fl/fx/audio/spectral_equalizer.hpp"
#include "fl/fx/audio/detectors/multiband_beat_detector.hpp"
#include "fl/fx/audio/detectors/musical_beat_detector.hpp"
