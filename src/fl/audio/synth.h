#pragma once

/// @file synth.h
/// @brief Bandlimited audio synthesizer - waveform oscillator module
///
/// Provides a flexible anti-aliased (bandlimited) digital audio oscillator
/// that generates waveforms made of line segments. Uses BLEP (Band-Limited
/// Step) and BLAMP (Band-Limited Ramp) techniques to eliminate aliasing.
///
/// This is the foundation oscillator module for a planned full audio synthesizer.
/// Currently provides basic waveform generation. Future versions will expand to
/// include envelopes (ADSR), filters, LFOs, effects, and polyphony management.
///
/// The library supports multiple independent engine instances, allowing
/// different oscillators to use different quality settings.
///
/// Classic waveforms:
///                               peak    half    zero
///                     reflect   time   height   wait
///      Sawtooth          1       0       0       0
///      Square            1       0       1       0
///      Triangle          1       0.5     0       0
///
/// Usage:
/// @code
///   // Create an engine (shared among oscillators with same settings)
///   auto engine = ISynthEngine::create(32, 16);
///
///   // Create oscillators using the engine
///   auto osc1 = ISynthOscillator::create(engine, SynthShape::Sawtooth);
///   auto osc2 = ISynthOscillator::create(engine, SynthShape::Square);
///
///   // Generate samples
///   float buffer[256];
///   float freq = 440.0f / 44100.0f;  // 440 Hz at 44.1 kHz sample rate
///   osc1->generateSamples(buffer, 256, freq);
/// @endcode
///
/// @note This API is currently focused on the oscillator. Future versions will
/// add envelope generators, filters, and other synthesizer components. The
/// interface is expected to evolve but maintain backward compatibility where
/// practical.

#include "fl/stl/stdint.h"
#include "fl/stl/span.h"
#include "fl/stl/shared_ptr.h"

namespace fl {

// Forward declarations
class ISynthOscillator;
class ISynthEngine;
FASTLED_SHARED_PTR(ISynthOscillator);
FASTLED_SHARED_PTR(ISynthEngine);

/// Predefined waveform shapes for synth oscillator
enum class SynthShape {
    Sawtooth,        // Classic sawtooth wave (reflect=1, peak=0, half=0, wait=0)
    Square,          // Classic square wave (reflect=1, peak=0, half=1, wait=0)
    Triangle,        // Classic triangle wave (reflect=1, peak=0.5, half=0, wait=0)
    AlternatingSaw,  // Alternating sawtooth (reflect=0, peak=0, half=0, wait=0)
    Custom           // User-defined parameters
};

/// Waveform parameters for custom waveforms
struct SynthParams {
    i32 reflect = 1;      ///< Mirror second half of waveform (0 or 1)
    float peakTime = 0.0f;    ///< Position of peak in cycle [0..1]
    float halfHeight = 0.0f;  ///< Height at half-cycle point
    float zeroWait = 0.0f;    ///< Wait time at zero [0..1]

    /// Default constructor - sawtooth wave
    SynthParams() = default;

    /// Full parameter constructor
    SynthParams(i32 reflect, float peakTime, float halfHeight, float zeroWait)
        : reflect(reflect), peakTime(peakTime), halfHeight(halfHeight), zeroWait(zeroWait) {}

    /// Create parameters for a predefined shape
    static SynthParams fromShape(SynthShape shape);
};

/// Interface for synth engine that holds BLEP/BLAMP tables
///
/// ISynthEngine encapsulates the precomputed tables needed for
/// anti-aliased waveform generation. You can create multiple engines
/// with different quality settings.
class ISynthEngine {
public:
    /// Factory function to create an engine with the specified quality settings
    /// @param width BLEP width (4..64), larger = better quality, more CPU
    /// @param oversample Oversampling factor (2+), larger = less noise
    /// @return Shared pointer to the engine
    static ISynthEnginePtr create(i32 width = 32, i32 oversample = 16);

    virtual ~ISynthEngine() = default;

    /// Check if engine was initialized successfully
    virtual bool isValid() const = 0;

    /// Get the width setting
    virtual i32 getWidth() const = 0;

    /// Get the oversample setting
    virtual i32 getOversample() const = 0;
};

/// Interface class for synth oscillator
///
/// This interface provides a clean API for waveform synthesis.
/// Use the static create() factory function to obtain an instance.
///
/// @note Future versions will expand this interface with envelope generators,
/// filters, and modulation sources as the synthesizer grows.
class ISynthOscillator {
public:
    /// Factory function to create an oscillator with specified engine and parameters
    /// @param engine Shared pointer to the engine to use (keeps engine alive)
    /// @param params Waveform parameters
    /// @return Shared pointer to the oscillator
    static ISynthOscillatorPtr create(ISynthEnginePtr engine, const SynthParams& params);

    /// Factory function to create an oscillator with specified engine and shape
    /// @param engine Shared pointer to the engine to use (keeps engine alive)
    /// @param shape Predefined waveform shape
    /// @return Shared pointer to the oscillator
    static ISynthOscillatorPtr create(ISynthEnginePtr engine, SynthShape shape = SynthShape::Sawtooth);

    virtual ~ISynthOscillator() = default;

    /// Generate audio samples
    /// @param output Buffer to fill with samples
    /// @param numSamples Number of samples to generate
    /// @param freq Frequency divided by sample rate (e.g., 440/44100 for 440 Hz at 44.1 kHz)
    virtual void generateSamples(float* output, i32 numSamples, float freq) = 0;

    /// Generate audio samples (span version)
    /// @param output Span to fill with samples
    /// @param freq Frequency divided by sample rate
    virtual void generateSamples(fl::span<float> output, float freq) = 0;

    /// Change waveform shape (takes effect at next cycle boundary)
    /// @param shape Predefined waveform shape
    virtual void setShape(SynthShape shape) = 0;

    /// Change waveform parameters (takes effect at next cycle boundary)
    /// @param params Custom waveform parameters
    virtual void setParams(const SynthParams& params) = 0;

    /// Get current waveform parameters
    virtual SynthParams getParams() const = 0;

    /// Reset oscillator to beginning of cycle
    virtual void reset() = 0;

    /// Get the engine this oscillator uses
    virtual ISynthEnginePtr getEngine() const = 0;
};

} // namespace fl
