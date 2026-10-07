// ok no header - public methods are declared in FastLED.h and channels/channel.h
#define FASTLED_INTERNAL
// IWYU pragma: private

#include "FastLED.h" // ok include: implements public CFastLED registration
#include "fl/channels/channel.h"
#include "fl/channels/channel_events.h"
#include "fl/log/log.h"

void CFastLED::add(fl::ChannelPtr channel) {
	if (!channel) {
		return;
	}
	auto& chnls = channels();
	// Protect against double-add
	if (chnls.has(channel)) {
		return;
	}
	chnls.push_back(channel);
	// Add channel to the CLEDController linked list
	// fl::Channel uses DeferRegister mode, so explicit addToDrawList() call is required
	// Note: addToDrawList() now fires onChannelAdded event
	channel->addToDrawList();
}

void CFastLED::remove(fl::ChannelPtr channel) {
	if (!channel) {
		return;
	}
	// Note: removeFromDrawList() now fires onChannelRemoved event
	channel->removeFromDrawList();
	// Remove from internal storage (safe if not found - erase is a no-op)
	channels().erase(channel);
}

namespace fl {

// Re-exposed protected base class methods
void Channel::addToDrawList() {
    if (isInList()) {
        FL_WARN("Channel '" << mName << "': Skipping addToDrawList() - already in draw list");
        return;
    }
    CPixelLEDController<RGB>::addToList();
    // Fire event after adding to draw list (detectable even if user bypasses FastLED.add())
    auto& events = ChannelEvents::instance();
    events.onChannelAdded(*this);
}

void Channel::removeFromDrawList() {
    if (!isInList()) {
        FL_WARN("Channel '" << mName << "': Skipping removeFromDrawList() - not in draw list");
        return;
    }
    CPixelLEDController<RGB>::removeFromDrawList();
    // Fire event after removing from draw list (detectable even if user bypasses FastLED.remove())
    auto& events = ChannelEvents::instance();
    events.onChannelRemoved(*this);

    // Clear driver weak_ptr when removed from draw list
    mDriver.reset();
}

} // namespace fl
