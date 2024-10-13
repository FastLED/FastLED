#pragma once

#include "namespace.h"
#include "eorder.h"
#include <stdint.h>

#include "channel_data.h"
#include "crgb.h"
#include "exports.h"

#include "singleton.h"

FASTLED_NAMESPACE_BEGIN

#define FASTLED_HAS_CLOCKLESS 1

class InstanceCounter {
public:
	static InstanceCounter& getInstance();

	uint32_t increment() {
		return mCount++;
	}
	private:
		uint32_t mCount = 0;
};

inline InstanceCounter& InstanceCounter::getInstance() {
	InstanceCounter& out = Singleton<InstanceCounter>::instance();
	return out;
}

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
public:
	virtual void init() { }
	ClocklessController() {
		mId = InstanceCounter::getInstance().increment();
	}

protected:
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		ChannelData& ch_data = Singleton<ChannelData>::instance();
		const uint8_t* rgb = pixels.mData;
		int nLeds = pixels.mLen;
		//ch_data.update(mId, millis(), SliceUint8(rgb, nLeds * 3));
		ch_data.update(mId, millis(), rgb, nLeds * 3);
	}
	private:
	  int mId = 0;
};

FASTLED_NAMESPACE_END

