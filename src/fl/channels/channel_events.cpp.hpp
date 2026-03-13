#include "fl/channels/channel_events.h"
#include "fl/stl/singleton.h"

namespace fl {

ChannelEvents& ChannelEvents::instance() {
    return fl::Singleton<ChannelEvents>::instance();
}

}  // namespace fl
