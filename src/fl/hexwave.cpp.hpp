// hexwave.cpp.hpp - Implementation of high-level HexWave oscillator API
//
// This file implements the fl::IHexWaveOscillator interface and related APIs
// that wrap the low-level stb_hexwave library.

#include "fl/hexwave.h"
#include "fl/stl/cstring.h"
#include "third_party/stb/hexwave/stb_hexwave.h"

namespace fl {

// Import types from third_party namespace
using fl::third_party::hexwave::HexWave;
using fl::third_party::hexwave::HexWaveEngine;
using fl::third_party::hexwave::hexwave_engine_create;
using fl::third_party::hexwave::hexwave_engine_destroy;
using fl::third_party::hexwave::hexwave_create;
using fl::third_party::hexwave::hexwave_change;
using fl::third_party::hexwave::hexwave_generate_samples;


//////////////////////////////////////////////////////////////////////////////
// HexWaveEngineImpl - Implementation of IHexWaveEngine interface
//////////////////////////////////////////////////////////////////////////////

class HexWaveEngineImpl : public IHexWaveEngine {
public:
    HexWaveEngineImpl(int32_t width, int32_t oversample);
    ~HexWaveEngineImpl() override;

    // Non-copyable
    HexWaveEngineImpl(const HexWaveEngineImpl&) = delete;
    HexWaveEngineImpl& operator=(const HexWaveEngineImpl&) = delete;

    // IHexWaveEngine interface
    bool isValid() const override;
    int32_t getWidth() const override { return mWidth; }
    int32_t getOversample() const override { return mOversample; }

    // Internal access for HexWaveOscillatorImpl (same compilation unit)
    HexWaveEngine* getEngineInternal() const { return mEngine; }

private:
    HexWaveEngine* mEngine = nullptr;
    int32_t mWidth;
    int32_t mOversample;
};

//////////////////////////////////////////////////////////////////////////////
// HexWaveOscillatorImpl - Implementation of IHexWaveOscillator interface
//////////////////////////////////////////////////////////////////////////////

class HexWaveOscillatorImpl : public IHexWaveOscillator {
public:
    HexWaveOscillatorImpl(fl::shared_ptr<HexWaveEngineImpl> engine, const HexWaveParams& params);
    ~HexWaveOscillatorImpl() override;

    // Non-copyable
    HexWaveOscillatorImpl(const HexWaveOscillatorImpl&) = delete;
    HexWaveOscillatorImpl& operator=(const HexWaveOscillatorImpl&) = delete;

    // IHexWaveOscillator interface
    void generateSamples(float* output, int32_t numSamples, float freq) override;
    void generateSamples(fl::span<float> output, float freq) override;
    void setShape(HexWaveShape shape) override;
    void setParams(const HexWaveParams& params) override;
    HexWaveParams getParams() const override;
    void reset() override;
    IHexWaveEnginePtr getEngine() const override { return mEngine; }

private:
    fl::shared_ptr<HexWaveEngineImpl> mEngine;  // Shared pointer to engine (keeps it alive)
    HexWave* mHexWave = nullptr;                // Typed pointer to HexWave structure
    HexWaveParams mCurrentParams;
};

// HexWaveParams implementation
HexWaveParams HexWaveParams::fromShape(HexWaveShape shape) {
    switch (shape) {
        case HexWaveShape::Sawtooth:
            return HexWaveParams(1, 0.0f, 0.0f, 0.0f);
        case HexWaveShape::Square:
            return HexWaveParams(1, 0.0f, 1.0f, 0.0f);
        case HexWaveShape::Triangle:
            return HexWaveParams(1, 0.5f, 0.0f, 0.0f);
        case HexWaveShape::AlternatingSaw:
            return HexWaveParams(0, 0.0f, 0.0f, 0.0f);
        case HexWaveShape::Custom:
        default:
            return HexWaveParams();  // Default to sawtooth
    }
}

//////////////////////////////////////////////////////////////////////////////
// IHexWaveEngine static factory method
//////////////////////////////////////////////////////////////////////////////

IHexWaveEnginePtr IHexWaveEngine::create(int32_t width, int32_t oversample) {
    return fl::make_shared<HexWaveEngineImpl>(width, oversample);
}

//////////////////////////////////////////////////////////////////////////////
// HexWaveEngineImpl implementation
//////////////////////////////////////////////////////////////////////////////

HexWaveEngineImpl::HexWaveEngineImpl(int32_t width, int32_t oversample)
    : mWidth(width), mOversample(oversample) {
    // Clamp width to valid range
    if (mWidth < 4) mWidth = 4;
    if (mWidth > FL_STB_HEXWAVE_MAX_BLEP_LENGTH) mWidth = FL_STB_HEXWAVE_MAX_BLEP_LENGTH;

    // Clamp oversample to minimum
    if (mOversample < 2) mOversample = 2;

    mEngine = hexwave_engine_create(mWidth, mOversample, nullptr);
}

HexWaveEngineImpl::~HexWaveEngineImpl() {
    if (mEngine) {
        hexwave_engine_destroy(mEngine);
        mEngine = nullptr;
    }
}

bool HexWaveEngineImpl::isValid() const {
    return mEngine != nullptr;
}

//////////////////////////////////////////////////////////////////////////////
// IHexWaveOscillator static factory methods
//////////////////////////////////////////////////////////////////////////////

IHexWaveOscillatorPtr IHexWaveOscillator::create(IHexWaveEnginePtr engine, HexWaveShape shape) {
    return create(engine, HexWaveParams::fromShape(shape));
}

IHexWaveOscillatorPtr IHexWaveOscillator::create(IHexWaveEnginePtr engine, const HexWaveParams& params) {
    if (!engine || !engine->isValid()) {
        return nullptr;
    }
    // Safe downcast - we control the factory, so we know the concrete type
    auto engineImpl = fl::static_pointer_cast<HexWaveEngineImpl>(engine);
    return fl::make_shared<HexWaveOscillatorImpl>(engineImpl, params);
}

//////////////////////////////////////////////////////////////////////////////
// HexWaveOscillatorImpl implementation
//////////////////////////////////////////////////////////////////////////////

HexWaveOscillatorImpl::HexWaveOscillatorImpl(fl::shared_ptr<HexWaveEngineImpl> engine, const HexWaveParams& params)
    : mEngine(engine), mCurrentParams(params) {
    // Allocate HexWave structure
    mHexWave = static_cast<HexWave*>(fl::malloc(sizeof(HexWave)));
    fl::memset(mHexWave, 0, sizeof(HexWave));

    // Initialize the oscillator with the engine
    hexwave_create(
        mHexWave,
        engine->getEngineInternal(),
        params.reflect,
        params.peakTime,
        params.halfHeight,
        params.zeroWait
    );
}

HexWaveOscillatorImpl::~HexWaveOscillatorImpl() {
    if (mHexWave) {
        fl::free(mHexWave);
        mHexWave = nullptr;
    }
}

void HexWaveOscillatorImpl::generateSamples(float* output, int32_t numSamples, float freq) {
    if (mHexWave && output && numSamples > 0) {
        hexwave_generate_samples(output, numSamples, mHexWave, freq);
    }
}

void HexWaveOscillatorImpl::generateSamples(fl::span<float> output, float freq) {
    generateSamples(output.data(), static_cast<int32_t>(output.size()), freq);
}

void HexWaveOscillatorImpl::setShape(HexWaveShape shape) {
    setParams(HexWaveParams::fromShape(shape));
}

void HexWaveOscillatorImpl::setParams(const HexWaveParams& params) {
    if (mHexWave) {
        mCurrentParams = params;
        hexwave_change(
            mHexWave,
            params.reflect,
            params.peakTime,
            params.halfHeight,
            params.zeroWait
        );
    }
}

HexWaveParams HexWaveOscillatorImpl::getParams() const {
    return mCurrentParams;
}

void HexWaveOscillatorImpl::reset() {
    if (mHexWave && mEngine) {
        // Re-create the oscillator with current parameters
        hexwave_create(
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
