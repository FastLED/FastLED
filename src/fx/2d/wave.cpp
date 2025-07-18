#include "wave.h"

#include "fl/namespace.h"
#include "fl/pair.h"
#include "fl/vector.h"

namespace fl {

namespace {
struct BatchDraw {

    BatchDraw(CRGB *leds, WaveCrgbGradientMap::Gradient *grad)
        : mLeds(leds), mGradient(grad) {
        mIndices.reserve(kMaxBatchSize); // Should be a no op for FixedVector.
        mAlphas.reserve(kMaxBatchSize);
    }

    void push(fl::u32 index, uint8_t alpha) {
        if (isFull()) {
            flush();
        }
        mIndices.push_back(index);
        mAlphas.push_back(alpha);
    }

    bool isFull() { return mIndices.size() >= kMaxBatchSize; }

    void flush() {
        span<const uint8_t> alphas(mAlphas);
        CRGB rgb[kMaxBatchSize] = {};
        mGradient->fill(mAlphas, rgb);
        for (size_t i = 0; i < mIndices.size(); i++) {
            mLeds[mIndices[i]] = rgb[i];
        }
        mAlphas.clear();
        mIndices.clear();
    }

    static const size_t kMaxBatchSize = 32;
    using ArrayIndices = fl::FixedVector<fl::u32, kMaxBatchSize>;
    using ArrayAlphas = fl::FixedVector<uint8_t, kMaxBatchSize>;
    ArrayIndices mIndices;
    ArrayAlphas mAlphas;
    CRGB *mLeds = nullptr;
    WaveCrgbGradientMap::Gradient *mGradient = nullptr;
};
} // namespace

void WaveCrgbGradientMap::mapWaveToLEDs(const XYMap &xymap,
                                        WaveSimulation2D &waveSim, CRGB *leds) {
    BatchDraw batch(leds, &mGradient);
    const fl::u32 width = waveSim.getWidth();
    const fl::u32 height = waveSim.getHeight();
    for (fl::u32 y = 0; y < height; y++) {
        for (fl::u32 x = 0; x < width; x++) {
            fl::u32 idx = xymap(x, y);
            uint8_t value8 = waveSim.getu8(x, y);
            batch.push(idx, value8);
        }
    }
    batch.flush();
}

} // namespace fl
