#pragma once

#include "crgb.h"
#include "fixed_vector.h"
#include "fx/fx.h"
#include "fx/detail/fx_compositor.h"
#include "fx/detail/fx_layer.h"
#include "namespace.h"
#include "ptr.h"
#include <stdint.h>
#include <string.h>

#ifndef FASTLED_FX_ENGINE_MAX_FX
#define FASTLED_FX_ENGINE_MAX_FX 64
#endif

FASTLED_NAMESPACE_BEGIN

/**
 * @class FxEngine
 * @brief Manages and renders multiple visual effects (Fx) for LED strips.
 *
 * The FxEngine class is responsible for:
 * - Storing and managing a collection of visual effects (Fx objects)
 * - Handling transitions between effects
 * - Rendering the current effect or transition to an output buffer
 */
class FxEngine {
  public:
    /**
     * @brief Constructs an FxEngine with the specified number of LEDs.
     * @param numLeds The number of LEDs in the strip.
     */
    FxEngine(uint16_t numLeds);

    /**
     * @brief Destructor for FxEngine.
     */
    ~FxEngine();

    /**
     * @brief Adds a new effect to the engine.
     * @param effect Pointer to the effect to be added.
     * @return The index of the added effect, or -1 if the effect couldn't be added.
     */
    int addFx(FxPtr effect);

    /**
     * @brief Removes an effect from the engine.
     * @param index The index of the effect to remove.
     * @return A pointer to the removed effect, or nullptr if the index was invalid.
     */
    FxPtr removeFx(int index);

    /**
     * @brief Retrieves an effect from the engine without removing it.
     * @param index The index of the effect to retrieve.
     * @return A pointer to the effect, or nullptr if the index was invalid.
     */
    FxPtr getFx(int index);

    /**
     * @brief Renders the current effect or transition to the output buffer.
     * @param now The current time in milliseconds.
     * @param outputBuffer The buffer to render the effect into.
     */
    void draw(uint32_t now, CRGB *outputBuffer);

    /**
     * @brief Transitions to the next effect in the sequence.
     * @param duration The duration of the transition in milliseconds.
     * @return True if the transition was initiated, false otherwise.
     */
    bool nextFx(uint16_t duration);

    /**
     * @brief Sets the next effect to transition to.
     * @param index The index of the effect to transition to.
     * @param duration The duration of the transition in milliseconds.
     * @return True if the transition was set, false if the index was invalid.
     */
    bool setNextFx(uint16_t index, uint16_t duration);

  private:
    FixedVector<FxPtr, FASTLED_FX_ENGINE_MAX_FX> mEffects; ///< Collection of effects
    FxCompositor mCompositor; ///< Handles effect transitions and rendering
    uint16_t mCurrentIndex; ///< Index of the current effect
    uint16_t mDuration = 0; ///< Duration of the current transition
    bool mDurationSet = false; ///< Flag indicating if a new transition has been set
};

inline FxEngine::FxEngine(uint16_t numLeds)
    : mCompositor(numLeds), mCurrentIndex(0) {
}

inline FxEngine::~FxEngine() {}

inline int FxEngine::addFx(FxPtr effect) {
    if (mEffects.size() >= FASTLED_FX_ENGINE_MAX_FX) {
        return -1;
    }
    int index = mEffects.size();
    mEffects.push_back(effect);
    if (mEffects.size() == 1) {
        mCompositor.startTransition(0, 0, mEffects[0]);
    }
    return index;
}

inline bool FxEngine::nextFx(uint16_t duration) {
    uint16_t next_index = (mCurrentIndex + 1) % mEffects.size();
    return setNextFx(next_index, duration);
}

inline bool FxEngine::setNextFx(uint16_t index, uint16_t duration) {
    if (index >= mEffects.size() || index == mCurrentIndex) {
        return false;
    }
    mCurrentIndex = index;
    mDuration = duration;
    mDurationSet = true;
    return true;
}

inline FxPtr FxEngine::removeFx(int index) {
    if (index < 0 || index >= static_cast<int>(mEffects.size())) {
        return FxPtr();
    }
    
    FxPtr removedFx = mEffects[index];
    mEffects.erase(mEffects.begin() + index);
    
    if (mCurrentIndex == index) {
        // If we're removing the current effect, switch to the next one
        mCurrentIndex = mCurrentIndex % mEffects.size();
        mDurationSet = true;
        mDuration = 0; // Instant transition
    } else if (mCurrentIndex > index) {
        // Adjust the current index if we removed an effect before it
        mCurrentIndex--;
    }
    
    return removedFx;
}

inline FxPtr FxEngine::getFx(int index) {
    if (index < 0 || index >= static_cast<int>(mEffects.size())) {
        return FxPtr();  // null.
    }
    return mEffects[index];
}

inline void FxEngine::draw(uint32_t now, CRGB *finalBuffer) {
    if (mDurationSet) {
        mCompositor.startTransition(now, mDuration, mEffects[mCurrentIndex]);
        mDurationSet = false;
    }
    if (!mEffects.empty()) {
        mCompositor.draw(now, finalBuffer);
    }
}

FASTLED_NAMESPACE_END
