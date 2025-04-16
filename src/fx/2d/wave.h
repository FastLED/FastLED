

#include <stdint.h>

#include "fl/warn.h"

#include "fl/ptr.h"
#include "fl/wave_simulation.h"
#include "fl/xymap.h"
#include "fx/fx.h"
#include "fx/fx2d.h"

namespace fl {

FASTLED_SMART_PTR(WaveFx);

class IWaveCrgbMap {
  public:
    virtual ~IWaveCrgbMap() = default;
    virtual void mapWaveToLEDs(CRGB *leds, const WaveSimulation2D &waveSim) = 0;
};

// A great deafult for the wave rendering. It will draw black and then the
// amplitude of the wave will be more white.
class WaveCrgbMapDefault : public IWaveCrgbMap {
  public:
    void mapWaveToLEDs(CRGB *leds, const WaveSimulation2D &waveSim) override {
        for (uint16_t y = 0; y < waveSim.getHeight(); y++) {
            for (uint16_t x = 0; x < waveSim.getWidth(); x++) {
                uint16_t idx = waveSim.geti8(x, y);
                if (idx < waveSim.getWidth() * waveSim.getHeight()) {
                    leds[idx] = CRGB(waveSim.getu8(x, y), 0, 0);
                }
            }
        }
    }
};

struct WaveFxArgs {
    SuperSample factor = SuperSample::SUPER_SAMPLE_2X;
    bool half_duplex = true;
    bool auto_updates = true;
    float speed = 0.16f;
    float dampening = 6.0f;
    IWaveCrgbMap *crgbMap = nullptr;
};


// Uses bilearn filtering to double the size of the grid.
class WaveFx : public Fx2d {
  public:
    using Args = WaveFxArgs;

    WaveFx(XYMap xymap, Args args = Args())
        : Fx2d(xymap), mWaveSim(xymap.getWidth(), xymap.getHeight(), args.factor,
                                args.speed, args.dampening) {
        // Initialize the wave simulation with the given parameters.
        if (args.crgbMap == nullptr) {
            // Use the default CRGB mapping function.
            mCrgbMap.reset(new WaveCrgbMapDefault());
        } else {
            // Set a custom CRGB mapping function.
            mCrgbMap.reset(args.crgbMap);
        }

        setAutoUpdate(args.auto_updates);
    }

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

    void set(size_t x, size_t y, float value) {
        // Set the value at the given coordinates in the wave simulation.
        mWaveSim.set(x, y, value);
    }

    uint8_t getu8(size_t x, size_t y) const {
        // Get the 8-bit value at the given coordinates in the wave simulation.
        return mWaveSim.getu8(x, y);
    }

    // This will now own the crgbMap.
    void setCrgbMap(IWaveCrgbMap *crgbMap) {
        // Set a custom CRGB mapping function.
        mCrgbMap.reset(crgbMap);
    }

    void draw(DrawContext context) override {
        // Update the wave simulation.
        if (mAutoUpdates) {
            mWaveSim.update();
        }
        // Map the wave values to the LEDs.
        mCrgbMap->mapWaveToLEDs(context.leds, mWaveSim);
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

    fl::Str fxName() const override { return "WaveFx"; }

    WaveSimulation2D mWaveSim;
    scoped_ptr<IWaveCrgbMap> mCrgbMap;
    bool mAutoUpdates = true;
};

} // namespace fl
