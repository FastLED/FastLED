

#include <stdint.h>
#include "fl/warn.h"

// Two dimensional wave simulation.
template <uint32_t W, uint32_t H> class WaveSimulation2D {
  public:
    WaveSimulation2D() = default;
    ~WaveSimulation2D() = default;

    void setSpeed(float something) { courantSq_ = something; }

    void setDampenening(float damp) { dampening = damp; }

    float getDampening() const { return dampening; }

    float getSpeed() const { return courantSq_; }

    float get(size_t x, size_t y) const {
        if (x >= W || y >= H) {
            FASTLED_WARN("Out of range: " << x << ", " << y);
            return 0.0f;
        }
        return grid[whichGrid_][y + 1][x + 1];
    }

    bool has(size_t x, size_t y) const {
        if (x >= W || y >= H) {
            return false;
        }
        return true;
    }

    void set(size_t x, size_t y, float value) {
        if (x >= W || y >= H) {
            //printf("Out of range.\n");
            FASTLED_WARN("Out of range: " << x << ", " << y);
            return;
        }
        grid[whichGrid_][y + 1][x + 1] = value;
    }

    void update() {
        float (*curr)[W + 2] = grid[whichGrid_];
        float (*next)[W + 2] = grid[whichGrid_ ^= 1]; // also toggles whichGrid.

        for (size_t i = 0; i < H + 2; i++) {
            curr[i][0] = curr[i][1];
            curr[i][W + 1] = curr[i][W];
        }
        for (size_t i = 0; i < W + 2; i++) {
            curr[0][i] = curr[1][i];
            curr[H + 1][i] = curr[H][i];
        }

        const float dampening_factor = pow(2.0, dampening);

        for (size_t j = 1; j <= H; j++) {
            for (size_t i = 1; i <= W; i++) {
                float f = -next[j][i] + 2.0f * curr[j][i] +
                          courantSq_ * (curr[j][i + 1] + curr[j][i - 1] +
                                        curr[j + 1][i] + curr[j - 1][i] -
                                        4.0f * curr[j][i]);
                f = f - (f / dampening_factor);
                f = std::max<float>(-1.0f, std::min<float>(1.0f, f));
                next[j][i] = f;
            }
        }
    }

  private:
    size_t whichGrid_ = 0;
    float grid[2][H + 2][W + 2] = {
        {{0.0f}}}; // Two extra for the boundary condition.
    float courantSq_ = 0.16f;
    float dampening = 6.0f;
};