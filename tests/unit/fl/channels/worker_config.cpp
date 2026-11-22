#include "test.h"
#include "FastLED.h"
#include "fl/channels/worker_config.h"
#include "fl/stl/optional.h"
#include "fl/stl/static_assert.h"

namespace {
// Freeze the old layout independently: named scheduling policies must not
// introduce another per-channel field or retain an optional's presence tag.
struct LegacyWorkerConfigLayout {
    fl::Core core;
    fl::optional<fl::u8> priority;
};
struct LegacyChannelOptionsLayout {
    CRGB correction;
    CRGB temperature;
    fl::u8 dither;
    fl::Bus bus;
    fl::variant<fl::Empty, fl::Rgbw, fl::Rgbww> white;
    fl::optional<float> gamma;
#if FL_COLOR_PROFILE_RUNTIME
    fl::ColorProfileBinding profile;
#endif
    fl::u8 busWhich;
    LegacyWorkerConfigLayout worker;
};
FL_STATIC_ASSERT(fl::WorkerConfig().isDefault(),
                 "Default worker settings must be foldable without runtime code");
}

FL_TEST_CASE("Named worker settings fit in two bytes without growing old configuration") {
    FL_CHECK_LE(sizeof(fl::WorkerConfig), sizeof(LegacyWorkerConfigLayout));
    FL_CHECK_LE(sizeof(fl::WorkerConfig), 2u);
    FL_CHECK_LE(sizeof(fl::ChannelOptions), sizeof(LegacyChannelOptionsLayout));
    FL_CHECK_TRUE(fl::WorkerConfig().isDefault());
    FL_CHECK_EQ(sizeof(fl::Priority), 1u);
    FL_CHECK(fl::WorkerConfig().priority == fl::Priority::Auto);
    fl::WorkerConfig config;
    config.priority = fl::Priority::High;
    FL_CHECK_FALSE(config.isDefault());
}
