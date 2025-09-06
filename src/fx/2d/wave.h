#pragma once

#include "fl/stdint.h"

#include "fl/warn.h"

#include "fl/colorutils.h"
#include "fl/gradient.h"
#include "fl/memory.h"
#include "fl/wave_simulation.h"
#include "fl/xymap.h"
#include "fx/fx.h"
#include "fx/fx2d.h"
#include "pixelset.h"

namespace fl {

FASTLED_SMART_PTR(WaveFx);
FASTLED_SMART_PTR(WaveCrgbMap);
FASTLED_SMART_PTR(WaveCrgbMapDefault);
FASTLED_SMART_PTR(WaveCrgbGradientMap);

class WaveCrgbMap {
  public:
    virtual ~WaveCrgbMap() = default;
    virtual void mapWaveToLEDs(const XYMap &xymap, WaveSimulation2D &waveSim,
                               CRGB *leds) = 0;
};

// A great deafult for the wave rendering. It will draw black and then the
// amplitude of the wave will be more white.
class WaveCrgbMapDefault : public WaveCrgbMap {
  public:
    void mapWaveToLEDs(const XYMap &xymap, WaveSimulation2D &waveSim,
                       CRGB *leds) override {
        const fl::u32 width = waveSim.getWidth();
        const fl::u32 height = waveSim.getHeight();
        for (fl::u32 y = 0; y < height; y++) {
            for (fl::u32 x = 0; x < width; x++) {
                fl::u32 idx = xymap(x, y);
                uint8_t value8 = waveSim.getu8(x, y);
                leds[idx] = CRGB(value8, value8, value8);
            }
        }
    }
};

class WaveCrgbGradientMap : public WaveCrgbMap {
  public:
    using Gradient = fl::GradientInlined;
    WaveCrgbGradientMap(const CRGBPalette16 &palette) : mGradient(palette) {}
    WaveCrgbGradientMap() = default;

    void mapWaveToLEDs(const XYMap &xymap, WaveSimulation2D &waveSim,
                       CRGB *leds) override;

    void setGradient(const Gradient &gradient) { mGradient = gradient; }

  private:
    Gradient mGradient;
};

struct WaveFxArgs {
    WaveFxArgs() = default;
    WaveFxArgs(SuperSample factor, bool half_duplex, bool auto_updates,
               float speed, float dampening, WaveCrgbMapPtr crgbMap)
        : factor(factor), half_duplex(half_duplex), auto_updates(auto_updates),
          speed(speed), dampening(dampening), crgbMap(crgbMap) {}
    WaveFxArgs(const WaveFxArgs &) = default;
    WaveFxArgs &operator=(const WaveFxArgs &) = default;
    SuperSample factor = SuperSample::SUPER_SAMPLE_2X;
    bool half_duplex = true;
    bool auto_updates = true;
    float speed = 0.16f;
    float dampening = 6.0f;
    bool x_cyclical = false;
    bool use_change_grid = false; // Whether to use change grid tracking (default: disabled for better visuals)
    WaveCrgbMapPtr crgbMap;
};

// Uses bilearn filtering to double the size of the grid.
class WaveFx : public Fx2d {
  public:
    using Args = WaveFxArgs;

    WaveFx(const XYMap& xymap, Args args = Args())
        : Fx2d(xymap), mWaveSim(xymap.getWidth(), xymap.getHeight(),
                                args.factor, args.speed, args.dampening) {
        // Initialize the wave simulation with the given parameters.
        if (args.crgbMap == nullptr) {
            // Use the default CRGB mapping function.
            mCrgbMap = fl::make_shared<WaveCrgbMapDefault>();
        } else {
            // Set a custom CRGB mapping function.
            mCrgbMap = args.crgbMap;
        }
        setAutoUpdate(args.auto_updates);
        setXCylindrical(args.x_cyclical);
        setUseChangeGrid(args.use_change_grid);
    }

    void setXCylindrical(bool on) { mWaveSim.setXCylindrical(on); }

    void setSpeed(float speed) {
        // Set the speed of the wave simulation.
        mWaveSim.setSpeed(speed);
    }

    void setDampening(float dampening) {
        // Set the dampening of the wave simulation.
        mWaveSim.setDampening(dampening);
    }

    void setHalfDuplex(bool on) {
        // Set whether the wave simulation is half duplex.
        mWaveSim.setHalfDuplex(on);
    }

    void setSuperSample(SuperSample factor) {
        // Set the supersampling factor of the wave simulation.
        mWaveSim.setSuperSample(factor);
    }

    void setEasingMode(U8EasingFunction mode) {
        // Set the easing mode for the 8-bit value.
        mWaveSim.setEasingMode(mode);
    }

    void setUseChangeGrid(bool enabled) {
        // Set whether to use the change grid tracking optimization.
        mWaveSim.setUseChangeGrid(enabled);
    }

    bool getUseChangeGrid() const {
        // Get the current change grid tracking setting.
        return mWaveSim.getUseChangeGrid();
    }

    void setf(size_t x, size_t y, float value) {
        // Set the value at the given coordinates in the wave simulation.
        mWaveSim.setf(x, y, value);
    }

    void addf(size_t x, size_t y, float value) {
        // Add a value at the given coordinates in the wave simulation.
        float sum = value + mWaveSim.getf(x, y);
        mWaveSim.setf(x, y, MIN(1.0f, sum));
    }

    uint8_t getu8(size_t x, size_t y) const {
        // Get the 8-bit value at the given coordinates in the wave simulation.
        return mWaveSim.getu8(x, y);
    }

    // This will now own the crgbMap.
    void setCrgbMap(WaveCrgbMapPtr crgbMap) {
        // Set a custom CRGB mapping function.
        mCrgbMap = crgbMap;
    }

    void draw(DrawContext context) override {
        // Update the wave simulation.
        if (mAutoUpdates) {
            mWaveSim.update();
        }
        // Map the wave values to the LEDs.
        mCrgbMap->mapWaveToLEDs(mXyMap, mWaveSim, context.leds);
    }

    void setAutoUpdate(bool autoUpdate) {
        // Set whether to automatically update the wave simulation.
        mAutoUpdates = autoUpdate;
    }

    void update() {
        // Called automatically in draw. Only invoke this if you want extra
        // simulation updates.
        // Update the wave simulation.
        mWaveSim.update();
    }

    fl::string fxName() const override { return "WaveFx"; }

    WaveSimulation2D mWaveSim;
    WaveCrgbMapPtr mCrgbMap;
    bool mAutoUpdates = true;
};

} // namespace fl
