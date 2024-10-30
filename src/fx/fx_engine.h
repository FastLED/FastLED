#pragma once

#include <stdint.h>
#include <string.h>


#include "crgb.h"
#include "fixed_map.h"
#include "fx/fx.h"
#include "fx/detail/fx_compositor.h"
#include "fx/detail/fx_layer.h"
#include "namespace.h"
#include "ref.h"
#include "ui.h"
#include "fx/detail/time_warp.h"


// Forward declaration
class TimeWarp;

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
    typedef FixedMap<int, FxRef, FASTLED_FX_ENGINE_MAX_FX> IntFxMap;
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
    int addFx(FxRef effect);

    /**
     * @brief Adds a new effect to the engine. Allocate from static memory.
     *        This is not reference tracked and an object passed in must never be
     *        deleted, as the engine will use a non tracking Ref which may outlive
     *        a call to removeFx() and the engine will thefore not know that an
     *        object has been deleted. But if it's a static object that's
     *        then the object probably wasn't going to be deleted anyway.
     */
    int addFx(Fx& effect) { return addFx(Ref<Fx>::NoTracking(effect)); }

    /**
     * @brief Requests removal of an effect from the engine, which might not happen
     *        immediately (for example the Fx needs to finish a transition).
     * @param index The index of the effect to remove.
     * @return A pointer to the removed effect, or nullptr if the index was invalid.
     */
    FxRef removeFx(int index);

    /**
     * @brief Retrieves an effect from the engine without removing it.
     * @param index The id of the effect to retrieve.
     * @return A pointer to the effect, or nullptr if the index was invalid.
     */
    FxRef getFx(int index);

    int getCurrentFxId() const { return mCurrId; }

    /**
     * @brief Renders the current effect or transition to the output buffer.
     * @param now The current time in milliseconds.
     * @param outputBuffer The buffer to render the effect into.
     */
    bool draw(uint32_t now, CRGB *outputBuffer);

    /**
     * @brief Transitions to the next effect in the sequence.
     * @param duration The duration of the transition in milliseconds.
     * @return True if the transition was initiated, false otherwise.
     */
    bool nextFx(uint16_t transition_ms = 500);

    /**
     * @brief Sets the next effect to transition to.
     * @param index The index of the effect to transition to.
     * @param duration The duration of the transition in milliseconds.
     * @return True if the transition was set, false if the index was invalid.
     */
    bool setNextFx(int index, uint16_t duration);

    
    IntFxMap& _getEffects() { return mEffects; }

    /**
     * @brief Sets the time scale for the TimeWarp object.
     * @param timeScale The new time scale value.
     */
    void setTimeScale(float timeScale) { mTimeWarp.setTimeScale(timeScale); }

  private:
    Slider mTimeBender;
    int mCounter = 0;
    TimeWarp mTimeWarp;  // FxEngine controls the clock, to allow "time-bending" effects.
    IntFxMap mEffects; ///< Collection of effects
    FxCompositor mCompositor; ///< Handles effect transitions and rendering
    int mCurrId; ///< Id of the current effect
    uint16_t mDuration = 0; ///< Duration of the current transition
    bool mDurationSet = false; ///< Flag indicating if a new transition has been set
};

inline FxEngine::FxEngine(uint16_t numLeds)
    : mTimeBender("FxEngineSpeed", 1.0f, -50.0f, 50.0f, 0.01f), 
      mTimeWarp(0), 
      mCompositor(numLeds), 
      mCurrId(0) {
}

inline FxEngine::~FxEngine() {}

inline int FxEngine::addFx(FxRef effect) {
    bool auto_set = mEffects.empty();
    bool ok = mEffects.insert(mCounter, effect);
    if (!ok) {
        return -1;
    }
    if (auto_set) {
        mCurrId = mCounter;
        mCompositor.startTransition(0, 0, effect);
    }
    return mCounter++;
}

inline bool FxEngine::nextFx(uint16_t duration) {
    bool ok = mEffects.next(mCurrId, &mCurrId, true);
    if (!ok) {
        return false;
    }
    setNextFx(mCurrId, duration);
    return true;
}

inline bool FxEngine::setNextFx(int index, uint16_t duration) {
    if (!mEffects.has(index)) {
        return false;
    }
    mCurrId = index;
    mDuration = duration;
    mDurationSet = true;
    return true;
}

inline FxRef FxEngine::removeFx(int index) {
    if (!mEffects.has(index)) {
        return FxRef();
    }
    
    FxRef removedFx;
    bool ok = mEffects.get(index, &removedFx);
    if (!ok) {
        return FxRef();
    }
    
    if (mCurrId == index) {
        // If we're removing the current effect, switch to the next one
        mEffects.next(mCurrId, &mCurrId, true);
        mDurationSet = true;
        mDuration = 0; // Instant transition
    }
    
    return removedFx;
}

inline FxRef FxEngine::getFx(int id) {
    if (mEffects.has(id)) {
        FxRef fx;
        mEffects.get(id, &fx);
        return fx;
    }
    return FxRef();
}

inline bool FxEngine::draw(uint32_t now, CRGB *finalBuffer) {
    mTimeWarp.setTimeScale(mTimeBender);
    mTimeWarp.update(now);
    uint32_t warpedTime = mTimeWarp.getTime();

    if (mEffects.empty()) {
        return false;
    }
    if (mDurationSet) {
        FxRef fx;
        bool ok = mEffects.get(mCurrId, &fx);
        if (!ok) {
            // something went wrong.
            return false;
        }
        mCompositor.startTransition(now, mDuration, fx);
        mDurationSet = false;
    }
    if (!mEffects.empty()) {
        mCompositor.draw(now, warpedTime, finalBuffer);
    }
    return true;
}

FASTLED_NAMESPACE_END
