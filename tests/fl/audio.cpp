
/// @file audio.cpp
/// @brief Unity build for all audio tests
///
/// This file includes all audio test headers to create a single compilation unit.
/// Unity builds improve compilation speed by reducing header parsing overhead
/// and allow better optimization across test boundaries.

#include "test.h"
#include "FastLED.h"

// Test that doctest registration works
FL_TEST_CASE("Audio - Test registration works") {
    FL_CHECK(true);
}

// Include audio test files (unity build pattern)
#include "tests/fl/audio/audio_context.hpp"
#include "tests/fl/audio/auto_gain.hpp"
#include "tests/fl/audio/frequency_bin_mapper.hpp"
#include "tests/fl/audio/noise_floor_tracker.hpp"
#include "tests/fl/audio/signal_conditioner.hpp"
#include "tests/fl/audio/silence_envelope.hpp"
#include "tests/fl/audio/spectral_equalizer.hpp"
#include "tests/fl/audio/synth.hpp"
#include "tests/fl/audio/detector/equalizer.hpp"
#include "tests/fl/audio/gain.hpp"
#include "tests/fl/audio/mic_response_data.hpp"
#include "tests/fl/audio/detector/multiband_beat_detector.hpp"
#include "tests/fl/audio/detector/musical_beat_detector.hpp"

FL_TEST_FILE(FL_FILEPATH) {

} // FL_TEST_FILE
