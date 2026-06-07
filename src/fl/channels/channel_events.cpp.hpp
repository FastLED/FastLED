#include "fl/channels/channel_events.h"
#include "fl/stl/singleton.h"

namespace fl {

// Out-of-line `instance()` only exists in the full event-firing build.
// Under FASTLED_DISABLE_CHANNEL_EVENTS the header-defined inline
// `instance()` body fires (returns a static no-op object), and the
// `fl::Singleton<ChannelEvents>` template is not instantiated — letting
// `--gc-sections` drop both that template's machinery and the singleton
// itself. See #2931 / #2886.
#if !defined(FASTLED_DISABLE_CHANNEL_EVENTS) || !FASTLED_DISABLE_CHANNEL_EVENTS
ChannelEvents& ChannelEvents::instance() {
    return fl::Singleton<ChannelEvents>::instance();
}
#endif

}  // namespace fl
