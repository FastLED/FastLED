/// @file wave.h
/// @brief 2D wave simulation effect for LED matrices
///
/// Provides realistic water-like wave effects that propagate across LED grids.
/// Supports multiple wave layers, gradient coloring, and various simulation parameters.

#pragma once

#include "fl/stl/stdint.h"

#include "fl/colorutils.h"
#include "fl/gradient.h"
#include "fl/stl/shared_ptr.h"  // For FASTLED_SHARED_PTR macros and shared_ptr
#include "fl/wave_simulation.h"
#include "fl/xymap.h"
#include "fl/fx/fx.h"
#include "fl/fx/fx2d.h"

namespace fl {

FASTLED_SHARED_PTR(WaveFx);
FASTLED_SHARED_PTR(WaveCrgbMap);
FASTLED_SHARED_PTR(WaveCrgbMapDefault);
FASTLED_SHARED_PTR(WaveCrgbGradientMap);

/// @brief Abstract base class for mapping wave simulation values to LED colors
///
/// Defines the interface for converting wave amplitude values from WaveSimulation2D
/// into CRGB color values for LED display.
class WaveCrgbMap {
  public:
    virtual ~WaveCrgbMap() = default;

    /// @brief Convert wave simulation values to LED colors
    /// @param xymap Coordinate mapping from 2D grid to 1D LED array
    /// @param waveSim Wave simulation containing amplitude values
    /// @param leds Output LED array to write colors to
    virtual void mapWaveToLEDs(const XYMap &xymap, WaveSimulation2D &waveSim,
                               CRGB *leds) = 0;
};

/// @brief Default wave-to-color mapper producing grayscale output
///
/// Maps wave amplitudes to grayscale colors where:
/// - Zero amplitude = black (0,0,0)
/// - Maximum amplitude = white (255,255,255)
/// This creates a simple but effective visualization of wave heights.
class WaveCrgbMapDefault : public WaveCrgbMap {
  public:
    /// @brief Map wave values to grayscale LED colors
    /// @param xymap Coordinate mapping from 2D grid to 1D LED array
    /// @param waveSim Wave simulation containing amplitude values
    /// @param leds Output LED array to write colors to
    void mapWaveToLEDs(const XYMap &xymap, WaveSimulation2D &waveSim,
                       CRGB *leds) override {
        const fl::u32 width = waveSim.getWidth();
        const fl::u32 height = waveSim.getHeight();
        for (fl::u32 y = 0; y < height; y++) {
            for (fl::u32 x = 0; x < width; x++) {
                fl::u32 idx = xymap(x, y);
                uint8_t value8 = waveSim.getu8(x, y);
                leds[idx] = CRGB(value8, value8, value8);
            }
        }
    }
};

/// @brief Wave-to-color mapper using gradient/palette coloring
///
/// Maps wave amplitudes to colors using a gradient palette, allowing for
/// colorful wave effects (e.g., blue water, fire-like waves, etc.).
/// Uses batch processing internally for improved performance.
class WaveCrgbGradientMap : public WaveCrgbMap {
  public:
    using Gradient = fl::GradientInlined;

    /// @brief Construct with a color palette
    /// @param palette 16-color palette defining the gradient
    WaveCrgbGradientMap(const CRGBPalette16 &palette) : mGradient(palette) {}

    /// @brief Default constructor with no initial gradient
    WaveCrgbGradientMap() = default;

    /// @brief Map wave values to gradient-colored LEDs
    /// @param xymap Coordinate mapping from 2D grid to 1D LED array
    /// @param waveSim Wave simulation containing amplitude values
    /// @param leds Output LED array to write colors to
    void mapWaveToLEDs(const XYMap &xymap, WaveSimulation2D &waveSim,
                       CRGB *leds) override;

    /// @brief Set or update the color gradient
    /// @param gradient New gradient to use for color mapping
    void setGradient(const Gradient &gradient) { mGradient = gradient; }

  private:
    Gradient mGradient;
};

/// @brief Configuration parameters for WaveFx effect
///
/// Defines all the settings that control wave behavior including
/// simulation quality, physics parameters, and color mapping.
struct WaveFxArgs {
    WaveFxArgs() = default;

    /// @brief Construct with all parameters
    /// @param factor Supersampling quality (higher = smoother but slower)
    /// @param half_duplex If true, waves only go positive (no negative values)
    /// @param auto_updates If true, automatically advance simulation each frame
    /// @param speed Wave propagation speed (0.0 to 1.0, typical: 0.1-0.3)
    /// @param dampening Energy loss rate (higher = waves die faster, typical: 3-10)
    /// @param crgbMap Custom color mapper (nullptr uses default grayscale)
    WaveFxArgs(SuperSample factor, bool half_duplex, bool auto_updates,
               float speed, float dampening, WaveCrgbMapPtr crgbMap)
        : factor(factor), half_duplex(half_duplex), auto_updates(auto_updates),
          speed(speed), dampening(dampening), crgbMap(crgbMap) {}
    WaveFxArgs(const WaveFxArgs &) = default;
    WaveFxArgs &operator=(const WaveFxArgs &) = default;

    /// Supersampling quality (SUPER_SAMPLE_2X recommended for balance)
    SuperSample factor = SuperSample::SUPER_SAMPLE_2X;
    /// If true, constrains waves to positive values only
    bool half_duplex = true;
    /// If true, simulation advances automatically in draw()
    bool auto_updates = true;
    /// Wave propagation speed (0.0-1.0, typical: 0.1-0.3)
    float speed = 0.16f;
    /// Energy dampening factor (higher = faster decay, typical: 3-10)
    float dampening = 6.0f;
    /// If true, waves wrap around the x-axis (cylindrical topology)
    bool x_cyclical = false;
    /// Use change grid tracking for optimization (may reduce visual quality)
    bool use_change_grid = false;
    /// Custom color mapper (nullptr uses default grayscale)
    WaveCrgbMapPtr crgbMap;
};

/// @brief 2D wave simulation effect with supersampling and gradient coloring
///
/// Creates realistic water-like wave effects on LED matrices. Features include:
/// - Physics-based wave propagation using WaveSimulation2D
/// - Supersampling for smooth, high-quality rendering
/// - Configurable speed, dampening, and other physics parameters
/// - Support for gradient/palette coloring or grayscale
/// - Optional cylindrical wrapping on x-axis
/// - Half-duplex mode for positive-only waves
///
/// Example usage:
/// @code
/// XYMap xymap(16, 16);
/// WaveFx::Args args;
/// args.speed = 0.18f;
/// args.dampening = 9.0f;
/// args.crgbMap = fl::make_shared<WaveCrgbGradientMap>(myPalette);
/// WaveFx wave(xymap, args);
///
/// // Trigger a wave at position (8, 8)
/// wave.setf(8, 8, 1.0f);
///
/// // In draw loop:
/// Fx::DrawContext ctx(millis(), leds);
/// wave.draw(ctx);
/// @endcode
///
/// @see WaveSimulation2D for the underlying physics engine
/// @see examples/FxWave2d/FxWave2d.ino for a complete demonstration
class WaveFx : public Fx2d {
  public:
    using Args = WaveFxArgs;

    /// @brief Construct wave effect with given coordinate mapping and parameters
    /// @param xymap Coordinate mapping from 2D grid to 1D LED array
    /// @param args Configuration parameters (uses defaults if not specified)
    WaveFx(const XYMap& xymap, Args args = Args())
        : Fx2d(xymap), mWaveSim(xymap.getWidth(), xymap.getHeight(),
                                args.factor, args.speed, args.dampening) {
        // Initialize the wave simulation with the given parameters.
        if (args.crgbMap == nullptr) {
            // Use the default CRGB mapping function.
            mCrgbMap = fl::make_shared<WaveCrgbMapDefault>();
        } else {
            // Set a custom CRGB mapping function.
            mCrgbMap = args.crgbMap;
        }
        setAutoUpdate(args.auto_updates);
        setXCylindrical(args.x_cyclical);
        setUseChangeGrid(args.use_change_grid);
    }

    /// @brief Enable/disable cylindrical topology on x-axis
    /// @param on If true, waves wrap around from right edge to left edge
    ///
    /// When enabled, creates a seamless horizontal loop effect.
    /// Useful for LED strips arranged in a cylinder or torus.
    void setXCylindrical(bool on) { mWaveSim.setXCylindrical(on); }

    /// @brief Set wave propagation speed
    /// @param speed Speed value (0.0 to 1.0, typical range: 0.1-0.3)
    ///
    /// Higher values make waves propagate faster across the grid.
    /// Typical values: 0.12-0.26 for realistic water effects.
    void setSpeed(float speed) {
        // Set the speed of the wave simulation.
        mWaveSim.setSpeed(speed);
    }

    /// @brief Set wave energy dampening
    /// @param dampening Dampening factor (typical range: 3.0-10.0)
    ///
    /// Higher values cause waves to lose energy and dissipate faster.
    /// Lower values create longer-lasting waves.
    /// Typical values: 3.0 for slow decay, 9.0 for faster decay.
    void setDampening(float dampening) {
        // Set the dampening of the wave simulation.
        mWaveSim.setDampening(dampening);
    }

    /// @brief Enable/disable half-duplex mode
    /// @param on If true, constrains waves to positive values only
    ///
    /// Half-duplex mode prevents negative wave values, creating
    /// effects that look more like ripples on water surface.
    /// When disabled, waves can have negative values for more
    /// complex interference patterns.
    void setHalfDuplex(bool on) {
        // Set whether the wave simulation is half duplex.
        mWaveSim.setHalfDuplex(on);
    }

    /// @brief Set supersampling quality level
    /// @param factor Quality level (SUPER_SAMPLE_NONE, _2X, _4X, or _8X)
    ///
    /// Supersampling improves visual quality by simulating at higher
    /// resolution than the LED grid. Higher values = smoother but slower.
    /// Recommended: SUPER_SAMPLE_2X for good balance.
    void setSuperSample(SuperSample factor) {
        // Set the supersampling factor of the wave simulation.
        mWaveSim.setSuperSample(factor);
    }

    /// @brief Set easing function for wave amplitude calculation
    /// @param mode Easing mode (LINEAR or SQRT)
    ///
    /// - LINEAR: Direct mapping of wave values (sharper, more digital)
    /// - SQRT: Square root mapping (softer, more natural-looking waves)
    void setEasingMode(U8EasingFunction mode) {
        // Set the easing mode for the 8-bit value.
        mWaveSim.setEasingMode(mode);
    }

    /// @brief Enable/disable change grid tracking optimization
    /// @param enabled If true, uses change tracking to optimize updates
    ///
    /// Change grid tracking preserves set points over multiple iterations
    /// for more stable results, but may reduce dramatic visual effects.
    /// Disabling saves memory but may cause more oscillation.
    void setUseChangeGrid(bool enabled) {
        // Set whether to use the change grid tracking optimization.
        mWaveSim.setUseChangeGrid(enabled);
    }

    /// @brief Get current change grid tracking setting
    /// @return True if change grid tracking is enabled
    bool getUseChangeGrid() const {
        // Get the current change grid tracking setting.
        return mWaveSim.getUseChangeGrid();
    }

    /// @brief Set wave amplitude at a specific grid position
    /// @param x X coordinate in grid space
    /// @param y Y coordinate in grid space
    /// @param value Wave amplitude (0.0 to 1.0)
    ///
    /// Use this to create a wave disturbance at a specific point.
    /// Value of 1.0 creates maximum amplitude wave that will propagate outward.
    void setf(size_t x, size_t y, float value) {
        // Set the value at the given coordinates in the wave simulation.
        mWaveSim.setf(x, y, value);
    }

    /// @brief Add wave amplitude to existing value at a position
    /// @param x X coordinate in grid space
    /// @param y Y coordinate in grid space
    /// @param value Wave amplitude to add (clamped to max 1.0)
    ///
    /// Adds to existing wave amplitude, useful for creating multiple
    /// overlapping disturbances. Result is clamped to maximum of 1.0.
    void addf(size_t x, size_t y, float value) {
        // Add a value at the given coordinates in the wave simulation.
        float sum = value + mWaveSim.getf(x, y);
        mWaveSim.setf(x, y, FL_MIN(1.0f, sum));
    }

    /// @brief Get wave amplitude as 8-bit value
    /// @param x X coordinate in grid space
    /// @param y Y coordinate in grid space
    /// @return Wave amplitude as 0-255 value
    ///
    /// Returns the current wave amplitude at the given position,
    /// scaled and mapped to 0-255 range.
    uint8_t getu8(size_t x, size_t y) const {
        // Get the 8-bit value at the given coordinates in the wave simulation.
        return mWaveSim.getu8(x, y);
    }

    /// @brief Set custom color mapping function
    /// @param crgbMap Shared pointer to color mapper
    ///
    /// Replaces the current color mapper with a new one.
    /// Use WaveCrgbMapDefault for grayscale or WaveCrgbGradientMap
    /// for palette-based coloring.
    void setCrgbMap(WaveCrgbMapPtr crgbMap) {
        // Set a custom CRGB mapping function.
        mCrgbMap = crgbMap;
    }

    /// @brief Render wave to LED array
    /// @param context Draw context containing timestamp and LED buffer
    ///
    /// Updates the wave simulation (if auto-update enabled) and
    /// maps wave values to LED colors using the current color mapper.
    void draw(DrawContext context) override {
        // Update the wave simulation.
        if (mAutoUpdates) {
            mWaveSim.update();
        }
        // Map the wave values to the LEDs.
        mCrgbMap->mapWaveToLEDs(mXyMap, mWaveSim, context.leds);
    }

    /// @brief Enable/disable automatic simulation updates
    /// @param autoUpdate If true, simulation advances each draw() call
    ///
    /// When enabled, draw() automatically advances the wave simulation.
    /// Disable this if you want manual control via update() calls.
    void setAutoUpdate(bool autoUpdate) {
        // Set whether to automatically update the wave simulation.
        mAutoUpdates = autoUpdate;
    }

    /// @brief Manually advance wave simulation by one step
    ///
    /// Advances the wave physics simulation by one timestep.
    /// Only needed if auto-update is disabled or you want extra
    /// simulation steps between frames.
    void update() {
        // Called automatically in draw. Only invoke this if you want extra
        // simulation updates.
        // Update the wave simulation.
        mWaveSim.update();
    }

    /// @brief Get effect name
    /// @return "WaveFx"
    fl::string fxName() const override { return "WaveFx"; }

    WaveSimulation2D mWaveSim;
    WaveCrgbMapPtr mCrgbMap;
    bool mAutoUpdates = true;
};

} // namespace fl
