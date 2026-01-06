#pragma once

#include "fl/fastled.h"
#include "fl/stl/vector.h"
#include "fl/fx/fx1d.h"

namespace fl {

FASTLED_SHARED_PTR(Particles1d);

/// @brief Power-based particle system for 1D LED strips creating organic light effects
///
/// ## Core Concept: Power-Based Lifecycle
/// Each particle has a power level that decreases linearly from 1.0 (birth) to 0.0 (death).
/// Power affects three properties simultaneously:
///   - Velocity: velocity = base_velocity × power (particle slows as it ages)
///   - Saturation: saturation = base_sat + (1-power) × (255-base_sat) (color intensifies)
///   - Brightness: brightness = base_brightness × power (dims toward death)
///
/// Visual Journey: Fast bright desaturated → Medium colorful → Crawling saturated ember
///
/// ## Technical Features
///
/// **Sub-Pixel Rendering:** Floating-point positions render across adjacent LEDs for smooth motion
///
/// **Overdraw Technique:** Multiple update/draw cycles per frame (default: 20x) create smooth
/// motion blur trails by repeatedly fading and redrawing particles
///
/// **Cyclical Wrapping:** Particles wrap seamlessly at strip boundaries, perfect for LED rings
///
/// ## Usage Example
/// @code
/// fl::Particles1d particles(NUM_LEDS, 10, 2);  // 10 particles, fade rate 2
/// particles.setLifetime(5000);  // 5 second lifespan
/// particles.setSpeed(1.5f);     // 1.5x speed multiplier
///
/// void loop() {
///     if (millis() - lastSpawn >= 2000) {
///         particles.spawnRandomParticle();
///         lastSpawn = millis();
///     }
///     particles.draw(fl::Fx::DrawContext(millis(), leds));
/// }
/// @endcode
///
/// ## Performance (ESP32 @ 240MHz, 210 LEDs)
/// - Memory: ~60 bytes base + ~20 bytes per particle
/// - CPU: ~15ms per frame with 20x overdraw (suitable for 30+ FPS)
///
/// @note Spawning is externally controlled for flexibility (triggers, music-reactive, etc.)
class Particles1d : public Fx1d {
  public:
    /// @param num_leds Number of LEDs in the strip
    /// @param max_particles Maximum number of simultaneous particles (default: 10)
    /// @param fade_rate Fade amount per frame for trails (0-255, default: 2)
    Particles1d(u16 num_leds, u8 max_particles = 10, u8 fade_rate = 2);

    ~Particles1d();

    /// @brief Update and render all particles with overdraw technique
    /// @param context Draw context containing current time and LED buffer
    void draw(DrawContext context) override;

    /// @brief Spawn a particle with random position, velocity, color, and lifetime
    ///
    /// Spawning behavior:
    /// - First tries to find an inactive particle slot
    /// - If all slots are full, replaces the oldest particle by birthTime
    ///
    /// @note Call this from your loop() based on desired spawn rate (e.g., every 2 seconds)
    /// @note Spawn control is intentionally external for flexibility (music-reactive, triggers, etc.)
    void spawnRandomParticle();

    /// @brief Set average particle lifetime in milliseconds
    /// @param lifetime_ms Lifetime in milliseconds (default: 4000)
    void setLifetime(u16 lifetime_ms);

    /// @brief Set overdraw count (higher = smoother trails, more CPU)
    /// @param count Number of update/draw cycles per frame (default: 20)
    void setOverdrawCount(u8 count);

    /// @brief Set speed multiplier (1.0 = normal, >1.0 = faster, <1.0 = slower)
    /// @param speed Speed multiplier applied to all particles (default: 1.0)
    void setSpeed(float speed);

    /// @brief Set fade rate for trails (0-255, higher = shorter trails)
    /// @param fade_rate Fade amount per frame (default: 2)
    void setFadeRate(u8 fade_rate);

    /// @brief Set cyclical mode (true = wrap around, false = stop at edges)
    /// @param cyclical Wrap mode - perfect for LED rings (default: true)
    void setCyclical(bool cyclical);

    fl::string fxName() const override;

  private:
    /// @brief Individual particle with power-based lifecycle
    ///
    /// Power level (1.0 → 0.0) controls velocity, saturation, and brightness.
    /// Uses floating-point position for sub-pixel rendering smoothness.
    struct Particle {
        float pos;          ///< Position (floating point for sub-pixel rendering)
        float baseVel;      ///< Base velocity (actual velocity = baseVel × power)
        CHSV baseColor;     ///< Base color (HSV) - saturation increases with age
        u32 birthTime;      ///< Spawn timestamp (ms)
        u32 lifetime;       ///< Lifespan in milliseconds
        bool active;        ///< Active flag (false = available for reuse)

        Particle();

        /// @return Power level from 1.0 (birth) to 0.0 (death)
        float getPower(u32 now) const;

        /// @brief Spawn with random position, velocity, color, and lifetime
        void spawn(u16 numLeds);

        /// @brief Spawn with specific parameters
        /// @param pos Initial position
        /// @param baseVel Base velocity (positive = forward, negative = backward)
        /// @param baseColor Initial HSV color
        /// @param lifetime Lifespan in milliseconds
        void spawn(float pos, float baseVel, CHSV baseColor, u32 lifetime);

        /// @brief Update position based on velocity × power
        void update(u32 now, u16 numLeds, float speedMultiplier, bool cyclical);

        /// @brief Render particle with sub-pixel accuracy and power-modulated color
        void draw(CRGB* leds, u32 now, u16 numLeds);
    };

    u8 mFadeRate;                    ///< Fade amount per frame (0-255, higher = shorter trails)
    u16 mLifetimeMs = 4000;          ///< Average particle lifetime in milliseconds
    u8 mOverdrawCount;               ///< Number of update/draw cycles per frame (higher = smoother trails, more CPU)
    float mSpeedMultiplier;          ///< Global speed multiplier (1.0 = normal, >1.0 = faster, <1.0 = slower)
    bool mCyclical;                  ///< Wrap mode: true = wrap around, false = stop at edges
    fl::vector<Particle> mParticles; ///< Particle pool (oldest particle reused when full)
};

} // namespace fl
