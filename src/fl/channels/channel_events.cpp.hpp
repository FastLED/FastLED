#include "channel_events.h"
#include "fl/singleton.h"

namespace fl {

ChannelEvents& ChannelEvents::instance() {
    return fl::Singleton<ChannelEvents>::instance();
}

}  // namespace fl
