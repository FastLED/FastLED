#pragma once

/// @file mic_response_data.h
/// High-resolution microphone frequency response data and utilities.
///
/// Stores 61-point (1/6-octave, 20 Hz–20 kHz) mic response curves as the
/// source of truth. At init time these are downsampled to whatever bin layout
/// the equalizer uses. Pink noise compensation is computed from bin centers
/// rather than hardcoded.

#include "fl/audio/mic_profiles.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/math.h"
#include "fastled_progmem.h"

namespace fl {

// ============================================================================
// PROGMEM float reader — portable across AVR and non-AVR platforms
// ============================================================================

/// Read a float from PROGMEM. On AVR this reads flash via pgm_read_dword;
/// on other platforms it's a plain memory read via memcpy (no aliasing issues).
inline float fl_progmem_read_float(const float* addr) {
    u32 raw = FL_PGM_READ_DWORD_ALIGNED(addr);
    float result;
    FL_BUILTIN_MEMCPY(&result, &raw, sizeof(float));
    return result;
}

// ============================================================================
// 1/6-octave frequency points: 61 points from 20 Hz to 20 kHz
// ============================================================================

static constexpr int kMicResponsePoints = 61;

// 1/6-octave spaced frequencies from 20 Hz to 20 kHz.
// Generated via: f[i] = 20 * 2^(i/6), i = 0..60
// f[0] = 20.0, f[60] = 20159.4 ≈ 20 kHz
static const float kMicResponseFreqs[kMicResponsePoints] FL_PROGMEM = {
       20.0f,    22.4f,    25.2f,    28.3f,    31.7f,    35.6f, //  0- 5
       40.0f,    44.9f,    50.4f,    56.6f,    63.5f,    71.3f, //  6-11
       80.0f,    89.8f,   100.8f,   113.1f,   127.0f,   142.5f, // 12-17
      160.0f,   179.6f,   201.6f,   226.3f,   254.0f,   285.1f, // 18-23
      320.0f,   359.1f,   403.1f,   452.5f,   508.0f,   570.2f, // 24-29
      640.0f,   718.3f,   806.3f,   905.1f,  1016.0f,  1140.4f, // 30-35
     1280.0f,  1436.5f,  1612.5f,  1810.2f,  2032.0f,  2280.8f, // 36-41
     2560.0f,  2873.1f,  3225.0f,  3620.4f,  4064.0f,  4561.6f, // 42-47
     5120.0f,  5746.1f,  6450.0f,  7240.8f,  8128.0f,  9123.2f, // 48-53
    10240.0f, 11492.2f, 12900.0f, 14481.6f, 16256.1f, 18246.5f, // 54-59
    20480.0f                                                      // 60
};

// ============================================================================
// Per-mic response curves (gain at each 1/6-octave point)
// Values > 1.0 = mic is weak → boost; < 1.0 = mic is hot → cut
// ============================================================================

// INMP441 (TDK InvenSense)
// Derived from ikostoski/esp32-i2s-slm IIR equalization filter analysis.
// Flat 100 Hz–8 kHz, bass rolloff below 100 Hz, resonance peak ~12-15 kHz.
static const float kMicResponse_INMP441[kMicResponsePoints] FL_PROGMEM = {
    // 20-36 Hz: significant bass rolloff (f[0..5])
    3.50f, 3.20f, 2.90f, 2.60f, 2.30f, 2.00f,
    // 40-71 Hz: moderate rolloff (f[6..11])
    1.74f, 1.55f, 1.40f, 1.28f, 1.18f, 1.12f,
    // 80-143 Hz: transitioning to flat (f[12..17])
    1.07f, 1.04f, 1.02f, 1.01f, 1.00f, 1.00f,
    // 160-285 Hz: essentially flat (f[18..23])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 320-570 Hz: flat (f[24..29])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 640-1140 Hz: flat (f[30..35])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 1280-2281 Hz: flat (f[36..41])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 2560-4562 Hz: beginning of rise (f[42..47])
    1.00f, 0.99f, 0.98f, 0.97f, 0.96f, 0.95f,
    // 5120-9123 Hz: resonance approach (f[48..53])
    0.93f, 0.90f, 0.87f, 0.83f, 0.80f, 0.78f,
    // 10240-18247 Hz: resonance peak region (f[54..59])
    0.76f, 0.75f, 0.76f, 0.80f, 0.88f, 0.95f,
    // 20480 Hz (f[60])
    1.00f
};

// ICS-43434 (TDK InvenSense)
// Derived from ikostoski/esp32-i2s-slm measured response.
// Flat 200 Hz–2.5 kHz, rising response above 3 kHz.
static const float kMicResponse_ICS43434[kMicResponsePoints] FL_PROGMEM = {
    // 20-36 Hz: significant bass rolloff (f[0..5])
    3.80f, 3.50f, 3.10f, 2.80f, 2.50f, 2.20f,
    // 40-71 Hz: moderate rolloff (f[6..11])
    1.90f, 1.65f, 1.45f, 1.30f, 1.20f, 1.13f,
    // 80-143 Hz: transitioning to flat (f[12..17])
    1.08f, 1.04f, 1.02f, 1.01f, 1.00f, 1.00f,
    // 160-285 Hz (f[18..23])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 320-570 Hz (f[24..29])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 640-1140 Hz (f[30..35])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 1280-2281 Hz: flat (f[36..41])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 2560-4562 Hz: beginning of HF rise (f[42..47])
    0.98f, 0.96f, 0.93f, 0.91f, 0.89f, 0.87f,
    // 5120-9123 Hz: strong HF rise (f[48..53])
    0.85f, 0.83f, 0.81f, 0.80f, 0.79f, 0.78f,
    // 10240-18247 Hz: very hot at HF (f[54..59])
    0.77f, 0.77f, 0.78f, 0.80f, 0.83f, 0.87f,
    // 20480 Hz (f[60])
    0.90f
};

// SPM1423 (Knowles)
// Estimated from SPM1423HM4H-B datasheet frequency response.
// Flat 100 Hz–10 kHz, moderate HF rise.
static const float kMicResponse_SPM1423[kMicResponsePoints] FL_PROGMEM = {
    // 20-36 Hz: bass rolloff (f[0..5])
    2.80f, 2.60f, 2.40f, 2.20f, 2.00f, 1.80f,
    // 40-71 Hz: moderate rolloff (f[6..11])
    1.60f, 1.45f, 1.32f, 1.22f, 1.15f, 1.10f,
    // 80-143 Hz: transitioning to flat (f[12..17])
    1.06f, 1.03f, 1.01f, 1.00f, 1.00f, 1.00f,
    // 160-285 Hz (f[18..23])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 320-570 Hz (f[24..29])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 640-1140 Hz (f[30..35])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 1280-2281 Hz: flat (f[36..41])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 2560-4562 Hz: slight HF rise (f[42..47])
    0.98f, 0.96f, 0.94f, 0.93f, 0.92f, 0.91f,
    // 5120-9123 Hz: moderate rise (f[48..53])
    0.90f, 0.89f, 0.88f, 0.87f, 0.86f, 0.85f,
    // 10240-18247 Hz: strong rise (f[54..59])
    0.84f, 0.84f, 0.85f, 0.87f, 0.90f, 0.93f,
    // 20480 Hz (f[60])
    0.95f
};

// Generic MEMS: average of INMP441 and ICS-43434 responses
static const float kMicResponse_GenericMEMS[kMicResponsePoints] FL_PROGMEM = {
    // 20-36 Hz (f[0..5])
    3.65f, 3.35f, 3.00f, 2.70f, 2.40f, 2.10f,
    // 40-71 Hz (f[6..11])
    1.82f, 1.60f, 1.42f, 1.29f, 1.19f, 1.12f,
    // 80-143 Hz (f[12..17])
    1.07f, 1.04f, 1.02f, 1.01f, 1.00f, 1.00f,
    // 160-285 Hz (f[18..23])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 320-570 Hz (f[24..29])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 640-1140 Hz (f[30..35])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 1280-2281 Hz (f[36..41])
    1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
    // 2560-4562 Hz (f[42..47])
    0.99f, 0.97f, 0.95f, 0.94f, 0.92f, 0.91f,
    // 5120-9123 Hz (f[48..53])
    0.89f, 0.86f, 0.84f, 0.81f, 0.79f, 0.78f,
    // 10240-18247 Hz (f[54..59])
    0.76f, 0.76f, 0.77f, 0.80f, 0.85f, 0.91f,
    // 20480 Hz (f[60])
    0.95f
};

// Line-In: flat response, no correction needed
static const float kMicResponse_LineIn[kMicResponsePoints] FL_PROGMEM = {
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f
};

// ============================================================================
// MicResponseCurve — lightweight handle to a mic's response data
// ============================================================================

struct MicResponseCurve {
    const float* freqs;  ///< Pointer to frequency array (PROGMEM)
    const float* gains;  ///< Pointer to gain array (PROGMEM)
    int count;           ///< Number of data points
};

/// Get the high-resolution response curve for a given mic profile.
/// Returns a curve with count=0 for MicProfile::None.
inline MicResponseCurve getMicResponseCurve(MicProfile profile) {
    MicResponseCurve curve = {nullptr, nullptr, 0};
    switch (profile) {
    case MicProfile::INMP441:
        curve = {kMicResponseFreqs, kMicResponse_INMP441, kMicResponsePoints};
        break;
    case MicProfile::ICS43434:
        curve = {kMicResponseFreqs, kMicResponse_ICS43434, kMicResponsePoints};
        break;
    case MicProfile::SPM1423:
        curve = {kMicResponseFreqs, kMicResponse_SPM1423, kMicResponsePoints};
        break;
    case MicProfile::GenericMEMS:
        curve = {kMicResponseFreqs, kMicResponse_GenericMEMS, kMicResponsePoints};
        break;
    case MicProfile::LineIn:
        curve = {kMicResponseFreqs, kMicResponse_LineIn, kMicResponsePoints};
        break;
    case MicProfile::None:
    default:
        break;
    }
    return curve;
}

// ============================================================================
// Interpolation and downsampling
// ============================================================================

/// Interpolate mic response at an arbitrary frequency (Hz).
/// Uses log-frequency linear interpolation with binary search.
/// Clamps to endpoint values outside the data range.
inline float interpolateMicResponse(const MicResponseCurve& curve, float freq_hz) {
    if (curve.count <= 0 || !curve.freqs || !curve.gains) return 1.0f;

    float f0 = fl_progmem_read_float(&curve.freqs[0]);
    float fN = fl_progmem_read_float(&curve.freqs[curve.count - 1]);

    // Clamp to endpoints
    if (freq_hz <= f0) return fl_progmem_read_float(&curve.gains[0]);
    if (freq_hz >= fN) return fl_progmem_read_float(&curve.gains[curve.count - 1]);

    // Binary search for bracketing interval
    int lo = 0, hi = curve.count - 1;
    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        float fMid = fl_progmem_read_float(&curve.freqs[mid]);
        if (freq_hz < fMid) {
            hi = mid;
        } else {
            lo = mid;
        }
    }

    float fLo = fl_progmem_read_float(&curve.freqs[lo]);
    float fHi = fl_progmem_read_float(&curve.freqs[hi]);
    float gLo = fl_progmem_read_float(&curve.gains[lo]);
    float gHi = fl_progmem_read_float(&curve.gains[hi]);

    // Log-frequency linear interpolation
    float logFrac = fl::logf(freq_hz / fLo) / fl::logf(fHi / fLo);
    return gLo + (gHi - gLo) * logFrac;
}

/// Downsample a high-resolution mic response curve to N output bins.
/// @param curve  The source mic response curve
/// @param binCenters  Array of bin center frequencies (Hz), length numBins
/// @param numBins  Number of output bins
/// @param out  Output array of gains, length numBins
inline void downsampleMicResponse(const MicResponseCurve& curve,
                                  const float* binCenters, int numBins,
                                  float* out) {
    for (int i = 0; i < numBins; ++i) {
        out[i] = interpolateMicResponse(curve, binCenters[i]);
    }
}

// ============================================================================
// Pink noise compensation (derived from bin geometry)
// ============================================================================

/// Compute pink noise compensation gain for a single frequency.
/// Pink noise has 1/f spectral density → equal power per octave.
/// To flatten it: multiply by sqrt(f / f_ref).
/// @param freq_hz  Bin center frequency in Hz
/// @param f_ref    Reference frequency (typically geometric mean of all bins)
/// @return  Compensation gain factor
inline float computePinkNoiseGain(float freq_hz, float f_ref) {
    if (freq_hz <= 0.0f || f_ref <= 0.0f) return 1.0f;
    return fl::sqrtf(freq_hz / f_ref);
}

/// Compute pink noise compensation gains for all bins.
/// Normalizes so the geometric mean of all gains ≈ 1.0.
/// @param binCenters  Array of bin center frequencies (Hz)
/// @param numBins     Number of bins
/// @param out         Output array of gains (length numBins)
inline void computePinkNoiseGains(const float* binCenters, int numBins, float* out) {
    if (numBins <= 0) return;

    // Compute geometric mean of bin centers for f_ref
    float logSum = 0.0f;
    int validCount = 0;
    for (int i = 0; i < numBins; ++i) {
        if (binCenters[i] > 0.0f) {
            logSum += fl::logf(binCenters[i]);
            ++validCount;
        }
    }
    float f_ref = (validCount > 0)
        ? fl::expf(logSum / static_cast<float>(validCount))
        : 1000.0f;

    // Compute raw gains
    for (int i = 0; i < numBins; ++i) {
        out[i] = computePinkNoiseGain(binCenters[i], f_ref);
    }

    // Normalize so geometric mean of output gains = 1.0
    float outLogSum = 0.0f;
    for (int i = 0; i < numBins; ++i) {
        if (out[i] > 0.0f) {
            outLogSum += fl::logf(out[i]);
        }
    }
    float geoMean = fl::expf(outLogSum / static_cast<float>(numBins));
    if (geoMean > 0.001f) {
        for (int i = 0; i < numBins; ++i) {
            out[i] /= geoMean;
        }
    }
}

} // namespace fl
