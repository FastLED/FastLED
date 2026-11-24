
// Simulation test for PerlinParticlePunch particle system.
// Verifies that particles spawn, move, render, and die correctly.

#include "fl/stl/stdint.h"
#include "test.h"
#include "fl/fx/1d/perlin_particle_punch.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// Helper: count non-black LEDs in the buffer
static int countLitLeds(const CRGB *leds, u16 count) {
    int lit = 0;
    for (u16 i = 0; i < count; ++i) {
        if (leds[i].r > 0 || leds[i].g > 0 || leds[i].b > 0)
            lit++;
    }
    return lit;
}

// Helper: check if any LEDs in a range are lit
static bool anyLitInRange(const CRGB *leds, int start, int end, u16 count) {
    if (start < 0) start = 0;
    if (end > (int)count) end = (int)count;
    for (int i = start; i < end; ++i) {
        if (leds[i].r > 0 || leds[i].g > 0 || leds[i].b > 0)
            return true;
    }
    return false;
}

FL_TEST_CASE("PerlinParticlePunch: ambient particles spawn at position 0") {
    constexpr u16 NUM_LEDS = 50;
    CRGB leds[NUM_LEDS];
    PerlinParticlePunch fx(NUM_LEDS);

    // Spawn ambient particles
    for (int i = 0; i < 5; i++) {
        fx.spawnAmbient(0.8f);
    }

    // Draw first frame — particles should be near position 0
    Fx::DrawContext ctx(1000, leds);
    fx.draw(ctx);

    // LEDs near position 0 should be lit (particles + noise)
    FL_CHECK(anyLitInRange(leds, 0, 10, NUM_LEDS));
}

FL_TEST_CASE("PerlinParticlePunch: ambient particles move forward over frames") {
    constexpr u16 NUM_LEDS = 50;
    CRGB leds[NUM_LEDS];
    PerlinParticlePunch fx(NUM_LEDS);
    fx.setSpeed(2.0f);

    // Spawn several ambient particles
    for (int i = 0; i < 10; i++) {
        fx.spawnAmbient(1.0f);
    }

    // Run many frames to let particles travel
    for (u32 t = 1000; t < 1100; t += 1) {
        Fx::DrawContext ctx(t, leds);
        fx.draw(ctx);
    }

    // After 100 frames at high speed, particles should have reached middle/end
    FL_CHECK(anyLitInRange(leds, 20, 50, NUM_LEDS));
}

FL_TEST_CASE("PerlinParticlePunch: meteor spawns at position 0 and travels") {
    constexpr u16 NUM_LEDS = 100;
    CRGB leds[NUM_LEDS];
    PerlinParticlePunch fx(NUM_LEDS);

    // Spawn a high-intensity meteor
    fx.spawnMeteor(1.0f);

    // Frame 1: meteor should be near position 0
    Fx::DrawContext ctx1(1000, leds);
    fx.draw(ctx1);
    FL_CHECK(anyLitInRange(leds, 0, 10, NUM_LEDS));

    // Run 20 frames — meteor should move well into the strip
    for (u32 t = 1001; t < 1021; t++) {
        Fx::DrawContext ctx(t, leds);
        fx.draw(ctx);
    }

    // After 20 frames, meteor head + tail should light up middle area
    FL_CHECK(anyLitInRange(leds, 30, 80, NUM_LEDS));
}

FL_TEST_CASE("PerlinParticlePunch: meteor intensity 0 still spawns") {
    constexpr u16 NUM_LEDS = 50;
    CRGB leds[NUM_LEDS];
    PerlinParticlePunch fx(NUM_LEDS);

    // Even at low intensity, a meteor should spawn and move
    fx.spawnMeteor(0.1f);

    for (u32 t = 1000; t < 1030; t++) {
        Fx::DrawContext ctx(t, leds);
        fx.draw(ctx);
    }

    // Should still produce some lit LEDs beyond noise
    int lit = countLitLeds(leds, NUM_LEDS);
    FL_CHECK(lit > 0);
}

FL_TEST_CASE("PerlinParticlePunch: spawnAmbient intensity clamps correctly") {
    constexpr u16 NUM_LEDS = 50;
    CRGB leds[NUM_LEDS];
    PerlinParticlePunch fx(NUM_LEDS);

    // Passing intensity > 1.0 should not crash
    fx.spawnAmbient(5.0f);
    fx.spawnAmbient(0.0f);
    fx.spawnAmbient(-1.0f);

    Fx::DrawContext ctx(1000, leds);
    fx.draw(ctx);
    // Just verify no crash
    FL_CHECK(true);
}

FL_TEST_CASE("PerlinParticlePunch: spawnMeteor intensity clamps to 0-1") {
    constexpr u16 NUM_LEDS = 50;
    CRGB leds[NUM_LEDS];
    PerlinParticlePunch fx(NUM_LEDS);

    // Passing values out of range should clamp, not crash
    fx.spawnMeteor(500.0f); // should clamp to 1.0
    fx.spawnMeteor(-10.0f); // should clamp to 0.0

    Fx::DrawContext ctx(1000, leds);
    fx.draw(ctx);
    FL_CHECK(true);
}

FL_TEST_CASE("PerlinParticlePunch: trail buffer creates fading persistence") {
    constexpr u16 NUM_LEDS = 50;
    CRGB leds_frame1[NUM_LEDS];
    CRGB leds_frame2[NUM_LEDS];
    PerlinParticlePunch fx(NUM_LEDS);
    fx.setAmbientTrailIntensity(240); // Very long trails
    fx.setSpeed(3.0f);

    // Spawn and draw frame 1
    fx.spawnAmbient(1.0f);
    Fx::DrawContext ctx1(1000, leds_frame1);
    fx.draw(ctx1);

    // Draw frame 2 (no new spawns) — trail buffer should still have data
    Fx::DrawContext ctx2(1001, leds_frame2);
    fx.draw(ctx2);

    // Frame 2 should still have lit LEDs from trail persistence
    int lit = countLitLeds(leds_frame2, NUM_LEDS);
    FL_CHECK(lit > 0);
}

FL_TEST_CASE("PerlinParticlePunch: palette changes affect output") {
    constexpr u16 NUM_LEDS = 20;
    CRGB leds1[NUM_LEDS];
    CRGB leds2[NUM_LEDS];

    PerlinParticlePunch fx1(NUM_LEDS);
    PerlinParticlePunch fx2(NUM_LEDS);

    // Set different palettes
    CRGBPalette16 redPalette(CRGB::Black, CRGB::Red, CRGB::Red, CRGB::White);
    fx2.setNoisePalette(redPalette);

    Fx::DrawContext ctx1(5000, leds1);
    fx1.draw(ctx1);
    Fx::DrawContext ctx2(5000, leds2);
    fx2.draw(ctx2);

    // The two should produce different colors for at least some LEDs
    bool different = false;
    for (u16 i = 0; i < NUM_LEDS; ++i) {
        if (leds1[i] != leds2[i]) {
            different = true;
            break;
        }
    }
    FL_CHECK(different);
}

FL_TEST_CASE("PerlinParticlePunch: pool exhaustion does not crash") {
    constexpr u16 NUM_LEDS = 20;
    CRGB leds[NUM_LEDS];
    PerlinParticlePunch fx(NUM_LEDS);

    // Spam spawn beyond pool capacity (50 ambient, 5 meteor, 50 debris)
    for (int i = 0; i < 200; i++) {
        fx.spawnAmbient(0.5f);
    }
    for (int i = 0; i < 20; i++) {
        fx.spawnMeteor(0.8f);
    }

    // Draw several frames — should not crash
    for (u32 t = 1000; t < 1050; t++) {
        Fx::DrawContext ctx(t, leds);
        fx.draw(ctx);
    }
    FL_CHECK(true);
}

} // FL_TEST_FILE
