#include <cstdint>
#include <cmath>
#include <algorithm>
#include <iostream>
#include "fl/warn.h"
#include "fl/scoped_ptr.h"

class WaveSimulation2D {
public:
    // Constructor: Initializes the simulation with dynamic width (W) and height (H).
    WaveSimulation2D(uint32_t W, uint32_t H)
        : width(W), height(H), stride(W + 2),
          grid1(new float[(W + 2) * (H + 2)]()),
          grid2(new float[(W + 2) * (H + 2)]()),
          courantSq_(0.16f), dampening(6.0f), whichGrid_(0)
    {
    }

    ~WaveSimulation2D() = default;

    // Set the simulation speed.
    void setSpeed(float something) { 
        courantSq_ = something; 
    }

    // Set the dampening value.
    void setDampenening(float damp) { 
        dampening = damp; 
    }

    float getDampening() const { 
        return dampening; 
    }

    float getSpeed() const { 
        return courantSq_; 
    }

    // Return the value at an inner grid cell (x,y)
    float get(size_t x, size_t y) const {
        if (x >= width || y >= height) {
            FASTLED_WARN("Out of range: " << x << ", " << y);
            return 0.0f;
        }
        const float* curr = (whichGrid_ == 0 ? grid1.get() : grid2.get());
        return curr[(y + 1) * stride + (x + 1)];
    }

    // Verify if (x,y) is within the inner grid.
    bool has(size_t x, size_t y) const {
        return (x < width && y < height);
    }

    // Set the value in the active grid at (x,y).
    void set(size_t x, size_t y, float value) {
        if (x >= width || y >= height) {
            // std::cerr << "Out of range: " << x << ", " << y << "\n";
            FASTLED_WARN("Out of range: " << x << ", " << y);
            return;
        }
        float* curr = (whichGrid_ == 0 ? grid1.get() : grid2.get());
        curr[(y + 1) * stride + (x + 1)] = value;
    }

    // Advance the simulation by one time step.
    void update() {
        // Define pointers to the current and next grids.
        float* curr = (whichGrid_ == 0 ? grid1.get() : grid2.get());
        float* next = (whichGrid_ == 0 ? grid2.get() : grid1.get());

        // Update horizontal boundaries for each row.
        for (size_t j = 0; j < height + 2; ++j) {
            curr[j * stride + 0] = curr[j * stride + 1];
            curr[j * stride + (width + 1)] = curr[j * stride + width];
        }

        // Update vertical boundaries for each column.
        for (size_t i = 0; i < width + 2; ++i) {
            curr[0 * stride + i] = curr[1 * stride + i];
            curr[(height + 1) * stride + i] = curr[height * stride + i];
        }

        const float dampening_factor = std::pow(2.0f, dampening);

        // Compute the new state of each inner cell.
        for (size_t j = 1; j <= height; ++j) {
            for (size_t i = 1; i <= width; ++i) {
                float f = -next[j * stride + i] +
                          2.0f * curr[j * stride + i] +
                          courantSq_ * ( curr[j * stride + (i + 1)] +
                                         curr[j * stride + (i - 1)] +
                                         curr[(j + 1) * stride + i] +
                                         curr[(j - 1) * stride + i] -
                                         4.0f * curr[j * stride + i] );
                f = f - (f / dampening_factor);
                f = std::max(-1.0f, std::min(1.0f, f));
                next[j * stride + i] = f;
            }
        }

        // Swap the current and next grids.
        whichGrid_ ^= 1;
    }

private:
    uint32_t width;    // Simulation width.
    uint32_t height;   // Simulation height.
    uint32_t stride;   // Number of elements per row (width + 2).

    // Two separate grids stored as scoped arrays.
    fl::scoped_array<float> grid1;
    fl::scoped_array<float> grid2;

    size_t whichGrid_;    // Indicates the active grid (0 or 1).
    float courantSq_;     // Speed parameter for the simulation.
    float dampening;      // Dampening factor.
};
