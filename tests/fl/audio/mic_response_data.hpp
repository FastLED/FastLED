// Unit tests for mic_response_data.h — high-resolution mic curves and utilities

#include "fl/audio/mic_response_data.h"
#include "fl/stl/math.h"
#include "fl/stl/math.h"

using namespace fl;

FL_TEST_CASE("MicResponseData - interpolation at exact data points") {
    MicResponseCurve curve = getMicResponseCurve(MicProfile::LineIn);
    FL_CHECK_GT(curve.count, 0);

    // LineIn is all 1.0 — interpolation at any frequency should return 1.0
    for (int i = 0; i < curve.count; ++i) {
        float freq = fl_progmem_read_float(&curve.freqs[i]);
        float gain = interpolateMicResponse(curve, freq);
        FL_CHECK(fl::almost_equal(gain, 1.0f, 0.001f));
    }
}

FL_TEST_CASE("MicResponseData - interpolation between points is smooth") {
    MicResponseCurve curve = getMicResponseCurve(MicProfile::INMP441);
    FL_CHECK_GT(curve.count, 0);

    // Pick a frequency between two known data points (e.g., between index 16 and 17)
    float f16 = fl_progmem_read_float(&curve.freqs[16]);
    float f17 = fl_progmem_read_float(&curve.freqs[17]);
    float g16 = fl_progmem_read_float(&curve.gains[16]);
    float g17 = fl_progmem_read_float(&curve.gains[17]);
    float midFreq = fl::sqrtf(f16 * f17); // geometric mean

    float interpolated = interpolateMicResponse(curve, midFreq);

    // Interpolated value should be between the two endpoints
    float lo = fl::min(g16, g17);
    float hi = fl::max(g16, g17);
    FL_CHECK_GE(interpolated, lo - 0.01f);
    FL_CHECK_LE(interpolated, hi + 0.01f);
}

FL_TEST_CASE("MicResponseData - clamping at frequency extremes") {
    MicResponseCurve curve = getMicResponseCurve(MicProfile::INMP441);
    FL_CHECK_GT(curve.count, 0);

    float g0 = fl_progmem_read_float(&curve.gains[0]);
    float gN = fl_progmem_read_float(&curve.gains[curve.count - 1]);

    // Below 20 Hz → should clamp to first value
    float belowMin = interpolateMicResponse(curve, 5.0f);
    FL_CHECK(fl::almost_equal(belowMin, g0, 0.001f));

    // Above 20 kHz → should clamp to last value
    float aboveMax = interpolateMicResponse(curve, 30000.0f);
    FL_CHECK(fl::almost_equal(aboveMax, gN, 0.001f));
}

FL_TEST_CASE("MicResponseData - downsample to 16 bins") {
    MicResponseCurve curve = getMicResponseCurve(MicProfile::INMP441);

    // Create log-spaced bin centers matching EqualizerDetector defaults (60–5120 Hz)
    float binCenters[16];
    float fmin = 60.0f;
    float fmax = 5120.0f;
    float m = fl::logf(fmax / fmin);
    for (int i = 0; i < 16; ++i) {
        binCenters[i] = fmin * fl::expf(m * static_cast<float>(i) / 15.0f);
    }

    float gains[16];
    downsampleMicResponse(curve, binCenters, 16, gains);

    // All gains should be positive and reasonable
    for (int i = 0; i < 16; ++i) {
        FL_CHECK_GT(gains[i], 0.0f);
        FL_CHECK_LT(gains[i], 10.0f);
    }

    // Bass bin (60 Hz) should have significant boost (mic rolls off at low freq)
    FL_CHECK_GT(gains[0], 1.1f);

    // Mid-range bins (1–2 kHz) should be near 1.0 (flat region)
    FL_CHECK(fl::almost_equal(gains[8], 1.0f, 0.1f));
}

FL_TEST_CASE("MicResponseData - pink noise gains follow sqrt formula") {
    float binCenters[4] = {100.0f, 400.0f, 1600.0f, 6400.0f};
    float gains[4];
    computePinkNoiseGains(binCenters, 4, gains);

    // Gains should be monotonically increasing (higher freq = more gain)
    for (int i = 1; i < 4; ++i) {
        FL_CHECK_GT(gains[i], gains[i - 1]);
    }

    // All gains should be positive
    for (int i = 0; i < 4; ++i) {
        FL_CHECK_GT(gains[i], 0.0f);
    }
}

FL_TEST_CASE("MicResponseData - pink noise geometric mean normalization") {
    float binCenters[16];
    float fmin = 60.0f;
    float fmax = 5120.0f;
    float m = fl::logf(fmax / fmin);
    for (int i = 0; i < 16; ++i) {
        binCenters[i] = fmin * fl::expf(m * static_cast<float>(i) / 15.0f);
    }

    float gains[16];
    computePinkNoiseGains(binCenters, 16, gains);

    // Geometric mean should be approximately 1.0
    float logSum = 0.0f;
    for (int i = 0; i < 16; ++i) {
        logSum += fl::logf(gains[i]);
    }
    float geoMean = fl::expf(logSum / 16.0f);
    FL_CHECK(fl::almost_equal(geoMean, 1.0f, 0.01f));
}

FL_TEST_CASE("MicResponseData - all mic profiles produce valid gains") {
    MicProfile profiles[] = {
        MicProfile::INMP441, MicProfile::ICS43434,
        MicProfile::SPM1423, MicProfile::GenericMEMS, MicProfile::LineIn
    };

    float binCenters[16];
    float fmin = 60.0f;
    float fmax = 5120.0f;
    float m = fl::logf(fmax / fmin);
    for (int i = 0; i < 16; ++i) {
        binCenters[i] = fmin * fl::expf(m * static_cast<float>(i) / 15.0f);
    }

    for (auto profile : profiles) {
        MicResponseCurve curve = getMicResponseCurve(profile);
        FL_CHECK_GT(curve.count, 0);

        float gains[16];
        downsampleMicResponse(curve, binCenters, 16, gains);

        for (int i = 0; i < 16; ++i) {
            // No NaN
            FL_CHECK_FALSE(gains[i] != gains[i]);
            // Positive
            FL_CHECK_GT(gains[i], 0.0f);
            // Reasonable range (not absurdly large)
            FL_CHECK_LT(gains[i], 10.0f);
        }
    }
}

FL_TEST_CASE("MicResponseData - None profile returns empty curve") {
    MicResponseCurve curve = getMicResponseCurve(MicProfile::None);
    FL_CHECK_EQ(curve.count, 0);
    FL_CHECK(curve.freqs == nullptr);
    FL_CHECK(curve.gains == nullptr);
}

FL_TEST_CASE("MicResponseData - interpolation with None curve returns 1.0") {
    MicResponseCurve curve = getMicResponseCurve(MicProfile::None);
    float gain = interpolateMicResponse(curve, 1000.0f);
    FL_CHECK(fl::almost_equal(gain, 1.0f, 0.001f));
}

FL_TEST_CASE("MicResponseData - mic correction * pink noise stays reasonable") {
    // Combined mic + pink noise gains should stay within 0.1x–20x for all profiles.
    MicProfile profiles[] = {
        MicProfile::INMP441, MicProfile::ICS43434,
        MicProfile::SPM1423, MicProfile::GenericMEMS, MicProfile::LineIn
    };

    float binCenters[16];
    float fmin = 60.0f;
    float fmax = 5120.0f;
    float m = fl::logf(fmax / fmin);
    for (int i = 0; i < 16; ++i) {
        binCenters[i] = fmin * fl::expf(m * static_cast<float>(i) / 15.0f);
    }

    float pinkGains[16];
    computePinkNoiseGains(binCenters, 16, pinkGains);

    for (auto profile : profiles) {
        MicResponseCurve curve = getMicResponseCurve(profile);
        float micGains[16];
        downsampleMicResponse(curve, binCenters, 16, micGains);

        for (int i = 0; i < 16; ++i) {
            float combined = micGains[i] * pinkGains[i];
            FL_CHECK_GT(combined, 0.1f);
            FL_CHECK_LT(combined, 20.0f);
        }
    }
}

FL_TEST_CASE("MicResponseData - INMP441 key frequencies match reference") {
    MicResponseCurve curve = getMicResponseCurve(MicProfile::INMP441);

    // At 40 Hz (f[6]), ikostoski analysis: significant bass rolloff, ~1.74x
    float gain40 = interpolateMicResponse(curve, 40.0f);
    FL_CHECK(fl::almost_equal(gain40, 1.74f, 0.1f));

    // At 80 Hz (f[12]), transitioning to flat, ~1.07x
    float gain80 = interpolateMicResponse(curve, 80.0f);
    FL_CHECK(fl::almost_equal(gain80, 1.07f, 0.1f));

    // At ~1000 Hz, should be essentially flat (1.0)
    float gain1k = interpolateMicResponse(curve, 1000.0f);
    FL_CHECK(fl::almost_equal(gain1k, 1.0f, 0.05f));
}
