/*  FastLED - Rhythmic Particle System
    ---------------------------------------------------------
    Audio-reactive particle system for music visualization,
    optimized for Tipper-style broken beat EDM.

    Features:
    - Structure-of-Arrays (SoA) layout for cache efficiency
    - Multiple emitter types (kick, snare, hat, overlay)
    - Physics: radial gravity, curl noise flow field, kick ducking
    - Zero heap allocations in render loop

    License: MIT (same spirit as FastLED)
*/

#pragma once

#include "FastLED.h"

#if SKETCH_HAS_LOTS_OF_MEMORY

#include "fl/stdint.h"
#include "fl/function.h"
#include "crgb.h"

namespace fl {

// ---------- Configuration Structures ----------

/// @brief Configuration for the rhythmic particle system
struct RhythmParticlesConfig {
    int max_particles = 1000;            ///< Maximum particle count
    float dt = 1.0f / 120.0f;            ///< Simulation timestep (120 FPS default)
    float velocity_decay = 0.985f;       ///< Velocity damping per frame
    float radial_gravity = 0.0f;         ///< Radial pull to center (negative = repulsion)
    float curl_strength = 0.7f;          ///< Flow field intensity
    float kick_duck_amount = 0.35f;      ///< Brightness duck on kick (0-1)
    float kick_duck_duration_ms = 80.0f; ///< Duck duration
    uint8_t bloom_threshold = 64;        ///< Bloom activation threshold (0-255)
    float bloom_strength = 0.5f;         ///< Bloom intensity
    int width = 32;                      ///< Logical canvas width
    int height = 8;                      ///< Logical canvas height
    bool enable_3d = false;              ///< Enable 3D particle movement
};

/// @brief Configuration for particle emitter
struct ParticleEmitterConfig {
    float emit_rate = 10.0f;             ///< Particles per event
    float velocity_min = 0.5f;           ///< Min initial velocity
    float velocity_max = 2.0f;           ///< Max initial velocity
    float life_min = 0.5f;               ///< Min lifetime (seconds)
    float life_max = 2.0f;               ///< Max lifetime (seconds)
    CRGB color_base = CRGB::White;       ///< Base color
    uint8_t hue_variance = 30;           ///< Hue randomization ±
    float spread_angle = 360.0f;         ///< Emission cone angle (degrees)
    float x = 0.5f;                      ///< Emitter X position (normalized 0-1)
    float y = 0.5f;                      ///< Emitter Y position (normalized 0-1)
    float z = 0.0f;                      ///< Emitter Z position (normalized 0-1)
};

/// @brief Emitter type identifiers
enum class EmitterType : uint8_t {
    Kick = 0,       ///< Kick drum emitter (bass onsets)
    Snare,          ///< Snare/glitch emitter (mid onsets)
    HiHat,          ///< Hi-hat spray emitter (high onsets)
    Overlay,        ///< Polymetric overlay emitter (7/8 accents)
    Custom          ///< User-defined emitter
};

// ---------- Rhythmic Particle System ----------

/// @brief Audio-reactive particle system with SoA layout
/// @details Optimized for real-time music visualization on embedded platforms
class RhythmParticles {
public:
    /// @brief Construct rhythmic particle system
    /// @param cfg Configuration parameters
    explicit RhythmParticles(const RhythmParticlesConfig& cfg);

    /// @brief Destructor
    ~RhythmParticles();

    // ---- Event Handlers (connect to BeatDetector callbacks) ----

    /// @brief Handle beat event
    /// @param phase4_4 Phase in 4/4 meter (0.0-1.0)
    /// @param phase7_8 Phase in overlay meter (0.0-1.0)
    void onBeat(float phase4_4, float phase7_8);

    /// @brief Handle subdivision event
    /// @param subdivision Subdivision type (16th, triplet, etc.)
    /// @param swing_offset Swing offset value
    void onSubdivision(int subdivision, float swing_offset);

    /// @brief Handle bass onset
    /// @param confidence Onset confidence (0.0-1.0+)
    /// @param timestamp_ms Timestamp in milliseconds
    void onOnsetBass(float confidence, float timestamp_ms);

    /// @brief Handle mid onset
    /// @param confidence Onset confidence (0.0-1.0+)
    /// @param timestamp_ms Timestamp in milliseconds
    void onOnsetMid(float confidence, float timestamp_ms);

    /// @brief Handle high onset
    /// @param confidence Onset confidence (0.0-1.0+)
    /// @param timestamp_ms Timestamp in milliseconds
    void onOnsetHigh(float confidence, float timestamp_ms);

    /// @brief Handle fill event
    /// @param starting True if fill is starting, false if ending
    /// @param density Fill density (0.0-1.0)
    void onFill(bool starting, float density);

    // ---- Simulation ----

    /// @brief Advance physics simulation
    /// @param dt Delta time in seconds (optional, uses config dt if not specified)
    void update(float dt = 0.0f);

    /// @brief Render particles to LED buffer
    /// @param leds LED buffer
    /// @param num_leds Number of LEDs
    void render(CRGB* leds, int num_leds);

    // ---- Configuration ----

    /// @brief Set emitter configuration
    /// @param type Emitter type
    /// @param cfg Emitter configuration
    void setEmitterConfig(EmitterType type, const ParticleEmitterConfig& cfg);

    /// @brief Get emitter configuration
    /// @param type Emitter type
    /// @return Emitter configuration
    const ParticleEmitterConfig& getEmitterConfig(EmitterType type) const;

    /// @brief Set color palette
    /// @param palette Color palette for particles
    void setPalette(const CRGBPalette16& palette);

    /// @brief Get current configuration
    const RhythmParticlesConfig& config() const { return _cfg; }

    /// @brief Update configuration
    void setConfig(const RhythmParticlesConfig& cfg);

    // ---- Stats ----

    /// @brief Get number of active particles
    /// @return Active particle count
    int getActiveParticleCount() const { return _particleCount; }

    /// @brief Get maximum particle capacity
    /// @return Maximum particles
    int getMaxParticles() const { return _maxParticles; }

    /// @brief Reset particle system (clear all particles)
    void reset();

private:
    RhythmParticlesConfig _cfg;

    // Structure-of-Arrays (SoA) particle storage for cache efficiency
    int _maxParticles;
    int _particleCount;

    float* _x;      ///< X positions
    float* _y;      ///< Y positions
    float* _z;      ///< Z positions
    float* _vx;     ///< X velocities
    float* _vy;     ///< Y velocities
    float* _vz;     ///< Z velocities
    uint8_t* _h;    ///< Hue
    uint8_t* _s;    ///< Saturation
    uint8_t* _v;    ///< Value/brightness
    float* _life;   ///< Remaining lifetime (seconds)
    float* _maxLife; ///< Initial lifetime (seconds)

    // Emitter configurations
    ParticleEmitterConfig _emitterKick;
    ParticleEmitterConfig _emitterSnare;
    ParticleEmitterConfig _emitterHiHat;
    ParticleEmitterConfig _emitterOverlay;

    // Color palette
    CRGBPalette16 _palette;

    // Kick ducking state
    float _kickDuckTime;
    float _kickDuckLevel;

    // Fill state
    bool _inFill;
    float _fillDensity;

    // Noise field state
    uint32_t _noiseTime;

    // Internal methods
    void allocateParticles(int max_particles);
    void deallocateParticles();
    void emitParticles(const ParticleEmitterConfig& emitter, int count, float energy = 1.0f);
    void applyForces(float dt);
    void updateLifetime(float dt);
    void cullDead();
    void applyKickDuck(float dt);
    float getCurlNoiseX(float x, float y, float z) const;
    float getCurlNoiseY(float x, float y, float z) const;
    float getCurlNoiseZ(float x, float y, float z) const;
};

} // namespace fl

#endif // SKETCH_HAS_LOTS_OF_MEMORY
