#pragma once

#include <stdint.h>

#include "fl/math_macros.h" // if needed for MAX/MIN macros
#include "fl/namespace.h"
#include "fl/scoped_ptr.h"
#include "fl/warn.h"
#include "fl/wave_simulation_real.h"

#include "fl/ptr.h"
#include "fl/supersample.h"
#include "fl/xymap.h"
#include "fx/fx.h"
#include "fx/fx2d.h"

namespace fl {

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
    //    The underlying simulation will have length * multiplier cells.
    //  - speed and dampening are passed on to the internal simulation.
    WaveSimulation1D(uint32_t length,
                     SuperSample factor = SuperSample::SUPER_SAMPLE_NONE,
                     float speed = 0.16f, int dampening = 6)
        : outerLength(length),
          multiplier(static_cast<uint32_t>(factor)),
          sim(new WaveSimulation1D_Real(length * multiplier, speed, dampening)) {
        // Extra updates (frames) are applied because the simulation slows down in
        // proportion to the supersampling factor.
        extraFrames = static_cast<uint8_t>(factor) - 1;
    }

    ~WaveSimulation1D() = default;

    // Delegate methods to the internal simulation.
    void setSpeed(float speed) { sim->setSpeed(speed); }
    void setDampenening(int damp) { sim->setDampenening(damp); }
    int getDampenening() const { return sim->getDampenening(); }
    float getSpeed() const { return sim->getSpeed(); }

    // Downsampled getter for the floating point value at index x.
    // It averages over the corresponding 'multiplier'-sized block in the high-res simulation.
    float get(size_t x) const {
        if (!has(x))
            return 0.0f;
        float sum = 0.0f;
        for (uint32_t i = 0; i < multiplier; ++i) {
            sum += sim->get(x * multiplier + i);
        }
        return sum / static_cast<float>(multiplier);
    }

    // Downsampled getter for the Q15 (fixed point) value at index x.
    // It averages the multiplier cells of Q15 values.
    int16_t geti16(size_t x) const {
        if (!has(x))
            return 0;
        int32_t sum = 0;
        for (uint32_t i = 0; i < multiplier; ++i) {
            sum += sim->geti16(x * multiplier + i);
        }
        return static_cast<int16_t>(sum / multiplier);
    }

    // Downsampled getters for the 8-bit representations.
    int8_t geti8(size_t x) const {
        return static_cast<int8_t>(geti16(x) >> 8);
    }

    uint8_t getu8(size_t x) const {
        int16_t value = geti16(x);
        return static_cast<uint8_t>(((static_cast<uint16_t>(value) + 32768)) >> 8);
    }

    // Check if x is within the bounds of the outer (downsampled) simulation.
    bool has(size_t x) const {
        return (x < outerLength);
    }

    // Upsampling setter: set the value at an outer grid cell x by replicating it
    // to the corresponding multiplier cells in the high-res simulation.
    void set(size_t x, float value) {
        if (!has(x))
            return;
        for (uint32_t i = 0; i < multiplier; ++i) {
            sim->set(x * multiplier + i, value);
        }
    }

    // Advance the simulation one time step.
    void update() {
        sim->update();
        for (uint8_t i = 0; i < extraFrames; ++i) {
            sim->update();
        }
    }

    // Get the outer (downsampled) grid length.
    uint32_t getLength() const { return outerLength; }

  private:
    uint32_t outerLength;   // Length of the downsampled simulation.
    uint8_t extraFrames = 0;
    uint32_t multiplier;    // Supersampling multiplier (e.g., 2, 4, or 8).
    // Internal high-resolution simulation.
    fl::scoped_ptr<WaveSimulation1D_Real> sim;
};


class WaveSimulation2D {
  public:
    // Constructor:
    //   - W and H specify the desired inner grid size of the downsampled
    //   simulation.
    //   - 'factor' selects the supersampling multiplier (e.g., 2x, 4x, or 8x).
    //   - Internally, the simulation is created with dimensions (factor*W x
    //   factor*H).
    //   - 'speed' and 'dampening' parameters are passed on to the internal
    //   simulation.
    WaveSimulation2D(uint32_t W, uint32_t H,
                     SuperSample factor = SuperSample::SUPER_SAMPLE_NONE,
                     float speed = 0.16f, float dampening = 6.0f)
        : outerWidth(W), outerHeight(H),
          multiplier(static_cast<uint32_t>(factor)),
          sim(new WaveSimulation2D_Real(W * multiplier, H * multiplier, speed,
                                        dampening)) {
        // Extra frames are needed because the simulation slows down in
        // proportion to the supersampling factor.
        extraFrames = uint8_t(factor) - 1;
    }

    ~WaveSimulation2D() = default;

    // Delegated simulation methods.
    void setSpeed(float speed) { sim->setSpeed(speed); }

    void setExtraFrames(uint8_t extra) { extraFrames = extra; }

    void setDampenening(int damp) { sim->setDampenening(damp); }

    int getDampenening() const { return sim->getDampenening(); }

    float getSpeed() const { return sim->getSpeed(); }

    // Downsampled getter for the floating point value at (x,y) in the outer
    // grid. It averages over the corresponding multiplier×multiplier block in
    // the high-res simulation.
    float getf(size_t x, size_t y) const {
        if (!has(x, y))
            return 0.0f;
        float sum = 0.0f;
        for (uint32_t j = 0; j < multiplier; ++j) {
            for (uint32_t i = 0; i < multiplier; ++i) {
                sum += sim->getf(x * multiplier + i, y * multiplier + j);
            }
        }
        return sum / static_cast<float>(multiplier * multiplier);
    }

    // Downsampled getter for the Q15 (fixed point) value at (x,y).
    // It averages the multiplier×multiplier block of Q15 values.
    int16_t geti16(size_t x, size_t y) const {
        if (!has(x, y))
            return 0;
        int32_t sum = 0;
        for (uint32_t j = 0; j < multiplier; ++j) {
            for (uint32_t i = 0; i < multiplier; ++i) {
                sum += sim->geti16(x * multiplier + i, y * multiplier + j);
            }
        }
        return static_cast<int16_t>(sum / (multiplier * multiplier));
    }

    // Downsampled getters for the 8-bit representations.
    int8_t geti8(size_t x, size_t y) const {
        return static_cast<int8_t>(geti16(x, y) >> 8);
    }

    uint8_t getu8(size_t x, size_t y) const {
        int16_t value = geti16(x, y);
        return static_cast<uint8_t>(((static_cast<uint16_t>(value) + 32768)) >>
                                    8);
    }

    // Check if (x,y) is within the bounds of the outer (downsampled) grid.
    bool has(size_t x, size_t y) const {
        return (x < outerWidth) && (y < outerHeight);
    }

    // Upsampling setter: set the value at an outer grid cell (x,y) by
    // replicating it to all cells of the corresponding multiplier×multiplier
    // block in the high-res simulation.
    void set(size_t x, size_t y, float value) {
        if (!has(x, y))
            return;
        for (uint32_t j = 0; j < multiplier; ++j) {
            for (uint32_t i = 0; i < multiplier; ++i) {
                sim->set(x * multiplier + i, y * multiplier + j, value);
            }
        }
    }

    // Advance the simulation one time step.
    void update() {
        sim->update();
        for (uint8_t i = 0; i < extraFrames; ++i) {
            sim->update();
        }
    }

    // Get the outer grid dimensions.
    uint32_t getWidth() const { return outerWidth; }
    uint32_t getHeight() const { return outerHeight; }

  private:
    uint32_t outerWidth;  // Width of the downsampled (outer) grid.
    uint32_t outerHeight; // Height of the downsampled (outer) grid.
    uint8_t extraFrames = 0;
    uint32_t multiplier; // Supersampling multiplier (e.g., 2, 4, or 8).
    // Internal high-resolution simulation.
    fl::scoped_ptr<WaveSimulation2D_Real> sim;
};

} // namespace fl
