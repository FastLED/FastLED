// ok no header - public declarations remain in FastLED.h

#define FASTLED_INTERNAL
// IWYU pragma: private

#include "FastLED.h" // ok include: implements existing public CFastLED methods
#include "fl/stl/noexcept.h"
#include "fl/channels/manager.h"

void CFastLED::setDriverEnabled(const char* name, bool enabled) FL_NO_EXCEPT {
	fl::ChannelManager& manager = fl::channelManager();
	manager.setDriverEnabled(name, enabled);
}

bool CFastLED::setExclusiveDriver(fl::Bus bus, fl::u8 which) FL_NO_EXCEPT {
	fl::ChannelManager& manager = fl::channelManager();
	return manager.setExclusiveDriver(bus, which);
}

fl::size CFastLED::getDriverCount() const FL_NO_EXCEPT {
	fl::ChannelManager& manager = fl::channelManager();
	return manager.getDriverCount();
}

fl::span<const fl::DriverInfo> CFastLED::getDriverInfos() const FL_NO_EXCEPT {
	fl::ChannelManager& manager = fl::channelManager();
	return manager.getDriverInfos();
}
