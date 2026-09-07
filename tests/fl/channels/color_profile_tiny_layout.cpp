// Compile-only P2 tier layout probe. This target intentionally links no
// host-tier FastLED library or shared test PCH.

#include "FastLED.h"
#include "fl/channels/color_profile.h"
#include "fl/stl/static_assert.h"

namespace {
// Frozen pre-P2 layout, independently repeated here so a new nested layout
// type cannot make the comparison tautological.
struct LegacyChannelOptionsLayout {
    CRGB correction = UncorrectedColor;
    CRGB temperature = UncorrectedTemperature;
    fl::u8 ditherMode = BINARY_DITHER;
    fl::variant<fl::Empty, fl::Rgbw, fl::Rgbww> whiteCfg;
    fl::Bus bus = fl::Bus::AUTO;
    fl::u8 busWhich = 0;
    fl::optional<float> gamma;
};

constexpr fl::EmitterProfile kTinyStaticProfile = fl::EmitterProfile::rgb(
    "tiny/static", fl::Chromaticity(.640f, .330f),
    fl::Chromaticity(.300f, .600f), fl::Chromaticity(.150f, .060f),
    1.0f, 1.0f, 1.0f);
}  // namespace

FL_STATIC_ASSERT(FL_PLATFORM_HAS_TINY_MEMORY == 1,
              "tiny layout probe must compile with the tiny-memory tier");
FL_STATIC_ASSERT(sizeof(fl::ChannelOptions) == sizeof(LegacyChannelOptionsLayout),
              "TINY ChannelOptions must not gain P2 runtime state");
FL_STATIC_ASSERT(sizeof(fl::StaticProfileChannel<kTinyStaticProfile>) == sizeof(fl::Channel),
              "TINY static profile identity must add no Channel instance state");

int main() { return 0; }
