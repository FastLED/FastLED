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
#include "audio/audio_context.hpp"
#include "audio/auto_gain.hpp"
#include "audio/frequency_bin_mapper.hpp"
#include "audio/noise_floor_tracker.hpp"
#include "audio/signal_conditioner.hpp"
#include "audio/spectral_equalizer.hpp"
#include "audio/synth.hpp"
#include "audio/detectors/multiband_beat_detector.hpp"
#include "audio/detectors/musical_beat_detector.hpp"
