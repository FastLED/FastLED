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
// 64x64 needs ~136 KiB of solver state, so this example is filtered for
// memory-large boards. Override the grid at compile time for smaller parts:
//   -DFLUID_GRID=32   (36.1 KiB) and -DFLUID_GRID=48 (78.1 KiB)
#ifndef FLUID_GRID
#define FLUID_GRID 64
#endif
#define WIDTH FLUID_GRID
#define HEIGHT FLUID_GRID
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
    ///
    /// `fx`/`fy` are accelerations, so the impulse is scaled by `dt` exactly as
    /// Stam's `add_source` does. Without the dt factor the injected impulse per
    /// second is proportional to the frame rate, and the simulation visibly
    /// runs at a different speed on a 20 fps part than on a 60 fps one.
    ///
    /// The weight is only evaluated over a 3-sigma bounding box; outside that
    /// the Gaussian is below 0.001 and contributes nothing.
    void addForce(float dt, float x, float y, float fx, float fy, float radius) {
        const float r2 = radius * radius;
        const float invR2 = 1.0f / r2;
        const int i0 = fl::max(1, static_cast<int>(x - 3.0f * radius));
        const int i1 = fl::min(mW, static_cast<int>(x + 3.0f * radius) + 1);
        const int j0 = fl::max(1, static_cast<int>(y - 3.0f * radius));
        const int j1 = fl::min(mH, static_cast<int>(y + 3.0f * radius) + 1);
        for (int j = j0; j <= j1; ++j) {
            const float dy = static_cast<float>(j) - y;
            for (int i = i0; i <= i1; ++i) {
                const float dx = static_cast<float>(i) - x;
                const float w = fl::expf(-(dx * dx + dy * dy) * invR2);
                if (w < 0.001f) {
                    continue;
                }
                mU[ix(i, j)] += dt * fx * w;
                mV[ix(i, j)] += dt * fy * w;
            }
        }
    }

    /// Gaussian splat of density centered at (x, y) in grid space. `amount` is a
    /// rate (density per second), scaled by `dt` like `addForce`.
    void addDensity(float dt, float x, float y, float amount, float radius) {
        const float r2 = radius * radius;
        const float invR2 = 1.0f / r2;
        const int i0 = fl::max(1, static_cast<int>(x - 3.0f * radius));
        const int i1 = fl::min(mW, static_cast<int>(x + 3.0f * radius) + 1);
        const int j0 = fl::max(1, static_cast<int>(y - 3.0f * radius));
        const int j1 = fl::min(mH, static_cast<int>(y + 3.0f * radius) + 1);
        for (int j = j0; j <= j1; ++j) {
            const float dy = static_cast<float>(j) - y;
            for (int i = i0; i <= i1; ++i) {
                const float dx = static_cast<float>(i) - x;
                const float w = fl::expf(-(dx * dx + dy * dy) * invR2);
                if (w < 0.001f) {
                    continue;
                }
                mD[ix(i, j)] += dt * amount * w;
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
        // `dissipate` is documented as the fraction of density surviving one
        // 60 fps frame. Applying it raw would make the smoke lifetime scale
        // with frame rate (1.7 s at 60 fps, 5 s at 20 fps), so raise it to
        // dt*60 to get the same decay per second at any frame rate.
        const float diss = fl::powf(dissipate, dt * 60.0f);
        for (int i = 0; i < mCells; ++i) {
            mD[i] *= diss;
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
        // h = 1/mW (see project()), so the Laplacian coefficient is dt*diff/h^2.
        // Using mW*mH here is only correct on a square grid.
        const float a = dt * diff * static_cast<float>(mW * mW);
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
    // Force is applied every frame so the solver keeps moving; dye is injected
    // only every 0.2 s, otherwise fresh blobs swamp the advected structure and
    // the image reads as a static heat map instead of a fluid.
    // Both are dt-scaled, so the visual speed is frame-rate independent; the
    // dye interval is wall-clock too, so a 20 fps part gets the same injection
    // cadence as a 60 fps one.
    static float dyeAccumulator = 0.0f;
    dyeAccumulator += dt;
    const bool injectDye = dyeAccumulator >= 0.2f;
    if (injectDye) {
        dyeAccumulator = 0.0f;
    }
    for (int k = 0; k < 3; ++k) {
        const float phase = t * jet + static_cast<float>(k) * 2.094f;
        const float angle = fl::sinf(phase) * 1.7f + fl::cosf(phase * 0.7f);
        const float cx = (0.5f + 0.35f * fl::sinf(phase * 0.83f)) *
                         static_cast<float>(WIDTH);
        const float cy = (0.5f + 0.35f * fl::cosf(phase * 0.61f)) *
                         static_cast<float>(HEIGHT);
        const float mag = forceStrength.as<float>();
        solver.addForce(dt, cx, cy, fl::cosf(angle) * mag, fl::sinf(angle) * mag,
                        splatRadius.as<float>());
        if (injectDye) {
            solver.addDensity(dt, cx, cy, 0.9f, splatRadius.as<float>());
        }
    }

    solver.step(dt, viscosity.as<float>(), diffusion.as<float>(),
                dissipation.as<float>(), iterations.as<int>());

    // Render density to the matrix. Exposure follows the peak with a slow
    // decay (2 s time constant) instead of renormalising every frame: a
    // per-frame 1/peak would strobe the whole matrix whenever the brightest
    // cell changes, and it hides whether density is actually bounded.
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
    static float smoothPeak = 0.0f;
    if (peak > smoothPeak) {
        smoothPeak = peak; // rise immediately so nothing clips
    } else {
        const float k = 1.0f - fl::expf(-dt / 2.0f);
        smoothPeak += (peak - smoothPeak) * k;
    }
    if (smoothPeak < 0.001f) {
        smoothPeak = 0.001f;
    }
    const float exposure = 1.0f / smoothPeak;
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            // Quadratic tone map: pushes the diffuse haze down so the advected
            // filaments and vortex cores read against a dark background. A
            // sqrt map leaves the whole frame mid-tone, which hides structure.
            const float n = fl::clamp(solver.density(x + 1, y + 1) * exposure, 0.0f, 1.0f);
            const float d = n * n;
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

    // Report the raw peak density once a second so a run can demonstrate that
    // density stays bounded (acceptance criterion 1) instead of relying on a
    // screenshot, which is tone-mapped and cannot show absolute values.
    static uint32_t lastReport = 0;
    if (now - lastReport >= 1000) {
        lastReport = now;
        Serial.print("fluid peak=");
        Serial.println(peak);
    }
}
