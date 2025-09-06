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

#include "fl/stdint.h"

#include "fl/math_macros.h" // if needed for MAX/MIN macros
#include "fl/namespace.h"
#include "fl/unique_ptr.h"
#include "fl/warn.h"
#include "fl/wave_simulation_real.h"

#include "fl/grid.h"
#include "fl/memory.h"
#include "fl/supersample.h"
#include "fl/xymap.h"
#include "fx/fx.h"
#include "fx/fx2d.h"
#include "fl/int.h"

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
    WaveSimulation1D(u32 length,
                     SuperSample factor = SuperSample::SUPER_SAMPLE_NONE,
                     float speed = 0.16f, int dampening = 6);

    void init(u32 length, SuperSample factor, float speed, int dampening);

    void setSuperSample(SuperSample factor) {
        if (u32(factor) == mMultiplier) {
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
    void setExtraFrames(u8 extra);

    // Downsampled getter for the floating point value at index x.
    // It averages over the corresponding 'multiplier'-sized block in the
    // high-res simulation.
    float getf(fl::size x) const;

    // Downsampled getter for the Q15 (fixed point) value at index x.
    // It averages the multiplier cells of Q15 values.
    i16 geti16(fl::size x) const;
    i16 geti16Previous(fl::size x) const;

    bool geti16All(fl::size x, i16 *curr, i16 *prev, i16 *diff) const;

    // Downsampled getters for the 8-bit representations.
    i8 geti8(fl::size x) const;

    u8 getu8(fl::size x) const;

    // Check if x is within the bounds of the outer (downsampled) simulation.
    bool has(fl::size x) const;

    // Upsampling setter: set the value at an outer grid cell x by replicating
    // it to the corresponding multiplier cells in the high-res simulation.
    void setf(fl::size x, float value);

    void setHalfDuplex(bool on) { mSim->setHalfDuplex(on); }

    // Advance the simulation one time step.
    void update();

    // Get the outer (downsampled) grid length.
    u32 getLength() const;

    WaveSimulation1D_Real &real() { return *mSim; }

  private:
    u32 mOuterLength; // Length of the downsampled simulation.
    u8 mExtraFrames = 0;
    u32 mMultiplier; // Supersampling multiplier (e.g., 2, 4, or 8).
    U8EasingFunction mU8Mode = WAVE_U8_MODE_LINEAR;
    // Internal high-resolution simulation.
    fl::unique_ptr<WaveSimulation1D_Real> mSim;
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
    WaveSimulation2D(u32 W, u32 H,
                     SuperSample factor = SuperSample::SUPER_SAMPLE_NONE,
                     float speed = 0.16f, float dampening = 6.0f);

    void init(u32 width, u32 height, SuperSample factor, float speed,
              int dampening);

    ~WaveSimulation2D() = default;

    // Delegated simulation methods.
    void setSpeed(float speed);

    void setExtraFrames(u8 extra);

    void setDampening(int damp);

    void setEasingMode(U8EasingFunction mode) { mU8Mode = mode; }

    int getDampenening() const;

    float getSpeed() const;

    void setSuperSample(SuperSample factor) {
        if (u32(factor) == mMultiplier) {
            return;
        }
        init(mOuterWidth, mOuterHeight, factor, mSim->getSpeed(),
             mSim->getDampenening());
    }

    void setXCylindrical(bool on) { mSim->setXCylindrical(on); }

    // Downsampled getter for the floating point value at (x,y) in the outer
    // grid. It averages over the corresponding multiplier×multiplier block in
    // the high-res simulation.
    float getf(fl::size x, fl::size y) const;

    // Downsampled getter for the Q15 (fixed point) value at (x,y).
    // It averages the multiplier×multiplier block of Q15 values.
    i16 geti16(fl::size x, fl::size y) const;
    i16 geti16Previous(fl::size x, fl::size y) const;

    bool geti16All(fl::size x, fl::size y, i16 *curr, i16 *prev,
                   i16 *diff) const;

    // Downsampled getters for the 8-bit representations.
    i8 geti8(fl::size x, fl::size y) const;

    // Special function to get the value as a u8 for drawing / gradients.
    // Ease out functions are applied to this when in half duplex mode.
    u8 getu8(fl::size x, fl::size y) const;

    // Check if (x,y) is within the bounds of the outer (downsampled) grid.
    bool has(fl::size x, fl::size y) const;

    // Upsampling setter: set the value at an outer grid cell (x,y) by
    // replicating it to all cells of the corresponding multiplier×multiplier
    // block in the high-res simulation.
    void setf(fl::size x, fl::size y, float value);

    void seti16(fl::size x, fl::size y, i16 value);

    void setHalfDuplex(bool on) { mSim->setHalfDuplex(on); }

    // Advance the simulation one time step.
    void update();

    // Get the outer grid dimensions.
    u32 getWidth() const;
    u32 getHeight() const;

    // Configure whether to use the change grid tracking optimization
    void setUseChangeGrid(bool enabled);
    bool getUseChangeGrid() const { return mUseChangeGrid; }

    WaveSimulation2D_Real &real() { return *mSim; }

  private:
    u32 mOuterWidth;  // Width of the downsampled (outer) grid.
    u32 mOuterHeight; // Height of the downsampled (outer) grid.
    u8 mExtraFrames = 0;
    u32 mMultiplier = 1; // Supersampling multiplier (e.g., 1, 2, 4, or 8).
    U8EasingFunction mU8Mode = WAVE_U8_MODE_LINEAR;
    bool mUseChangeGrid = false; // Whether to use change grid tracking (default: disabled for better visuals)
    // Internal high-resolution simulation.
    fl::unique_ptr<WaveSimulation2D_Real> mSim;
    fl::Grid<i16> mChangeGrid; // Needed for multiple updates.
};

} // namespace fl
