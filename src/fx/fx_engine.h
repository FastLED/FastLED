#pragma once

#include <stdint.h>
#include <string.h>


#include "crgb.h"
#include "fl/map.h"
#include "fx/fx.h"
#include "fx/detail/fx_compositor.h"
#include "fx/detail/fx_layer.h"
#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fl/ui.h"
#include "fx/time.h"
#include "fx/video.h"
#include "fl/xymap.h"


// Forward declaration
class TimeFunction;

#ifndef FASTLED_FX_ENGINE_MAX_FX
#define FASTLED_FX_ENGINE_MAX_FX 64
#endif

namespace fl {

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
    typedef fl::FixedMap<int, FxPtr, FASTLED_FX_ENGINE_MAX_FX> IntFxMap;
    /**
     * @brief Constructs an FxEngine with the specified number of LEDs.
     * @param numLeds The number of LEDs in the strip.
     */
    FxEngine(uint16_t numLeds, bool interpolate=true);

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
     * @brief Adds a new effect to the engine. Allocate from static memory.
     *        This is not reference tracked and an object passed in must never be
     *        deleted, as the engine will use a non tracking Ptr which may outlive
     *        a call to removeFx() and the engine will thefore not know that an
     *        object has been deleted. But if it's a static object that's
     *        then the object probably wasn't going to be deleted anyway.
     */
    int addFx(Fx& effect) { return addFx(fl::Ptr<Fx>::NoTracking(effect)); }

    /**
     * @brief Requests removal of an effect from the engine, which might not happen
     *        immediately (for example the Fx needs to finish a transition).
     * @param index The index of the effect to remove.
     * @return A pointer to the removed effect, or nullptr if the index was invalid.
     */
    FxPtr removeFx(int index);

    /**
     * @brief Retrieves an effect from the engine without removing it.
     * @param index The id of the effect to retrieve.
     * @return A pointer to the effect, or nullptr if the index was invalid.
     */
    FxPtr getFx(int index);

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
     * @brief Sets the speed of the fx engine, which will impact the speed of all effects.
     * @param timeScale The new time scale value.
     */
    void setSpeed(float scale) { mTimeFunction.setScale(scale); }

  private:
    int mCounter = 0;
    TimeScale mTimeFunction;  // FxEngine controls the clock, to allow "time-bending" effects.
    IntFxMap mEffects; ///< Collection of effects
    FxCompositor mCompositor; ///< Handles effect transitions and rendering
    int mCurrId; ///< Id of the current effect
    uint16_t mDuration = 0; ///< Duration of the current transition
    bool mDurationSet = false; ///< Flag indicating if a new transition has been set
    bool mInterpolate = true;
};

}  // namespace fl
