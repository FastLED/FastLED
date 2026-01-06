/// @file wave.cpp
/// @brief Implementation of gradient-based wave color mapping with batch optimization

#include "fl/fx/2d/wave.h"

#include "fl/stl/pair.h"
#include "fl/stl/vector.h"

namespace fl {

namespace {
/// @brief Internal helper for batched gradient color mapping
///
/// Accumulates LED indices and wave amplitudes in fixed-size batches,
/// then performs gradient lookups in bulk for better performance.
/// This reduces overhead from individual gradient->fill() calls.
struct BatchDraw {

    /// @brief Construct batch drawer
    /// @param leds LED array to write colors to
    /// @param grad Gradient to use for color mapping
    BatchDraw(CRGB *leds, WaveCrgbGradientMap::Gradient *grad)
        : mLeds(leds), mGradient(grad) {
        mIndices.reserve(kMaxBatchSize); // Should be a no op for FixedVector.
        mAlphas.reserve(kMaxBatchSize);
    }

    /// @brief Add an LED to the current batch
    /// @param index LED array index to update
    /// @param alpha Wave amplitude (0-255) for gradient lookup
    ///
    /// If the batch is full, automatically flushes before adding.
    void push(fl::u32 index, uint8_t alpha) {
        if (isFull()) {
            flush();
        }
        mIndices.push_back(index);
        mAlphas.push_back(alpha);
    }

    /// @brief Check if batch is full
    /// @return True if batch has reached maximum size
    bool isFull() { return mIndices.size() >= kMaxBatchSize; }

    /// @brief Process all accumulated entries and update LEDs
    ///
    /// Performs a single gradient fill operation for all accumulated
    /// alphas, then writes the resulting colors to their corresponding LEDs.
    /// Clears the batch after processing.
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

    static const size_t kMaxBatchSize = 32;  ///< Maximum batch size before forced flush
    using ArrayIndices = fl::FixedVector<fl::u32, kMaxBatchSize>;
    using ArrayAlphas = fl::FixedVector<uint8_t, kMaxBatchSize>;
    ArrayIndices mIndices;   ///< LED indices in current batch
    ArrayAlphas mAlphas;     ///< Wave amplitudes in current batch
    CRGB *mLeds = nullptr;   ///< Target LED array
    WaveCrgbGradientMap::Gradient *mGradient = nullptr;  ///< Gradient for color mapping
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
