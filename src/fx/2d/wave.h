

#include <stdint.h>

#include "fl/warn.h"

#include "fl/ptr.h"
#include "fl/xymap.h"
#include "fx/fx.h"
#include "fx/fx2d.h"
#include "fl/wave_simulation.h"

namespace fl {


FASTLED_SMART_PTR(WaveFx);

class IWaveCrgbMap {
  public:
    virtual ~IWaveCrgbMap() = default;
    virtual void mapWaveToLEDs(CRGB *leds, const WaveSimulation2D &waveSim) = 0;
};

// A great deafult for the wave rendering. It will draw black and then the amplitude of the
// wave will be more white.
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

// Uses bilearn filtering to double the size of the grid.
class WaveFx : public Fx2d {
  public:
    WaveFx(XYMap xymap, float courantSq = 0.16f, float dampening = 6.0f)
        : Fx2d(xymap),
          mWaveSim(xymap.getWidth(), xymap.getHeight(), courantSq, dampening) {
        // Initialize the wave simulation with the given parameters.
        mCrgbMap.reset(new WaveCrgbMapDefault());
    }

    // This will now own the crgbMap.
    void setCrgbMap(IWaveCrgbMap *crgbMap) {
        // Set a custom CRGB mapping function.
        mCrgbMap.reset(crgbMap);
    }

    void draw(DrawContext context) override {
        // Update the wave simulation.
        mWaveSim.update();
        // Map the wave values to the LEDs.
        mCrgbMap->mapWaveToLEDs(context.leds, mWaveSim);
    }

    fl::Str fxName() const override { return "WaveFx"; }

    WaveSimulation2D mWaveSim;
    scoped_ptr<IWaveCrgbMap> mCrgbMap;
};

} // namespace fl
