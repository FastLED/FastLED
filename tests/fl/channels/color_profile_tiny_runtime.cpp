#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/channel_events.h"

namespace {
constexpr fl::EmitterProfile kTinyProfile = fl::EmitterProfile::rgb(
    "tiny/static", fl::Chromaticity(.640f, .330f), fl::Chromaticity(.300f, .600f),
    fl::Chromaticity(.150f, .060f), 1.0f, 1.0f, 1.0f);
}

int main() {
    CRGB leds[1] = {};
    fl::ChannelConfig config("tiny-static", fl::ClocklessChipset(), leds, RGB);
    fl::ChannelPtr channel = fl::Channel::create<kTinyProfile>(config);
    if (channel == nullptr || channel->emitterProfile() != &kTinyProfile) return 1;
    const fl::CLEDController& base = *channel;
    if (base.emitterProfile() != &kTinyProfile || channel->name() != "tiny-static") return 2;
    // TINY retains compile-time profile identity without an instance pointer,
    // but legacy correction remains mutually exclusive: it must clear the
    // static binding and never resurrect it on a later legacy update.
    channel->setCorrection(CRGB(255, 128, 64));
    if (channel->emitterProfile() != nullptr) return 4;
    channel->setCorrection(UncorrectedColor);
    if (channel->emitterProfile() != nullptr) return 6;

    // TINY supports only the compile-time Channel::create<Profile>() path.
    // Runtime profile, source, gamut, and target-white requests must report
    // that they were not admitted rather than allocating hidden controller
    // state or replacing the immutable static identity.
    fl::ChannelOptions runtime_options;
    if (runtime_options.setColorProfile(kTinyProfile, fl::SourceProfile::linearSrgb(),
                                        fl::GamutPolicy::Clamp) ||
        runtime_options.setTargetWhite(fl::Chromaticity(.3127f, .3290f)) ||
        runtime_options.hasColorProfile()) return 5;

    bool created = false;
    const int listener = fl::ChannelEvents::instance().onChannelCreated.add(
        [&](const fl::IChannel&) { created = true; });
    fl::ChannelConfig ordinary_config("tiny-ordinary", fl::ClocklessChipset(), leds, RGB);
    fl::ChannelPtr ordinary = fl::Channel::create(ordinary_config);
    fl::ChannelEvents::instance().onChannelCreated.remove(listener);
    if (ordinary == nullptr || ordinary->name() != "tiny-ordinary" || !created) return 3;

    // The legacy CFastLED factory must not silently discard its profile
    // template argument on TINY; it needs the same zero-state static identity
    // as Channel::create<Profile>().
    CLEDController& legacy = FastLED.addLeds<fl::ProfileId::WS2812B, WS2812, 1, GRB>(leds, 1);
    if (legacy.emitterProfile() != &fl::profiles::WS2812B) return 7;
    return 0;
}
