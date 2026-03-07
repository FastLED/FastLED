// synth.cpp.hpp - Implementation of bandlimited audio synthesizer oscillator
//
// This file implements the fl::ISynthOscillator interface and related APIs
// that wrap the low-level stb_hexwave library.

#include "fl/audio/synth.h"
#include "fl/stl/cstring.h"
#include "fl/stl/malloc.h"
// IWYU pragma: begin_keep
#include "third_party/stb/hexwave/stb_hexwave.h"
// IWYU pragma: end_keep

namespace fl {

// Short alias for third_party hexwave namespace to avoid verbose qualification
// without polluting fl:: namespace in unity builds.
namespace hw = third_party::hexwave;


//////////////////////////////////////////////////////////////////////////////
// SynthEngineImpl - Implementation of ISynthEngine interface
//////////////////////////////////////////////////////////////////////////////

class SynthEngineImpl : public ISynthEngine {
public:
    SynthEngineImpl(i32 width, i32 oversample);
    ~SynthEngineImpl() override;

    // Non-copyable
    SynthEngineImpl(const SynthEngineImpl&) = delete;
    SynthEngineImpl& operator=(const SynthEngineImpl&) = delete;

    // ISynthEngine interface
    bool isValid() const override;
    i32 getWidth() const override { return mWidth; }
    i32 getOversample() const override { return mOversample; }

    // Internal access for SynthOscillatorImpl (same compilation unit)
    hw::HexWaveEngine* getEngineInternal() const { return mEngine; }

private:
    hw::HexWaveEngine* mEngine = nullptr;
    i32 mWidth;
    i32 mOversample;
};

//////////////////////////////////////////////////////////////////////////////
// SynthOscillatorImpl - Implementation of ISynthOscillator interface
//////////////////////////////////////////////////////////////////////////////

class SynthOscillatorImpl : public ISynthOscillator {
public:
    SynthOscillatorImpl(fl::shared_ptr<SynthEngineImpl> engine, const SynthParams& params);
    ~SynthOscillatorImpl() override;

    // Non-copyable
    SynthOscillatorImpl(const SynthOscillatorImpl&) = delete;
    SynthOscillatorImpl& operator=(const SynthOscillatorImpl&) = delete;

    // ISynthOscillator interface
    void generateSamples(float* output, i32 numSamples, float freq) override;
    void generateSamples(fl::span<float> output, float freq) override;
    void setShape(SynthShape shape) override;
    void setParams(const SynthParams& params) override;
    SynthParams getParams() const override;
    void reset() override;
    ISynthEnginePtr getEngine() const override { return mEngine; }

private:
    fl::shared_ptr<SynthEngineImpl> mEngine;  // Shared pointer to engine (keeps it alive)
    hw::HexWave* mHexWave = nullptr;  // Typed pointer to HexWave structure
    SynthParams mCurrentParams;
};

// SynthParams implementation
SynthParams SynthParams::fromShape(SynthShape shape) {
    switch (shape) {
        case SynthShape::Sawtooth:
            return SynthParams(1, 0.0f, 0.0f, 0.0f);
        case SynthShape::Square:
            return SynthParams(1, 0.0f, 1.0f, 0.0f);
        case SynthShape::Triangle:
            return SynthParams(1, 0.5f, 0.0f, 0.0f);
        case SynthShape::AlternatingSaw:
            return SynthParams(0, 0.0f, 0.0f, 0.0f);
        case SynthShape::Custom:
        default:
            return SynthParams();  // Default to sawtooth
    }
}

//////////////////////////////////////////////////////////////////////////////
// ISynthEngine static factory method
//////////////////////////////////////////////////////////////////////////////

ISynthEnginePtr ISynthEngine::create(i32 width, i32 oversample) {
    return fl::make_shared<SynthEngineImpl>(width, oversample);
}

//////////////////////////////////////////////////////////////////////////////
// SynthEngineImpl implementation
//////////////////////////////////////////////////////////////////////////////

SynthEngineImpl::SynthEngineImpl(i32 width, i32 oversample)
    : mWidth(width), mOversample(oversample) {
    // Clamp width to valid range
    if (mWidth < 4) mWidth = 4;
    if (mWidth > FL_STB_HEXWAVE_MAX_BLEP_LENGTH) mWidth = FL_STB_HEXWAVE_MAX_BLEP_LENGTH;

    // Clamp oversample to minimum
    if (mOversample < 2) mOversample = 2;

    mEngine = hw::hexwave_engine_create(mWidth, mOversample, nullptr);
}

SynthEngineImpl::~SynthEngineImpl() {
    if (mEngine) {
        hw::hexwave_engine_destroy(mEngine);
        mEngine = nullptr;
    }
}

bool SynthEngineImpl::isValid() const {
    return mEngine != nullptr;
}

//////////////////////////////////////////////////////////////////////////////
// ISynthOscillator static factory methods
//////////////////////////////////////////////////////////////////////////////

ISynthOscillatorPtr ISynthOscillator::create(ISynthEnginePtr engine, SynthShape shape) {
    return create(engine, SynthParams::fromShape(shape));
}

ISynthOscillatorPtr ISynthOscillator::create(ISynthEnginePtr engine, const SynthParams& params) {
    if (!engine || !engine->isValid()) {
        return nullptr;
    }
    // Safe downcast - we control the factory, so we know the concrete type
    auto engineImpl = fl::static_pointer_cast<SynthEngineImpl>(engine);
    return fl::make_shared<SynthOscillatorImpl>(engineImpl, params);
}

//////////////////////////////////////////////////////////////////////////////
// SynthOscillatorImpl implementation
//////////////////////////////////////////////////////////////////////////////

SynthOscillatorImpl::SynthOscillatorImpl(fl::shared_ptr<SynthEngineImpl> engine, const SynthParams& params)
    : mEngine(engine), mCurrentParams(params) {
    // Allocate HexWave structure
    mHexWave = static_cast<hw::HexWave*>(fl::malloc(sizeof(hw::HexWave)));
    fl::memset(mHexWave, 0, sizeof(hw::HexWave));

    // Initialize the oscillator with the engine
    hw::hexwave_create(
        mHexWave,
        engine->getEngineInternal(),
        params.reflect,
        params.peakTime,
        params.halfHeight,
        params.zeroWait
    );
}

SynthOscillatorImpl::~SynthOscillatorImpl() {
    if (mHexWave) {
        fl::free(mHexWave);
        mHexWave = nullptr;
    }
}

void SynthOscillatorImpl::generateSamples(float* output, i32 numSamples, float freq) {
    if (mHexWave && output && numSamples > 0) {
        hw::hexwave_generate_samples(output, numSamples, mHexWave, freq);
    }
}

void SynthOscillatorImpl::generateSamples(fl::span<float> output, float freq) {
    generateSamples(output.data(), static_cast<i32>(output.size()), freq);
}

void SynthOscillatorImpl::setShape(SynthShape shape) {
    setParams(SynthParams::fromShape(shape));
}

void SynthOscillatorImpl::setParams(const SynthParams& params) {
    if (mHexWave) {
        mCurrentParams = params;
        hw::hexwave_change(
            mHexWave,
            params.reflect,
            params.peakTime,
            params.halfHeight,
            params.zeroWait
        );
    }
}

SynthParams SynthOscillatorImpl::getParams() const {
    return mCurrentParams;
}

void SynthOscillatorImpl::reset() {
    if (mHexWave && mEngine) {
        // Re-create the oscillator with current parameters
        hw::hexwave_create(
            mHexWave,
            mEngine->getEngineInternal(),
            mCurrentParams.reflect,
            mCurrentParams.peakTime,
            mCurrentParams.halfHeight,
            mCurrentParams.zeroWait
        );
    }
}

} // namespace fl
