/// @file gain.hpp
/// @brief Tests for AudioSample::applyGain() — the low-level PCM gain primitive

#pragma once

#include "fl/audio/audio.h"
#include "fl/stl/int.h"
#include "fl/stl/math.h"
#include "fl/stl/math.h"
#include "fl/stl/vector.h"
#include "test_helpers.hpp"
#include "test.h"

using namespace fl;
using namespace fl::test;

// ---------- AudioSample::applyGain tests ----------

FL_TEST_CASE("applyGain - unity gain is no-op") {
    vector<i16> data = generateSineWave(512, 440.0f, 44100.0f, 16000);
    AudioSample sample(data, 100);

    // Snapshot original PCM
    vector<i16> original(sample.pcm().begin(), sample.pcm().end());

    sample.applyGain(1.0f);

    // Every sample must be identical
    const auto& pcm = sample.pcm();
    FL_REQUIRE_EQ(pcm.size(), original.size());
    for (size i = 0; i < pcm.size(); ++i) {
        FL_CHECK_EQ(pcm[i], original[i]);
    }
}

FL_TEST_CASE("applyGain - 2x doubles samples") {
    vector<i16> data;
    data.reserve(512);
    for (size i = 0; i < 512; ++i) {
        data.push_back(static_cast<i16>((i % 2 == 0) ? 8000 : -8000));
    }
    AudioSample sample(data, 0);
    sample.applyGain(2.0f);

    const auto& pcm = sample.pcm();
    for (size i = 0; i < pcm.size(); ++i) {
        i16 expected = (i % 2 == 0) ? 16000 : -16000;
        FL_CHECK_EQ(pcm[i], expected);
    }
}

FL_TEST_CASE("applyGain - 0.5x halves samples") {
    vector<i16> data;
    data.reserve(512);
    for (size i = 0; i < 512; ++i) {
        data.push_back(static_cast<i16>((i % 2 == 0) ? 16000 : -16000));
    }
    AudioSample sample(data, 0);
    sample.applyGain(0.5f);

    const auto& pcm = sample.pcm();
    for (size i = 0; i < pcm.size(); ++i) {
        i16 expected = (i % 2 == 0) ? 8000 : -8000;
        FL_CHECK_EQ(pcm[i], expected);
    }
}

FL_TEST_CASE("applyGain - clamps at i16 max") {
    vector<i16> data(512, 20000);
    AudioSample sample(data, 0);
    sample.applyGain(2.0f);

    const auto& pcm = sample.pcm();
    for (size i = 0; i < pcm.size(); ++i) {
        FL_CHECK_EQ(pcm[i], 32767);
    }
}

FL_TEST_CASE("applyGain - clamps at i16 min") {
    vector<i16> data(512, -20000);
    AudioSample sample(data, 0);
    sample.applyGain(2.0f);

    const auto& pcm = sample.pcm();
    for (size i = 0; i < pcm.size(); ++i) {
        FL_CHECK_EQ(pcm[i], -32768);
    }
}

FL_TEST_CASE("applyGain - zero gain produces silence") {
    vector<i16> data = generateSineWave(512, 440.0f, 44100.0f, 16000);
    AudioSample sample(data, 0);
    sample.applyGain(0.0f);

    const auto& pcm = sample.pcm();
    for (size i = 0; i < pcm.size(); ++i) {
        FL_CHECK_EQ(pcm[i], 0);
    }
}

FL_TEST_CASE("applyGain - negative gain inverts") {
    vector<i16> data(512, 10000);
    AudioSample sample(data, 0);
    sample.applyGain(-1.0f);

    const auto& pcm = sample.pcm();
    for (size i = 0; i < pcm.size(); ++i) {
        FL_CHECK_EQ(pcm[i], -10000);
    }
}

FL_TEST_CASE("applyGain - very large gain clamps") {
    vector<i16> data = generateSineWave(512, 440.0f, 44100.0f, 16000);
    AudioSample sample(data, 0);
    sample.applyGain(1000.0f);

    const auto& pcm = sample.pcm();
    for (size i = 0; i < pcm.size(); ++i) {
        FL_CHECK_GE(pcm[i], -32768);
        FL_CHECK_LE(pcm[i], 32767);
    }
}

FL_TEST_CASE("applyGain - fractional gain precision") {
    vector<i16> data(512, 10000);
    AudioSample sample(data, 0);
    sample.applyGain(0.333f);

    const auto& pcm = sample.pcm();
    for (size i = 0; i < pcm.size(); ++i) {
        // 10000 * 0.333 = 3330.0, truncated to i16
        FL_CHECK_GE(pcm[i], 3329);
        FL_CHECK_LE(pcm[i], 3331);
    }
}

FL_TEST_CASE("applyGain - invalid sample is no-op") {
    AudioSample sample; // default, invalid
    FL_CHECK_FALSE(sample.isValid());
    sample.applyGain(2.0f); // must not crash
    FL_CHECK_FALSE(sample.isValid());
}

FL_TEST_CASE("applyGain - empty sample is no-op") {
    vector<i16> empty_data;
    AudioSample sample(empty_data, 0);
    FL_CHECK(sample.isValid());
    FL_CHECK_EQ(sample.size(), 0u);
    sample.applyGain(2.0f); // must not crash
    FL_CHECK(sample.isValid());
}

FL_TEST_CASE("applyGain - sequential gains multiply") {
    vector<i16> data(512, 100);
    AudioSample sample(data, 0);
    sample.applyGain(10.0f);
    sample.applyGain(10.0f);

    const auto& pcm = sample.pcm();
    for (size i = 0; i < pcm.size(); ++i) {
        FL_CHECK_EQ(pcm[i], 10000);
    }
}

FL_TEST_CASE("applyGain - preserves timestamp") {
    vector<i16> data = generateSineWave(512, 440.0f, 44100.0f, 8000);
    AudioSample sample(data, 12345);
    FL_CHECK_EQ(sample.timestamp(), 12345u);

    sample.applyGain(2.0f);
    FL_CHECK_EQ(sample.timestamp(), 12345u);
}
