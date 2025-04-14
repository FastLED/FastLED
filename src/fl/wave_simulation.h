


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


class WaveSimulation1D {
  public:
    // Constructor:
    //  - length: inner simulation grid length (excluding the 2 boundary cells).
    //  - speed: simulation speed (in float, will be stored in Q15).
    //  - dampening: exponent so that the effective damping factor is 2^(dampening).
    WaveSimulation1D(uint32_t length, float speed = 0.16f, int dampening = 6);
    ~WaveSimulation1D() = default;

    // Set simulation speed (courant parameter) using a float.
    void setSpeed(float something);

    // Set the dampening exponent (effective damping factor is 2^(dampening)).
    void setDampenening(int damp);

    // Get the current dampening exponent.
    int getDampenening() const;

    // Get the simulation speed as a float.
    float getSpeed() const;

    // Get the simulation value at the inner grid cell x (converted to float in the range [-1.0, 1.0]).
    float get(size_t x) const;

    int16_t geti16(size_t x) const;

    int8_t geti8(size_t x) const {
        return static_cast<int8_t>(geti16(x) >> 8);
    }

    uint8_t getu8(size_t x) const {
        int16_t value = geti16(x);
        // Rebase the range from [-32768, 32767] to [0, 65535] then extract the
        // upper 8 bits.
        return static_cast<uint8_t>(((static_cast<uint16_t>(value) + 32768)) >>
                                    8);
    }

    // Returns whether x is within the inner grid bounds.
    bool has(size_t x) const;

    // Set the value at grid cell x (expected range: [-1.0, 1.0]); the value is stored in Q15.
    void set(size_t x, float value);

    // Advance the simulation one time step.
    void update();

  private:
    uint32_t length; // Length of the inner simulation grid.
    // Two grids stored in fixed Q15 format, each with length+2 entries (including boundary cells).
    fl::scoped_array<int16_t> grid1;
    fl::scoped_array<int16_t> grid2;
    size_t whichGrid; // Indicates the active grid (0 or 1).

    int16_t mCourantSq; // Simulation speed (courant squared) stored in Q15.
    int mDampenening;    // Dampening exponent (damping factor = 2^(mDampenening)).
};




class WaveSimulation2D {
  public:
    // Constructor: Initializes the simulation with inner grid size (W x H).
    // The grid dimensions include a 1-cell border on each side.
    // Here, 'speed' is specified as a float (converted to fixed Q15)
    // and 'dampening' is given as an exponent so that the damping factor is
    // 2^dampening.
    WaveSimulation2D(uint32_t W, uint32_t H, float speed = 0.16f,
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


// WaveSimulation2D2x implements the same API as WaveSimulation2D but
// internally maintains a WaveSimulation2D at double resolution.
// It then downsamples the high-resolution simulation output for improved visual quality.
class WaveSimulation2D2x {
public:
    // Constructor:
    //   - W and H specify the desired inner grid size of the downsampled simulation.
    //   - Internally, the simulation is created with dimensions (2*W x 2*H).
    //   - courantSq and dampening parameters are passed to the internal simulation.
    WaveSimulation2D2x(uint32_t W, uint32_t H, float speed = 0.16f, float dampening = 6.0f)
        : outerWidth(W), outerHeight(H),
          // Create the internal simulation at 2x resolution.
          sim(new WaveSimulation2D(W * 2, H * 2, speed, dampening))
    { }

    ~WaveSimulation2D2x() = default;

    // Set the simulation speed (courantSq) using a float value.
    void setSpeed(float speed) {
        sim->setSpeed(speed);
    }

    // Set the dampening exponent.
    void setDampenening(int damp) {
        sim->setDampenening(damp);
    }

    // Get the current dampening exponent.
    int getDampenening() const {
        return sim->getDampenening();
    }

    // Get the simulation speed as a float.
    float getSpeed() const {
        return sim->getSpeed();
    }

    // Return the downsampled float value at inner cell (x,y).
    // This is computed by averaging the corresponding 2x2 block from the high-res simulation.
    float getf(size_t x, size_t y) const {
        if (!has(x, y))
            return 0.0f;
        float v1 = sim->getf(2 * x,     2 * y);
        float v2 = sim->getf(2 * x + 1, 2 * y);
        float v3 = sim->getf(2 * x,     2 * y + 1);
        float v4 = sim->getf(2 * x + 1, 2 * y + 1);
        return (v1 + v2 + v3 + v4) * 0.25f;
    }

    // Return the downsampled fixed Q15 value at inner cell (x,y).
    // Here, we average the 2x2 block values from the high-res simulation.
    int16_t geti16(size_t x, size_t y) const {
        if (!has(x, y))
            return 0;
        int32_t a = sim->geti16(2 * x,     2 * y);
        int32_t b = sim->geti16(2 * x + 1, 2 * y);
        int32_t c = sim->geti16(2 * x,     2 * y + 1);
        int32_t d = sim->geti16(2 * x + 1, 2 * y + 1);
        return static_cast<int16_t>((a + b + c + d) / 4);
    }

    // Return the downsampled 8-bit signed value.
    int8_t geti8(size_t x, size_t y) const {
        return static_cast<int8_t>(geti16(x, y) >> 8);
    }

    // Return the downsampled 8-bit unsigned value.
    uint8_t getu8(size_t x, size_t y) const {
        int16_t value = geti16(x, y);
        return static_cast<uint8_t>(((static_cast<uint16_t>(value) + 32768)) >> 8);
    }

    // Check if (x,y) is within the bounds of the outer (downsampled) grid.
    bool has(size_t x, size_t y) const {
        return (x < outerWidth) && (y < outerHeight);
    }

    // Set the value at an outer grid cell (x,y) by upsampling:
    // The corresponding 2x2 block in the high-res simulation is all set to the given value.
    void set(size_t x, size_t y, float value) {
        if (!has(x, y))
            return;
        sim->set(2 * x,     2 * y,     value);
        sim->set(2 * x + 1, 2 * y,     value);
        sim->set(2 * x,     2 * y + 1, value);
        sim->set(2 * x + 1, 2 * y + 1, value);
    }

    // Advance the simulation one time step.
    void update() {
        sim->update();
    }

    // Get the outer grid width.
    uint32_t getWidth() const { return outerWidth; }
    
    // Get the outer grid height.
    uint32_t getHeight() const { return outerHeight; }

private:
    uint32_t outerWidth;   // Width of the downsampled grid.
    uint32_t outerHeight;  // Height of the downsampled grid.
    // Internal high-resolution simulation (2x in each dimension).
    fl::scoped_ptr<WaveSimulation2D> sim;
};

} // namespace fl
