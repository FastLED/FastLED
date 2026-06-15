/*
Wave simulation classes for 1D and 2D simulations. This is called _Real because
there is a one to one mapping between the simulation and the LED grid. For
flexible super sampling see wave_simluation.h.

Based on works and code by Shawn Silverman.
*/

#pragma once

#include "fl/stl/stdint.h"

#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {

namespace wave_detail {
i16 float_to_fixed(float f);

// Convert fixed Q15 to float.
float fixed_to_float(i16 f);

// Compute the Q15 damping decay factor for a power-of-two damping
// exponent. Equivalent (modulo 1-LSB rounding) to the arithmetic-shift
// form `f -= f >> damp` used by the kernel before #3099/§6.
i16 compute_damp_decay_q15(int damp) FL_NOEXCEPT;
} // namespace wave_detail

class WaveSimulation1D_Real {
  public:
    // Constructor:
    //  - length: inner simulation grid length (excluding the 2 boundary cells).
    //  - speed: simulation speed (Courant squared, C^2) stored in Q15.
    //    Clamped to [0.0, 1.0] (CFL stability bound for the 1D 5-point
    //    stencil). Values outside that range produce visual instability
    //    and are silently clamped.
    //  - dampening: exponent so that the effective damping factor is
    //  2^(dampening).
    WaveSimulation1D_Real(u32 length, float speed = 0.16f,
                          int dampening = 6) FL_NOEXCEPT;
    ~WaveSimulation1D_Real() FL_NOEXCEPT = default;

    // Set simulation speed (Courant squared). Clamped to [0.0, 1.0] — see
    // constructor for rationale.
    void setSpeed(float something);

    // Set the dampening exponent (effective damping factor is 2^(dampening)).
    void setDampening(int damp) FL_NOEXCEPT;

    // Get the current dampening exponent.
    int getDampenening() const;

    // Get the simulation speed as a float.
    float getSpeed() const;

    void setHalfDuplex(bool on) { mHalfDuplex = on; }

    bool getHalfDuplex() const { return mHalfDuplex; }

    // Get the simulation value at the inner grid cell x (converted to float in
    // the range [-1.0, 1.0]).
    float getf(fl::size x) const;

    i16 geti16(fl::size x) const;
    i16 geti16Previous(fl::size x) const;

    i8 geti8(fl::size x) const { return static_cast<i8>(geti16(x) >> 8); }

    // If mHalfDuplex is set then the the values are adjusted so that negative
    // values will instead be represented by zero.
    u8 getu8(fl::size x) const {
        i16 value = geti16(x);
        // Rebase the range from [-32768, 32767] to [0, 65535] then extract the
        // upper 8 bits.
        // return static_cast<u8>(((static_cast<u16>(value) + 32768))
        // >>
        //                            8);
        if (mHalfDuplex) {
            u16 v2 = static_cast<u16>(value);
            v2 *= 2;
            return static_cast<u8>(v2 >> 8);
        } else {
            return static_cast<u8>(
                ((static_cast<u16>(value) + 32768)) >> 8);
        }
    }

    // Returns whether x is within the inner grid bounds.
    bool has(fl::size x) const;

    // Set the value at grid cell x (expected range: [-1.0, 1.0]); the value is
    // stored in Q15.
    void set(fl::size x, float value);

    // Advance the simulation one time step.
    void update();

  private:
    u32 length; // Length of the inner simulation grid.
    // Two grids stored in fixed Q15 format, each with length+2 entries
    // (including boundary cells).
    fl::vector<i16> grid1;
    fl::vector<i16> grid2;
    fl::size whichGrid; // Indicates the active grid (0 or 1).

    i16 mCourantSq; // Simulation speed (courant squared) stored in Q15.
    int mDampenening; // Dampening exponent (damping factor = 2^(mDampenening)).
    // Precomputed Q15 decay multiplier (1 - 1/2^mDampenening). The inner
    // loop uses `f = (i64(f) * mDampDecayQ15) >> 15` instead of the shift,
    // which generalizes cleanly to non-power-of-two damping if the public
    // API ever needs it.
    i16 mDampDecayQ15;
    bool mHalfDuplex =
        true; // Flag to restrict values to positive range during update.

};

// Discrete Laplacian stencil for the 2D wave PDE.
//   FivePoint           — standard 5-point: N+S+E+W - 4*C, O(h^2) accurate
//                         but anisotropic (waves visibly prefer cardinal
//                         directions, producing square-ish ripples at high
//                         super-sample factors).
//   NinePointIsotropic  — 1/6 * sum(diagonals) + 2/3 * sum(neighbors)
//                         - 10/3 * C, also O(h^2) but with isotropic
//                         leading error. ~2x reads + ALU per cell; gives
//                         visibly rounder ripples at SUPER_SAMPLE_2X and
//                         above. The wave_simulation.h wrapper auto-
//                         selects this when SuperSample >= 2X.
enum class LaplacianStencil : u8 {
    FivePoint = 0,
    NinePointIsotropic = 1,
};

class WaveSimulation2D_Real {
  public:
    // Constructor: Initializes the simulation with inner grid size (W x H).
    // The grid dimensions include a 1-cell border on each side.
    //  - speed: Courant squared (C^2) stored in Q15. Clamped to [0.0, 0.5]
    //    (CFL stability bound for the 2D 5-point stencil — stricter than 1D).
    //    Values outside that range produce visual instability and are
    //    silently clamped.
    //  - dampening: exponent so that the damping factor is 2^dampening.
    WaveSimulation2D_Real(u32 W, u32 H, float speed = 0.16f,
                          float dampening = 6.0f) FL_NOEXCEPT;
    ~WaveSimulation2D_Real() FL_NOEXCEPT = default;

    // Set the simulation speed (Courant squared). Clamped to [0.0, 0.5] — see
    // constructor for rationale.
    void setSpeed(float something);

    // Set the dampening factor exponent.
    // The dampening factor used is 2^(dampening).
    void setDampening(int damp) FL_NOEXCEPT;

    // Get the current dampening exponent.
    int getDampenening() const;

    // Get the simulation speed as a float (converted from fixed Q15).
    float getSpeed() const;

    // Return the value at an inner grid cell (x,y), converted to float.
    // The value returned is in the range [-1.0, 1.0].
    float getf(fl::size x, fl::size y) const;

    // Return the value at an inner grid cell (x,y) as a fixed Q15 integer
    // in the range [-32768, 32767].
    i16 geti16(fl::size x, fl::size y) const;
    i16 geti16Previous(fl::size x, fl::size y) const;

    i8 geti8(fl::size x, fl::size y) const {
        return static_cast<i8>(geti16(x, y) >> 8);
    }

    u8 getu8(fl::size x, fl::size y) const {
        i16 value = geti16(x, y);
        // Rebase the range from [-32768, 32767] to [0, 65535] then extract the
        // upper 8 bits.
        // return static_cast<u8>(((static_cast<u16>(value) + 32768))
        // >>
        //                             8);
        if (mHalfDuplex) {
            u16 v2 = static_cast<u16>(value);
            v2 *= 2;
            return static_cast<u8>(v2 >> 8);
        } else {
            return static_cast<u8>(
                ((static_cast<u16>(value) + 32768)) >> 8);
        }
    }

    // Select the discrete Laplacian stencil used by update(). Default is
    // FivePoint (backward compatible). NinePointIsotropic costs ~2x reads
    // + ALU per cell but produces visibly rounder ripples at high super-
    // sample factors; the wrapper class auto-selects it when appropriate.
    void setStencil(LaplacianStencil s) FL_NOEXCEPT { mStencil = s; }
    LaplacianStencil getStencil() const FL_NOEXCEPT { return mStencil; }

    void setXCylindrical(bool on) { mXCylindrical = on; }

    // Check if (x,y) is within the inner grid.
    bool has(fl::size x, fl::size y) const;

    // Set the value at an inner grid cell using a float;
    // the value is stored in fixed Q15 format.
    // value shoudl be between -1.0 and 1.0.
    void setf(fl::size x, fl::size y, float value);

    void seti16(fl::size x, fl::size y, i16 value);

    void setHalfDuplex(bool on) { mHalfDuplex = on; }

    bool getHalfDuplex() const { return mHalfDuplex; }

    // Advance the simulation one time step using fixed-point arithmetic.
    void update();

    u32 getWidth() const { return width; }
    u32 getHeight() const { return height; }

  private:
    u32 width;  // Width of the inner grid.
    u32 height; // Height of the inner grid.
    u32 stride; // Row length (width + 2 for the borders).

    // Two separate grids stored in fixed Q15 format.
    fl::vector_psram<i16> grid1;
    fl::vector_psram<i16> grid2;

    fl::size whichGrid; // Indicates the active grid (0 or 1).

    i16 mCourantSq; // Fixed speed parameter in Q15.
    int mDampening;     // Dampening exponent; used as 2^(dampening).
    // Precomputed Q15 decay multiplier (1 - 1/2^mDampening). See the 1D
    // class for the rationale.
    i16 mDampDecayQ15;
    bool mHalfDuplex =
        true; // Flag to restrict values to positive range during update.
    bool mXCylindrical = false; // Default to non-cylindrical mode
    LaplacianStencil mStencil =
        LaplacianStencil::FivePoint; // Backward-compatible default
};

} // namespace fl
