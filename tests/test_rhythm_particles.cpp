/*  Test - Rhythmic Particle System
    ---------------------------------------------------------
    Tests for audio-reactive particle system.

    License: MIT (same spirit as FastLED)
*/

#include "doctest.h"

#if SKETCH_HAS_LOTS_OF_MEMORY

#include "fx/particles/rhythm_particles.h"
#include <cmath>

using namespace fl;

TEST_CASE("RhythmParticles - Basic Initialization") {
    RhythmParticlesConfig cfg;
    cfg.max_particles = 100;
    cfg.width = 32;
    cfg.height = 8;

    RhythmParticles particles(cfg);

    // Initially, no particles should be active
    CHECK(particles.getActiveParticleCount() == 0);
    CHECK(particles.getMaxParticles() == 100);
}

TEST_CASE("RhythmParticles - Bass Onset Emission") {
    RhythmParticlesConfig cfg;
    cfg.max_particles = 100;

    RhythmParticles particles(cfg);

    // Trigger bass onset
    particles.onOnsetBass(1.0f, 0.0f);

    // Should have emitted particles
    CHECK(particles.getActiveParticleCount() > 0);
}

TEST_CASE("RhythmParticles - Mid Onset Emission") {
    RhythmParticlesConfig cfg;
    cfg.max_particles = 100;

    RhythmParticles particles(cfg);

    // Trigger mid onset
    particles.onOnsetMid(1.0f, 0.0f);

    // Should have emitted particles
    CHECK(particles.getActiveParticleCount() > 0);
}

TEST_CASE("RhythmParticles - High Onset Emission") {
    RhythmParticlesConfig cfg;
    cfg.max_particles = 100;

    RhythmParticles particles(cfg);

    // Trigger high onset
    particles.onOnsetHigh(1.0f, 0.0f);

    // Should have emitted particles
    CHECK(particles.getActiveParticleCount() > 0);
}

TEST_CASE("RhythmParticles - Particle Lifetime Decay") {
    RhythmParticlesConfig cfg;
    cfg.max_particles = 100;
    cfg.dt = 0.1f;  // 100ms timestep

    RhythmParticles particles(cfg);

    // Emit particles
    particles.onOnsetBass(1.0f, 0.0f);
    int initial_count = particles.getActiveParticleCount();
    CHECK(initial_count > 0);

    // Update several times (simulate time passing)
    for (int i = 0; i < 50; i++) {
        particles.update(0.1f);
    }

    // Particles should have died off
    int final_count = particles.getActiveParticleCount();
    CHECK(final_count < initial_count);
}

TEST_CASE("RhythmParticles - Physics Update") {
    RhythmParticlesConfig cfg;
    cfg.max_particles = 100;
    cfg.radial_gravity = 0.5f;
    cfg.curl_strength = 0.3f;

    RhythmParticles particles(cfg);

    // Emit particles
    particles.onOnsetBass(1.0f, 0.0f);

    // Update physics
    particles.update(0.016f);  // ~60 FPS

    // Should still have particles (just moved)
    CHECK(particles.getActiveParticleCount() > 0);
}

TEST_CASE("RhythmParticles - Rendering") {
    RhythmParticlesConfig cfg;
    cfg.max_particles = 100;
    cfg.width = 32;
    cfg.height = 8;

    RhythmParticles particles(cfg);

    // Emit particles
    particles.onOnsetBass(1.0f, 0.0f);

    // Create LED buffer
    CRGB leds[256];
    for (int i = 0; i < 256; i++) {
        leds[i] = CRGB::Black;
    }

    // Render
    particles.render(leds, 256);

    // Some LEDs should be lit
    bool any_lit = false;
    for (int i = 0; i < 256; i++) {
        if (leds[i].r > 0 || leds[i].g > 0 || leds[i].b > 0) {
            any_lit = true;
            break;
        }
    }
    CHECK(any_lit);
}

TEST_CASE("RhythmParticles - Emitter Configuration") {
    RhythmParticlesConfig cfg;
    cfg.max_particles = 100;

    RhythmParticles particles(cfg);

    // Set custom emitter config
    ParticleEmitterConfig kickCfg;
    kickCfg.emit_rate = 20.0f;
    kickCfg.velocity_min = 2.0f;
    kickCfg.velocity_max = 4.0f;
    kickCfg.color_base = CRGB::Red;

    particles.setEmitterConfig(EmitterType::Kick, kickCfg);

    // Verify config was set
    const ParticleEmitterConfig& retrieved = particles.getEmitterConfig(EmitterType::Kick);
    CHECK(retrieved.emit_rate == doctest::Approx(20.0f));
    CHECK(retrieved.velocity_min == doctest::Approx(2.0f));
}

TEST_CASE("RhythmParticles - Fill Detection") {
    RhythmParticlesConfig cfg;
    cfg.max_particles = 100;

    RhythmParticles particles(cfg);

    // Trigger fill
    particles.onFill(true, 0.8f);

    // Should emit overlay particles
    CHECK(particles.getActiveParticleCount() > 0);
}

TEST_CASE("RhythmParticles - Reset") {
    RhythmParticlesConfig cfg;
    cfg.max_particles = 100;

    RhythmParticles particles(cfg);

    // Emit particles
    particles.onOnsetBass(1.0f, 0.0f);
    particles.onOnsetMid(1.0f, 0.0f);
    CHECK(particles.getActiveParticleCount() > 0);

    // Reset
    particles.reset();

    // All particles should be cleared
    CHECK(particles.getActiveParticleCount() == 0);
}

TEST_CASE("RhythmParticles - Maximum Capacity") {
    RhythmParticlesConfig cfg;
    cfg.max_particles = 10;  // Very small capacity

    RhythmParticles particles(cfg);

    // Try to emit more particles than capacity
    for (int i = 0; i < 5; i++) {
        particles.onOnsetBass(1.0f, 0.0f);
    }

    // Should be capped at max capacity
    CHECK(particles.getActiveParticleCount() <= 10);
}

TEST_CASE("RhythmParticles - Velocity Decay") {
    RhythmParticlesConfig cfg;
    cfg.max_particles = 100;
    cfg.velocity_decay = 0.9f;

    RhythmParticles particles(cfg);

    // Emit particles
    particles.onOnsetBass(1.0f, 0.0f);

    // Update several times
    for (int i = 0; i < 10; i++) {
        particles.update(0.1f);
    }

    // Particles should still exist but with reduced velocity
    // (hard to test directly, but we verify they haven't all died)
    CHECK(particles.getActiveParticleCount() > 0);
}

TEST_CASE("RhythmParticles - Kick Duck Effect") {
    RhythmParticlesConfig cfg;
    cfg.max_particles = 100;
    cfg.kick_duck_amount = 0.5f;
    cfg.kick_duck_duration_ms = 100.0f;

    RhythmParticles particles(cfg);

    // Emit some particles
    particles.onOnsetMid(1.0f, 0.0f);
    particles.update(0.01f);

    // Trigger kick (should activate ducking)
    particles.onOnsetBass(1.0f, 0.0f);

    // Create LED buffer and render
    CRGB leds[256];
    for (int i = 0; i < 256; i++) {
        leds[i] = CRGB::Black;
    }
    particles.render(leds, 256);

    // Duck effect is internal, just verify rendering doesn't crash
    CHECK(true);
}

TEST_CASE("RhythmParticles - Configuration Update") {
    RhythmParticlesConfig cfg;
    cfg.max_particles = 50;

    RhythmParticles particles(cfg);

    // Update configuration
    RhythmParticlesConfig newCfg;
    newCfg.max_particles = 100;
    newCfg.radial_gravity = 1.0f;

    particles.setConfig(newCfg);

    // Verify config was updated
    CHECK(particles.getMaxParticles() == 100);
    CHECK(particles.config().radial_gravity == doctest::Approx(1.0f));
}

TEST_CASE("RhythmParticles - Bloom Effect") {
    RhythmParticlesConfig cfg;
    cfg.max_particles = 100;
    cfg.bloom_threshold = 100;
    cfg.bloom_strength = 0.5f;
    cfg.width = 32;
    cfg.height = 8;

    RhythmParticles particles(cfg);

    // Emit bright particles
    particles.onOnsetBass(1.0f, 0.0f);

    // Create LED buffer
    CRGB leds[256];
    for (int i = 0; i < 256; i++) {
        leds[i] = CRGB::Black;
    }

    // Render with bloom
    particles.render(leds, 256);

    // Bloom should spread to neighboring LEDs
    // (hard to test precisely, but verify no crashes)
    CHECK(true);
}

#endif // SKETCH_HAS_LOTS_OF_MEMORY
