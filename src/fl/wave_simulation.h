/*
This is the WaveSimulation API. These classes allow flexible super sampling
to achieve much better visual quality. They will also run the simulator
update multiple times in order to achieve consistent speed between super
sampling factors.

A super sampling value of 2x will give the best results for the CPU consumption
as most artifacts will be averaged out at this resolution.

Based on works and code by Shawn Silverman.
*/

#pragma once

#include <stdint.h>

#include "fl/math_macros.h" // if needed for MAX/MIN macros
#include "fl/namespace.h"
#include "fl/scoped_ptr.h"
#include "fl/warn.h"
#include "fl/wave_simulation_real.h"

#include "fl/grid.h"
#include "fl/ptr.h"
#include "fl/supersample.h"
#include "fl/xymap.h"
#include "fx/fx.h"
#include "fx/fx2d.h"

namespace fl {

enum U8EasingFunction { WAVE_U8_MODE_LINEAR, WAVE_U8_MODE_SQRT };

// -----------------------------------------------------------------------------
// New supersampled 1D simulation class.
//
// This class mimics the supersampling logic of WaveSimulation2D.
// The constructor accepts the desired downsampled length and a supersampling
// multiplier (via a SuperSample enum). Internally, it creates a high-resolution
// simulation of size (multiplier * length), and its accessor methods average
// over or replicate across the corresponding block of high-res cells.
class WaveSimulation1D {
  public:
    // Constructor:
    //  - length: desired downsampled grid length.
    //  - factor: supersampling multiplier (e.g., 1x, 2x, 4x, or 8x).
    //     Higher values yield better quality but are more cpu intensive.
    //  - speed and dampening are passed on to the internal simulation.
    WaveSimulation1D(uint32_t length,
                     SuperSample factor = SuperSample::SUPER_SAMPLE_NONE,
                     float speed = 0.16f, int dampening = 6);

    void init(uint32_t length, SuperSample factor, float speed, int dampening);

    void setSuperSample(SuperSample factor) {
        if (uint32_t(factor) == mMultiplier) {
            return;
        }
        init(mOuterLength, factor, mSim->getSpeed(), mSim->getDampenening());
    }

    // Only applies to getu8().
    void setEasingMode(U8EasingFunction mode) { mU8Mode = mode; }

    ~WaveSimulation1D() = default;

    // Delegate methods to the internal simulation.
    void setSpeed(float speed);
    void setDampening(int damp);
    int getDampenening() const;
    float getSpeed() const;

    // Runs the simulator faster by updating it multiple times.
    void setExtraFrames(uint8_t extra);

    // Downsampled getter for the floating point value at index x.
    // It averages over the corresponding 'multiplier'-sized block in the
    // high-res simulation.
    float getf(size_t x) const;

    // Downsampled getter for the Q15 (fixed point) value at index x.
    // It averages the multiplier cells of Q15 values.
    int16_t geti16(size_t x) const;
    int16_t geti16Previous(size_t x) const;

    bool geti16All(size_t x, int16_t *curr, int16_t *prev, int16_t *diff) const;

    // Downsampled getters for the 8-bit representations.
    int8_t geti8(size_t x) const;

    uint8_t getu8(size_t x) const;

    // Check if x is within the bounds of the outer (downsampled) simulation.
    bool has(size_t x) const;

    // Upsampling setter: set the value at an outer grid cell x by replicating
    // it to the corresponding multiplier cells in the high-res simulation.
    void setf(size_t x, float value);

    void setHalfDuplex(bool on) { mSim->setHalfDuplex(on); }

    // Advance the simulation one time step.
    void update();

    // Get the outer (downsampled) grid length.
    uint32_t getLength() const;

    WaveSimulation1D_Real &real() { return *mSim; }

  private:
    uint32_t mOuterLength; // Length of the downsampled simulation.
    uint8_t mExtraFrames = 0;
    uint32_t mMultiplier; // Supersampling multiplier (e.g., 2, 4, or 8).
    U8EasingFunction mU8Mode = WAVE_U8_MODE_LINEAR;
    // Internal high-resolution simulation.
    fl::scoped_ptr<WaveSimulation1D_Real> mSim;
};

class WaveSimulation2D {
  public:
    // Constructor:
    //   - W and H specify the desired inner grid size of the downsampled
    //   simulation.
    //   - 'factor' selects the supersampling mMultiplier (e.g., 2x, 4x, or 8x).
    //     Higher values yield better quality but are more cpu intensive.
    //   - Internally, the simulation is created with dimensions (factor*W x
    //   factor*H).
    //   - 'speed' and 'dampening' parameters are passed on to the internal
    //   simulation.
    WaveSimulation2D(uint32_t W, uint32_t H,
                     SuperSample factor = SuperSample::SUPER_SAMPLE_NONE,
                     float speed = 0.16f, float dampening = 6.0f);

    void init(uint32_t width, uint32_t height, SuperSample factor, float speed,
              int dampening);

    ~WaveSimulation2D() = default;

    // Delegated simulation methods.
    void setSpeed(float speed);

    void setExtraFrames(uint8_t extra);

    void setDampening(int damp);

    void setEasingMode(U8EasingFunction mode) { mU8Mode = mode; }

    int getDampenening() const;

    float getSpeed() const;

    void setSuperSample(SuperSample factor) {
        if (uint32_t(factor) == mMultiplier) {
            return;
        }
        init(mOuterWidth, mOuterHeight, factor, mSim->getSpeed(),
             mSim->getDampenening());
    }

    void setXCylindrical(bool on) { mSim->setXCylindrical(on); }

    // Downsampled getter for the floating point value at (x,y) in the outer
    // grid. It averages over the corresponding multiplier×multiplier block in
    // the high-res simulation.
    float getf(size_t x, size_t y) const;

    // Downsampled getter for the Q15 (fixed point) value at (x,y).
    // It averages the multiplier×multiplier block of Q15 values.
    int16_t geti16(size_t x, size_t y) const;
    int16_t geti16Previous(size_t x, size_t y) const;

    bool geti16All(size_t x, size_t y, int16_t *curr, int16_t *prev,
                   int16_t *diff) const;

    // Downsampled getters for the 8-bit representations.
    int8_t geti8(size_t x, size_t y) const;

    // Special function to get the value as a uint8_t for drawing / gradients.
    // Ease out functions are applied to this when in half duplex mode.
    uint8_t getu8(size_t x, size_t y) const;

    // Check if (x,y) is within the bounds of the outer (downsampled) grid.
    bool has(size_t x, size_t y) const;

    // Upsampling setter: set the value at an outer grid cell (x,y) by
    // replicating it to all cells of the corresponding multiplier×multiplier
    // block in the high-res simulation.
    void setf(size_t x, size_t y, float value);

    void seti16(size_t x, size_t y, int16_t value);

    void setHalfDuplex(bool on) { mSim->setHalfDuplex(on); }

    // Advance the simulation one time step.
    void update();

    // Get the outer grid dimensions.
    uint32_t getWidth() const;
    uint32_t getHeight() const;

    WaveSimulation2D_Real &real() { return *mSim; }

  private:
    uint32_t mOuterWidth;  // Width of the downsampled (outer) grid.
    uint32_t mOuterHeight; // Height of the downsampled (outer) grid.
    uint8_t mExtraFrames = 0;
    uint32_t mMultiplier = 1; // Supersampling multiplier (e.g., 1, 2, 4, or 8).
    U8EasingFunction mU8Mode = WAVE_U8_MODE_LINEAR;
    // Internal high-resolution simulation.
    fl::scoped_ptr<WaveSimulation2D_Real> mSim;
    fl::Grid<int16_t> mChangeGrid; // Needed for multiple updates.
};

} // namespace fl
