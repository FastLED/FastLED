// mood_palette.h - turn the room's mood into a colour family.
//
// Warmth (from valence) picks the hue centre: cool blues and teals for a
// sad or tense room, ambers and magentas for a happy one. Arousal picks how
// saturated and how wide the family is: a calm room is pastel and narrow,
// an excited one is vivid and spans more of the wheel.
#pragma once

#include "FastLED.h"
#include "fl/stl/stdint.h"

#include "ring_canvas.h"
#include "room_sense.h"

namespace mood_ring {

struct MoodPalette {
    fl::u8 hueBase = 160;  ///< centre of the family
    fl::u8 hueSpread = 40; ///< how far t=1 travels from t=0
    fl::u8 saturation = 200;
};

/// Derive the family for this frame. Cheap; call every frame.
MoodPalette paletteFor(const RoomSense &s, float hueDrift);

/// Colour at position t in [0, 1] along the family, at value v.
RGBf paletteColor(const MoodPalette &p, float t);

} // namespace mood_ring
