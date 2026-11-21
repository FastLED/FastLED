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
/// Memory is (W+2)*(H+2)*4 bytes per field and the solver keeps 8 float
/// fields (a fixed-point tier is not implemented yet):
///
///   | Grid  | Solver state |
///   |-------|--------------|
///   | 32x32 | 36.1 KiB     |
///   | 48x32 | 53.1 KiB     |
///   | 48x48 | 78.1 KiB     |
///   | 64x64 | 136.1 KiB    |
///
/// The default is 64x64 on the web compiler, host builds and PSRAM boards,
/// and 48x32 everywhere else.

#include <FastLED.h>

#include "fl/math/xymap.h"
#include "fl/stl/vector.h"
#include "fl/ui/ui.h"

// -- Matrix config --
// Override the grid at compile time: -DFLUID_W=.. -DFLUID_H=.., or
// -DFLUID_GRID=N for an NxN square (e.g. -DFLUID_GRID=32 for 36.1 KiB).
#if defined(FLUID_GRID)
#define FLUID_DEFAULT_W FLUID_GRID
#define FLUID_DEFAULT_H FLUID_GRID
#elif defined(FL_IS_WASM) || defined(FL_IS_STUB) || defined(CONFIG_SPIRAM) || \
    defined(BOARD_HAS_PSRAM)
#define FLUID_DEFAULT_W 64  // 136.1 KiB of solver state
#define FLUID_DEFAULT_H 64
#else
#define FLUID_DEFAULT_W 48  // 53.1 KiB: fits internal RAM on ESP32-class parts
#define FLUID_DEFAULT_H 32
#endif
#ifndef FLUID_W
#define FLUID_W FLUID_DEFAULT_W
#endif
#ifndef FLUID_H
#define FLUID_H FLUID_DEFAULT_H
#endif
#define WIDTH FLUID_W
#define HEIGHT FLUID_H
#define NUM_LEDS (WIDTH * HEIGHT)
// Use the platform's clockless data pin rather than a hardcoded pin number, and
// keep the #ifndef guard so a build can override it with -DDATA_PIN=...
#ifndef DATA_PIN
#define DATA_PIN FL_PIN_CLOCKLESS_1
#endif
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
fl::UISlider dissipation("Density Dissipation", 0.995f, 0.90f, 1.0f, 0.001f);
fl::UISlider forceStrength("Force Strength", 12.0f, 0.5f, 40.0f, 0.1f);
fl::UISlider splatRadius("Splat Radius", 2.5f, 0.5f, 12.0f, 0.1f);
fl::UISlider jetSpeed("Jet Angle Speed", 0.35f, 0.0f, 1.0f, 0.01f);
fl::UISlider dyeEnergy("Dye Energy (per second)", 0.9f, 0.0f, 5.0f, 0.05f);
fl::UISlider iterations("Solver Iterations", 12, 4, 40, 1);
fl::UIGroup solverGroup("Solver", viscosity, diffusion, dissipation, iterations);
fl::UIGroup injectorGroup("Injectors", forceStrength, splatRadius, jetSpeed, dyeEnergy);
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

    const float t = static_cast<float>(now) * 0.001f;
    const float jet = jetSpeed.as<float>();

    // Advance the simulation over the FULL elapsed wall-clock time, in substeps
    // of at most 50 ms so each step stays stable. Clamping the frame time
    // instead of substepping silently discarded elapsed time on long frames
    // (below 20 fps), so injection and dissipation lagged the injector
    // positions — which are driven by wall clock — making the simulation
    // frame-rate dependent. The substep cap also bounds the worst case at four
    // solver steps per frame.
    const float kMaxStep = 0.05f;
    const int kMaxSubsteps = 4;
    float remaining = dt;
    for (int sub = 0; sub < kMaxSubsteps && remaining > 0.0f; ++sub) {
        const float h = (remaining > kMaxStep) ? kMaxStep : remaining;
        remaining -= h;

        // Three Gaussian splat injectors whose angles are noise-modulated.
        // Delta channel: force AND dye are transferred onto the fluid plane
        // every substep, as an h-scaled increment. Gating the dye to a slower
        // cadence made the plate read as a series of blobs rather than a
        // continuous energy transfer, and it broke frame-rate parity between
        // the two channels.
        const float dyeRate = dyeEnergy.as<float>(); // density units per second
        for (int k = 0; k < 3; ++k) {
            const float phase = t * jet + static_cast<float>(k) * 2.094f;
            const float angle = fl::sinf(phase) * 1.7f + fl::cosf(phase * 0.7f);
            const float cx = (0.5f + 0.35f * fl::sinf(phase * 0.83f)) *
                             static_cast<float>(WIDTH);
            const float cy = (0.5f + 0.35f * fl::cosf(phase * 0.61f)) *
                             static_cast<float>(HEIGHT);
            const float mag = forceStrength.as<float>();
            solver.addForce(h, cx, cy, fl::cosf(angle) * mag, fl::sinf(angle) * mag,
                            splatRadius.as<float>());
            // Same delta semantics as the force: energy per second, scaled by h.
            solver.addDensity(h, cx, cy, dyeRate, splatRadius.as<float>());
        }

        solver.step(h, viscosity.as<float>(), diffusion.as<float>(),
                    dissipation.as<float>(), iterations.as<int>());
    }

    // Render density to the matrix. This is a *delta* transfer curve, not a
    // per-frame normalisation: brightness is a fixed, monotonic function of the
    // energy actually present on the fluid plane, so a cell
    // that has received twice the energy reads brighter and dims as dissipation
    // drains it. Normalising by the frame's peak made every frame read 0..100%
    // and threw away the energy history.
    const int mode = colorMode.as_int();
    const float dBlack = 0.020f; // black level: ambient haze maps to 0
    const float dRange = 0.300f; // energy above black level that reaches full scale
    float peak = 0.0f;
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            const float d = solver.density(x + 1, y + 1);
            if (d > peak) {
                peak = d;
            }
        }
    }
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            const float d = solver.density(x + 1, y + 1);
            float v01 = (d > dBlack) ? (d - dBlack) / dRange : 0.0f;
            if (v01 > 1.0f) v01 = 1.0f;
            const uint8_t v = static_cast<uint8_t>(v01 * 255.0f);
            CRGB c;
            // Every mode must reach the documented black level at v01 == 0.
            // The coloured modes carry a non-zero base channel, so scale those
            // offsets by v01 as well — otherwise empty cells stay blue/indigo
            // instead of going dark, and a zero dyeEnergy frame would still
            // light the whole plate.
            if (mode == 0) {
                c = HeatColor(v);
            } else if (mode == 1) {
                c = CRGB(v, static_cast<uint8_t>(v / 3),
                         static_cast<uint8_t>((255 - v) * v01));
            } else {
                c = CRGB(static_cast<uint8_t>(v / 2), v,
                         static_cast<uint8_t>(v / 4 + 40.0f * v01));
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
