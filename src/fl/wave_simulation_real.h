/*
Wave simulation classes for 1D and 2D simulations. This is called _Real because
there is a one to one mapping between the simulation and the LED grid. For
flexible super sampling see wave_simluation.h.

Based on works and code by Shawn Silverman.
*/

#pragma once

#include <stdint.h>

#include "fl/math_macros.h" // if needed for MAX/MIN macros
#include "fl/namespace.h"
#include "fl/scoped_ptr.h"
#include "fl/vector.h"
#include "fl/warn.h"

#include "fl/ptr.h"
#include "fl/supersample.h"
#include "fl/xymap.h"
#include "fx/fx.h"
#include "fx/fx2d.h"

namespace fl {

namespace wave_detail {
int16_t float_to_fixed(float f);

// Convert fixed Q15 to float.
float fixed_to_float(int16_t f);
} // namespace wave_detail

class WaveSimulation1D_Real {
  public:
    // Constructor:
    //  - length: inner simulation grid length (excluding the 2 boundary cells).
    //  - speed: simulation speed (in float, will be stored in Q15).
    //  - dampening: exponent so that the effective damping factor is
    //  2^(dampening).
    WaveSimulation1D_Real(uint32_t length, float speed = 0.16f,
                          int dampening = 6);
    ~WaveSimulation1D_Real() = default;

    // Set simulation speed (courant parameter) using a float.
    void setSpeed(float something);

    // Set the dampening exponent (effective damping factor is 2^(dampening)).
    void setDampening(int damp);

    // Get the current dampening exponent.
    int getDampenening() const;

    // Get the simulation speed as a float.
    float getSpeed() const;

    void setHalfDuplex(bool on) { mHalfDuplex = on; }

    bool getHalfDuplex() const { return mHalfDuplex; }


    // Get the simulation value at the inner grid cell x (converted to float in
    // the range [-1.0, 1.0]).
    float getf(size_t x) const;

    int16_t geti16(size_t x) const;
    int16_t geti16Previous(size_t x) const;

    int8_t geti8(size_t x) const { return static_cast<int8_t>(geti16(x) >> 8); }

    // If mHalfDuplex is set then the the values are adjusted so that negative
    // values will instead be represented by zero.
    uint8_t getu8(size_t x) const {
        int16_t value = geti16(x);
        // Rebase the range from [-32768, 32767] to [0, 65535] then extract the
        // upper 8 bits.
        // return static_cast<uint8_t>(((static_cast<uint16_t>(value) + 32768))
        // >>
        //                            8);
        if (mHalfDuplex) {
            uint16_t v2 = static_cast<uint16_t>(value);
            v2 *= 2;
            return static_cast<uint8_t>(v2 >> 8);
        } else {
            return static_cast<uint8_t>(
                ((static_cast<uint16_t>(value) + 32768)) >> 8);
        }
    }

    // Returns whether x is within the inner grid bounds.
    bool has(size_t x) const;

    // Set the value at grid cell x (expected range: [-1.0, 1.0]); the value is
    // stored in Q15.
    void set(size_t x, float value);

    // Advance the simulation one time step.
    void update();

  private:
    uint32_t length; // Length of the inner simulation grid.
    // Two grids stored in fixed Q15 format, each with length+2 entries
    // (including boundary cells).
    fl::vector<int16_t> grid1;
    fl::vector<int16_t> grid2;
    size_t whichGrid; // Indicates the active grid (0 or 1).

    int16_t mCourantSq; // Simulation speed (courant squared) stored in Q15.
    int mDampenening; // Dampening exponent (damping factor = 2^(mDampenening)).
    bool mHalfDuplex =
        true; // Flag to restrict values to positive range during update.

};

class WaveSimulation2D_Real {
  public:
    // Constructor: Initializes the simulation with inner grid size (W x H).
    // The grid dimensions include a 1-cell border on each side.
    // Here, 'speed' is specified as a float (converted to fixed Q15)
    // and 'dampening' is given as an exponent so that the damping factor is
    // 2^dampening.
    WaveSimulation2D_Real(uint32_t W, uint32_t H, float speed = 0.16f,
                          float dampening = 6.0f);
    ~WaveSimulation2D_Real() = default;

    // Set the simulation speed (courantSq) using a float value.
    void setSpeed(float something);

    // Set the dampening factor exponent.
    // The dampening factor used is 2^(dampening).
    void setDampening(int damp);

    // Get the current dampening exponent.
    int getDampenening() const;

    // Get the simulation speed as a float (converted from fixed Q15).
    float getSpeed() const;

    // Return the value at an inner grid cell (x,y), converted to float.
    // The value returned is in the range [-1.0, 1.0].
    float getf(size_t x, size_t y) const;

    // Return the value at an inner grid cell (x,y) as a fixed Q15 integer
    // in the range [-32768, 32767].
    int16_t geti16(size_t x, size_t y) const;
    int16_t geti16Previous(size_t x, size_t y) const;

    int8_t geti8(size_t x, size_t y) const {
        return static_cast<int8_t>(geti16(x, y) >> 8);
    }

    uint8_t getu8(size_t x, size_t y) const {
        int16_t value = geti16(x, y);
        // Rebase the range from [-32768, 32767] to [0, 65535] then extract the
        // upper 8 bits.
        // return static_cast<uint8_t>(((static_cast<uint16_t>(value) + 32768))
        // >>
        //                             8);
        if (mHalfDuplex) {
            uint16_t v2 = static_cast<uint16_t>(value);
            v2 *= 2;
            return static_cast<uint8_t>(v2 >> 8);
        } else {
            return static_cast<uint8_t>(
                ((static_cast<uint16_t>(value) + 32768)) >> 8);
        }
    }

    void setXCylindrical(bool on) { mXCylindrical = on; }

    // Check if (x,y) is within the inner grid.
    bool has(size_t x, size_t y) const;

    // Set the value at an inner grid cell using a float;
    // the value is stored in fixed Q15 format.
    // value shoudl be between -1.0 and 1.0.
    void setf(size_t x, size_t y, float value);

    void seti16(size_t x, size_t y, int16_t value);

    void setHalfDuplex(bool on) { mHalfDuplex = on; }

    bool getHalfDuplex() const { return mHalfDuplex; }

    // Advance the simulation one time step using fixed-point arithmetic.
    void update();

    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }

  private:
    uint32_t width;  // Width of the inner grid.
    uint32_t height; // Height of the inner grid.
    uint32_t stride; // Row length (width + 2 for the borders).

    // Two separate grids stored in fixed Q15 format.
    fl::vector<int16_t, fl::allocator_psram<int16_t>> grid1;
    fl::vector<int16_t, fl::allocator_psram<int16_t>> grid2;

    size_t whichGrid; // Indicates the active grid (0 or 1).

    int16_t mCourantSq; // Fixed speed parameter in Q15.
    int mDampening;     // Dampening exponent; used as 2^(dampening).
    bool mHalfDuplex =
        true; // Flag to restrict values to positive range during update.
    bool mXCylindrical = false; // Default to non-cylindrical mode
};

} // namespace fl
