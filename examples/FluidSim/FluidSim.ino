// @filter: (memory is large)
/// @file    FluidSim.ino
/// @brief   Stable-fluids (Navier-Stokes) solver on a 2D LED matrix
/// @example FluidSim.ino
///
/// A real incompressible fluid solver — advection, diffusion and a pressure
/// projection step — following Jos Stam's "Stable Fluids" (SIGGRAPH 1999).
/// Three Gaussian splat injectors drive it; jets modulate their angle from
/// noise so the motion is emergent rather than keyframed.
///
/// Inspired by Stefan Petrick's FastFluid experiments (AnimARTrix author).
///
/// This sketch is fully compatible with the FastLED web compiler:
///   fastled examples/FluidSim --test --test-screenshot frame.png
///
/// Grid is 64x64 by default. Memory is (W+2)*(H+2)*4 bytes per field and the
/// solver keeps 8 fields, so ~136 KB at 64x64 — PSRAM-class parts only.

#include <FastLED.h>

#include "fl/math/xymap.h"
#include "fl/stl/vector.h"
#include "fl/ui/ui.h"

// -- Matrix config --
#define WIDTH 64
#define HEIGHT 64
#define NUM_LEDS (WIDTH * HEIGHT)
#define DATA_PIN 2
#define BRIGHTNESS 255
#define SERPENTINE true

CRGB leds[NUM_LEDS];
fl::XYMap xyMap(WIDTH, HEIGHT, SERPENTINE);

// -- UI controls --
fl::UITitle title("FluidSim");
fl::UIDescription description(
    "Incompressible Navier-Stokes solver (Jos Stam, Stable Fluids 1999) on a "
    "2D LED matrix. Three Gaussian splat injectors with noise-modulated jets. "
    "Concept by Stefan Petrick, solver implementation for FastLED.");
fl::UISlider viscosity("Viscosity", 0.0001f, 0.0f, 0.001f, 0.00001f);
fl::UISlider diffusion("Diffusion", 0.0001f, 0.0f, 0.001f, 0.00001f);
fl::UISlider dissipation("Density Dissipation", 0.99f, 0.90f, 1.0f, 0.001f);
fl::UISlider forceStrength("Force Strength", 12.0f, 0.5f, 40.0f, 0.1f);
fl::UISlider splatRadius("Splat Radius", 2.5f, 1.0f, 12.0f, 0.1f);
fl::UISlider jetSpeed("Jet Angle Speed", 0.35f, 0.0f, 2.0f, 0.01f);
fl::UISlider iterations("Solver Iterations", 12, 4, 40, 1);
fl::UIGroup solverGroup("Solver", viscosity, diffusion, dissipation, iterations);
fl::UIGroup injectorGroup("Injectors", forceStrength, splatRadius, jetSpeed);
fl::UIDropdown colorMode("Color Mode", {"Heat", "Ink", "Aurora"});
fl::UIGroup appearanceGroup("Appearance", colorMode);

// ---------------------------------------------------------------------------
// Stable-fluids solver
// ---------------------------------------------------------------------------
namespace fluid {

// Padded grid: index 0 and W+1 / H+1 are boundary cells.
class Solver {
  public:
    Solver(int w, int h)
        : mW(w), mH(h), mStride(w + 2), mCells((w + 2) * (h + 2)) {
        mU.resize(mCells);
        mV.resize(mCells);
        mU0.resize(mCells);
        mV0.resize(mCells);
        mD.resize(mCells);
        mD0.resize(mCells);
        mP.resize(mCells);
        mDiv.resize(mCells);
        for (int i = 0; i < mCells; ++i) {
            mU[i] = mV[i] = mU0[i] = mV0[i] = 0.0f;
            mD[i] = mD0[i] = mP[i] = mDiv[i] = 0.0f;
        }
    }

    int width() const { return mW; }
    int height() const { return mH; }

    /// One full simulation step: velocity advection + projection, then density
    /// advection. `iter` is the Gauss-Seidel iteration count for the linear
    /// solves and the pressure projection.
    void step(float dt, float visc, float diff, float dissipate, int iter) {
        velStep(dt, visc, iter);
        densStep(dt, diff, dissipate, iter);
    }

    /// Gaussian splat of force (fx, fy) centered at (x, y) in grid space.
    void addForce(float x, float y, float fx, float fy, float radius) {
        const float r2 = radius * radius;
        for (int j = 1; j <= mH; ++j) {
            for (int i = 1; i <= mW; ++i) {
                const float dx = i - x;
                const float dy = j - y;
                const float w = fl::expf(-(dx * dx + dy * dy) / r2);
                if (w < 0.001f) {
                    continue;
                }
                mU[ix(i, j)] += fx * w;
                mV[ix(i, j)] += fy * w;
            }
        }
    }

    /// Gaussian splat of density centered at (x, y) in grid space.
    void addDensity(float x, float y, float amount, float radius) {
        const float r2 = radius * radius;
        for (int j = 1; j <= mH; ++j) {
            for (int i = 1; i <= mW; ++i) {
                const float dx = i - x;
                const float dy = j - y;
                const float w = fl::expf(-(dx * dx + dy * dy) / r2);
                if (w < 0.001f) {
                    continue;
                }
                mD[ix(i, j)] += amount * w;
            }
        }
    }

    float density(int i, int j) const { return mD[ix(i, j)]; }

  private:
    int ix(int i, int j) const { return i + mStride * j; }

    void velStep(float dt, float visc, int iter) {
        // Diffuse velocity, then project to a divergence-free field.
        diffuse(1, mU0, mU, visc, dt, iter);
        diffuse(2, mV0, mV, visc, dt, iter);
        project(mU0, mV0, mP, mDiv, iter);
        // Advect velocity by itself, then project again.
        advect(1, mU, mU0, mU0, mV0, dt);
        advect(2, mV, mV0, mU0, mV0, dt);
        project(mU, mV, mP, mDiv, iter);
    }

    void densStep(float dt, float diff, float dissipate, int iter) {
        diffuse(0, mD0, mD, diff, dt, iter);
        advect(0, mD, mD0, mU, mV, dt);
        // Dissipation is applied as a per-frame multiply, matching the
        // "smoke fades out" behaviour of the reference implementation.
        for (int i = 0; i < mCells; ++i) {
            mD[i] *= dissipate;
        }
    }

    /// Gauss-Seidel relaxation for (I - a*L) x = x0.
    void linSolve(int b, fl::vector<float> &x, const fl::vector<float> &x0,
                  float a, float c, int iter) {
        const float cRecip = 1.0f / c;
        for (int k = 0; k < iter; ++k) {
            for (int j = 1; j <= mH; ++j) {
                for (int i = 1; i <= mW; ++i) {
                    const int idx = ix(i, j);
                    x[idx] = (x0[idx] + a * (x[idx - 1] + x[idx + 1] +
                                             x[idx - mStride] +
                                             x[idx + mStride])) *
                             cRecip;
                }
            }
            setBnd(b, x);
        }
    }

    void diffuse(int b, fl::vector<float> &x, const fl::vector<float> &x0,
                 float diff, float dt, int iter) {
        const float a = dt * diff * static_cast<float>(mW * mH);
        linSolve(b, x, x0, a, 1.0f + 4.0f * a, iter);
    }

    void project(fl::vector<float> &u, fl::vector<float> &v,
                 fl::vector<float> &p, fl::vector<float> &div, int iter) {
        const float h = 1.0f / static_cast<float>(mW);
        for (int j = 1; j <= mH; ++j) {
            for (int i = 1; i <= mW; ++i) {
                const int idx = ix(i, j);
                div[idx] = -0.5f * h *
                           (u[idx + 1] - u[idx - 1] + v[idx + mStride] -
                            v[idx - mStride]);
                p[idx] = 0.0f;
            }
        }
        setBnd(0, div);
        setBnd(0, p);
        linSolve(0, p, div, 1.0f, 4.0f, iter);
        for (int j = 1; j <= mH; ++j) {
            for (int i = 1; i <= mW; ++i) {
                const int idx = ix(i, j);
                u[idx] -= 0.5f * (p[idx + 1] - p[idx - 1]) / h;
                v[idx] -= 0.5f * (p[idx + mStride] - p[idx - mStride]) / h;
            }
        }
        setBnd(1, u);
        setBnd(2, v);
    }

    /// Semi-Lagrangian advection with bilinear interpolation.
    void advect(int b, fl::vector<float> &d, const fl::vector<float> &d0,
                const fl::vector<float> &u, const fl::vector<float> &v,
                float dt) {
        const float dt0x = dt * static_cast<float>(mW);
        const float dt0y = dt * static_cast<float>(mH);
        for (int j = 1; j <= mH; ++j) {
            for (int i = 1; i <= mW; ++i) {
                const int idx = ix(i, j);
                float x = static_cast<float>(i) - dt0x * u[idx];
                float y = static_cast<float>(j) - dt0y * v[idx];
                x = fl::clamp(x, 0.5f, static_cast<float>(mW) + 0.5f);
                y = fl::clamp(y, 0.5f, static_cast<float>(mH) + 0.5f);
                const int i0 = static_cast<int>(x);
                const int j0 = static_cast<int>(y);
                const int i1 = i0 + 1;
                const int j1 = j0 + 1;
                const float s1 = x - static_cast<float>(i0);
                const float s0 = 1.0f - s1;
                const float t1 = y - static_cast<float>(j0);
                const float t0 = 1.0f - t1;
                d[idx] = s0 * (t0 * d0[ix(i0, j0)] + t1 * d0[ix(i0, j1)]) +
                         s1 * (t0 * d0[ix(i1, j0)] + t1 * d0[ix(i1, j1)]);
            }
        }
        setBnd(b, d);
    }

    /// Boundary conditions: b=1 -> horizontal velocity, b=2 -> vertical.
    void setBnd(int b, fl::vector<float> &x) {
        for (int i = 1; i <= mW; ++i) {
            x[ix(i, 0)] = (b == 2) ? -x[ix(i, 1)] : x[ix(i, 1)];
            x[ix(i, mH + 1)] = (b == 2) ? -x[ix(i, mH)] : x[ix(i, mH)];
        }
        for (int j = 1; j <= mH; ++j) {
            x[ix(0, j)] = (b == 1) ? -x[ix(1, j)] : x[ix(1, j)];
            x[ix(mW + 1, j)] = (b == 1) ? -x[ix(mW, j)] : x[ix(mW, j)];
        }
        x[ix(0, 0)] = 0.5f * (x[ix(1, 0)] + x[ix(0, 1)]);
        x[ix(0, mH + 1)] = 0.5f * (x[ix(1, mH + 1)] + x[ix(0, mH)]);
        x[ix(mW + 1, 0)] = 0.5f * (x[ix(mW, 0)] + x[ix(mW + 1, 1)]);
        x[ix(mW + 1, mH + 1)] =
            0.5f * (x[ix(mW, mH + 1)] + x[ix(mW + 1, mH)]);
    }

    int mW;
    int mH;
    int mStride;
    int mCells;
    fl::vector<float> mU;
    fl::vector<float> mV;
    fl::vector<float> mU0;
    fl::vector<float> mV0;
    fl::vector<float> mD;
    fl::vector<float> mD0;
    fl::vector<float> mP;
    fl::vector<float> mDiv;
};

} // namespace fluid

fluid::Solver solver(WIDTH, HEIGHT);

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS).setScreenMap(xyMap);
    FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
    static uint32_t last = 0;
    const uint32_t now = fl::millis();
    float dt = (last == 0) ? 0.016f : static_cast<float>(now - last) * 0.001f;
    last = now;
    if (dt > 0.05f) {
        dt = 0.05f; // clamp long frames so the solver stays stable
    }

    const float t = static_cast<float>(now) * 0.001f;
    const float jet = jetSpeed.as<float>();

    // Three Gaussian splat injectors whose angles are noise-modulated.
    // Force is applied every frame so the field keeps moving; dye is injected
    // only every 12th frame, otherwise fresh blobs swamp the advected
    // structure and the image reads as a static heat map instead of a fluid.
    static uint32_t frame = 0;
    const bool injectDye = (frame % 12) == 0;
    ++frame;
    for (int k = 0; k < 3; ++k) {
        const float phase = t * jet + static_cast<float>(k) * 2.094f;
        const float angle = fl::sinf(phase) * 1.7f + fl::cosf(phase * 0.7f);
        const float cx = (0.5f + 0.35f * fl::sinf(phase * 0.83f)) *
                         static_cast<float>(WIDTH);
        const float cy = (0.5f + 0.35f * fl::cosf(phase * 0.61f)) *
                         static_cast<float>(HEIGHT);
        const float mag = forceStrength.as<float>();
        solver.addForce(cx, cy, fl::cosf(angle) * mag, fl::sinf(angle) * mag,
                        splatRadius.as<float>());
        if (injectDye) {
            solver.addDensity(cx, cy, 0.9f, splatRadius.as<float>());
        }
    }

    solver.step(dt, viscosity.as<float>(), diffusion.as<float>(),
                dissipation.as<float>(), iterations.as<int>());

    // Render density to the matrix. Auto-exposure keeps the image readable
    // without hand-tuning the injection rate per grid size: the brightest
    // cell maps to full scale, everything else is relative to it.
    const int mode = colorMode.as_int();
    float peak = 0.0f;
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            const float d = solver.density(x + 1, y + 1);
            if (d > peak) {
                peak = d;
            }
        }
    }
    const float exposure = (peak > 0.001f) ? (1.0f / peak) : 0.0f;
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            // sqrt tone map: lifts the low end so advected wisps stay visible
            // instead of collapsing into a flat mid-tone.
            const float d = fl::sqrtf(fl::clamp(solver.density(x + 1, y + 1) * exposure, 0.0f, 1.0f));
            const uint8_t v = static_cast<uint8_t>(d * 255.0f);
            CRGB c;
            if (mode == 0) {
                c = HeatColor(v);
            } else if (mode == 1) {
                c = CRGB(v, static_cast<uint8_t>(v / 3), static_cast<uint8_t>(255 - v));
            } else {
                c = CRGB(static_cast<uint8_t>(v / 2), v, static_cast<uint8_t>(v / 4 + 40));
            }
            leds[xyMap.mapToIndex(x, y)] = c;
        }
    }

    FastLED.show();
}
