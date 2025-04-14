

#include <cmath>
#include <stdint.h>

#include "fl/math_macros.h" // if needed for MAX/MIN macros
#include "fl/namespace.h"
#include "fl/scoped_ptr.h"
#include "fl/warn.h"

#include "fl/ptr.h"
#include "fl/xymap.h"
#include "fx/fx.h"
#include "fx/fx2d.h"

namespace fl {

// Convert float to fixed Q15.
int16_t float_to_fixed(float f);

// Convert fixed Q15 to float.
float fixed_to_float(int16_t f);

// Multiply two Q15 fixed point numbers.
int16_t fixed_mul(int16_t a, int16_t b);

template <size_t N> class WaveSimulation1D {
  public:
    WaveSimulation1D() = default;
    ~WaveSimulation1D() = default;

    // Set the simulation speed (courant parameter) as a float.
    // Internally, it will be stored in Q15 fixed point.
    void setSpeed(float something) { mCourantSq = float_to_fixed(something); }

    // Set the dampening exponent.
    // The effective dampening factor is 2^(mDampenening).
    void setDampenening(int damp) { mDampenening = damp; }

    // Get the current dampening exponent.
    int getDampenening() const { return mDampenening; }

    // Get the simulation speed as a float (converted from fixed Q15).
    float getSpeed() const { return fixed_to_float(mCourantSq); }

    // Get the value at position x (as a float in the range [-1.0, 1.0]).
    float get(size_t x) const {
        if (x >= N) {
            FASTLED_WARN("Out of range.");
            return 0.0f;
        }
        // Note the +1 offset is for the boundary cell.
        return fixed_to_float(grid[whichGrid][x + 1]);
    }

    // Check if the index is within the simulation grid.
    bool has(size_t x) const { return x < N; }

    // Set the value at position x using a float (expected range: [-1.0, 1.0]).
    // The value is converted and stored in fixed Q15 format.
    void set(int x, float value) {
        if (x >= N) {
            FASTLED_WARN("warning X value too high");
            return;
        }
        if (x < 0) {
            FASTLED_WARN("warning X value is negative");
            return;
        }
        // Clamp x for safety, then store.
        x = MAX(0, MIN(x, N - 1));
        grid[whichGrid][x + 1] = float_to_fixed(value);
    }

    // Advance the simulation one time step using fixed-point arithmetic.
    void update() {
        // Obtain pointers to the current and next grids.
        int16_t *curr = grid[whichGrid];
        int16_t *next = grid[whichGrid ^= 1]; // Toggle active grid.

        // Update the boundary conditions.
        // Use a zero-gradient (Neumann) boundary: copy the adjacent value.
        curr[0] = curr[1];
        curr[N + 1] = curr[N];

        // Compute the dampening factor as an integer: 2^(mDampenening)
        int32_t dampening_factor = 1 << mDampenening;

        // Iterate over each inner cell.
        for (size_t i = 1; i < N + 1; i++) {
            // Compute the discrete second derivative (the 1D Laplacian):
            // lap = curr[i+1] - 2*curr[i] + curr[i-1]
            int32_t lap =
                (int32_t)curr[i + 1] - ((int32_t)curr[i] << 1) + curr[i - 1];

            // Multiply the Laplacian by the speed parameter (Q15
            // multiplication). The product is adjusted back to Q15.
            int32_t term = ((int32_t)mCourantSq * lap) >> 15;

            // Compute the new state:
            // f = -next[i] + 2*curr[i] + term
            int32_t f = -(int32_t)next[i] + ((int32_t)curr[i] << 1) + term;

            // Apply damping:
            f = f - (f / dampening_factor);

            // Clamp f to the valid Q15 range.
            if (f > 32767)
                f = 32767;
            else if (f < -32768)
                f = -32768;

            next[i] = (int16_t)f;
        }
    }

  private:
    size_t whichGrid = 0;
    // Two grids with extra boundary cells at both ends.
    int16_t grid[2][N + 2] = {{0}, {0}};
    // Simulation speed (courant squared) stored in fixed Q15.
    int16_t mCourantSq = float_to_fixed(0.16f);
    // Dampening exponent: effective damping factor is 2^(mDampenening).
    int mDampenening = 6;
};

class WaveSimulation2D {
  public:
    // Constructor: Initializes the simulation with inner grid size (W x H).
    // The grid dimensions include a 1-cell border on each side.
    // Here, 'speed' is specified as a float (converted to fixed Q15)
    // and 'dampening' is given as an exponent so that the damping factor is
    // 2^dampening.
    WaveSimulation2D(uint32_t W, uint32_t H, float courantSq = 0.16f,
                     float dampening = 6.0f);
    ~WaveSimulation2D() = default;

    // Set the simulation speed (courantSq) using a float value.
    void setSpeed(float something);

    // Set the dampening factor exponent.
    // The dampening factor used is 2^(dampening).
    void setDampenening(int damp);

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

    int8_t geti8(size_t x, size_t y) const {
        return static_cast<int8_t>(geti16(x, y) >> 8);
    }

    uint8_t getu8(size_t x, size_t y) const {
        int16_t value = geti16(x, y);
        // Rebase the range from [-32768, 32767] to [0, 65535] then extract the
        // upper 8 bits.
        return static_cast<uint8_t>(((static_cast<uint16_t>(value) + 32768)) >>
                                    8);
    }

    // Check if (x,y) is within the inner grid.
    bool has(size_t x, size_t y) const;

    // Set the value at an inner grid cell using a float;
    // the value is stored in fixed Q15 format.
    // value shoudl be between -1.0 and 1.0.
    void set(size_t x, size_t y, float value);

    // Advance the simulation one time step using fixed-point arithmetic.
    void update();

    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }

  private:
    uint32_t width;  // Width of the inner grid.
    uint32_t height; // Height of the inner grid.
    uint32_t stride; // Row length (width + 2 for the borders).

    // Two separate grids stored in fixed Q15 format.
    fl::scoped_array<int16_t> grid1;
    fl::scoped_array<int16_t> grid2;

    size_t whichGrid; // Indicates the active grid (0 or 1).

    int16_t mCourantSq; // Fixed speed parameter in Q15.
    int mDampening;     // Dampening exponent; used as 2^(dampening).
};

} // namespace fl
