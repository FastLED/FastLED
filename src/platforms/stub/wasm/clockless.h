#pragma once

#include "namespace.h"
#include "eorder.h"
#include <stdint.h>

#include "active_strip_data.h"
#include "crgb.h"
#include "exports.h"

#include "singleton.h"

FASTLED_NAMESPACE_BEGIN

#define FASTLED_HAS_CLOCKLESS 1

class ClocklessInstanceCounter {
public:
	static ClocklessInstanceCounter& getInstance();

	uint32_t increment() {
		return mCount++;
	}
	private:
		uint32_t mCount = 0;
};

inline ClocklessInstanceCounter& ClocklessInstanceCounter::getInstance() {
	ClocklessInstanceCounter& out = Singleton<ClocklessInstanceCounter>::instance();
	return out;
}

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
public:
	virtual void init() { }
	ClocklessController() {
		mId = ClocklessInstanceCounter::getInstance().increment();
	}

protected:
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		ActiveStripData& ch_data = Singleton<ActiveStripData>::instance();
		const uint8_t* rgb = pixels.mData;
		int nLeds = pixels.mLen;
		ch_data.update(mId, millis(), rgb, nLeds * 3);
	}
	private:
	  int mId = 0;
};

FASTLED_NAMESPACE_END

