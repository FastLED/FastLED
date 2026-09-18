// mood_palette.cpp - mood to colour family.
#include "mood_palette.h"

#include "ring_canvas.h"

namespace mood_ring {

namespace {

// Hue centres on the FastLED rainbow wheel (0..255).
constexpr float kCoolHue = 150.0f; // teal-blue
constexpr float kWarmHue = 20.0f;  // amber-orange

constexpr float kMinSpread = 24.0f;
constexpr float kMaxSpread = 96.0f;
constexpr float kMinSat = 150.0f;
constexpr float kMaxSat = 255.0f;

} // namespace

MoodPalette paletteFor(const RoomSense &s, float hueDrift) {
    MoodPalette p;
    // Travel the short way round the wheel from cool to warm (through
    // magenta/red), which is the emotionally right path: sad -> tense ->
    // happy, not sad -> green -> happy.
    const float w = clampf(s.warmth, 0.0f, 1.0f);
    float hue = kCoolHue + (kWarmHue + 256.0f - kCoolHue) * w;
    hue += hueDrift * 256.0f;
    while (hue >= 256.0f)
        hue -= 256.0f;
    while (hue < 0.0f)
        hue += 256.0f;
    p.hueBase = static_cast<fl::u8>(hue);

    const float a = clampf(s.arousal, 0.0f, 1.0f);
    p.hueSpread =
        static_cast<fl::u8>(kMinSpread + (kMaxSpread - kMinSpread) * a);
    p.saturation = static_cast<fl::u8>(kMinSat + (kMaxSat - kMinSat) * a);
    return p;
}

RGBf paletteColor(const MoodPalette &p, float t) {
    // Hue is quantised to the 256-step wheel, but neighbouring hues differ
    // by about one unit per channel at full value, so a slow drift reads as
    // continuous. Everything after this point stays in float.
    const float tt = clampf(t, 0.0f, 1.0f);
    const fl::u8 hue = static_cast<fl::u8>(
        p.hueBase + static_cast<fl::u8>(tt * static_cast<float>(p.hueSpread)));
    return toRGBf(CRGB(CHSV(hue, p.saturation, 255)));
}

} // namespace mood_ring
