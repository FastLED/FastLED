#pragma once

#include "namespace.h"
#include "eorder.h"

#include "channel_data.h"
#include "crgb.h"
#include "exports.h"

#include "singleton.h"

FASTLED_NAMESPACE_BEGIN

#define FASTLED_HAS_CLOCKLESS 1

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
public:
	virtual void init() { }

protected:
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		ChannelData& ch_data = Singleton<ChannelData>::instance();
		const uint8_t* rgb = pixels.mData;
		int nLeds = pixels.mLen;
		StripData stripData = {0, SliceUint8(rgb, nLeds * 3)};
		Slice<StripData> dataSlice(&stripData, 1);
		ch_data.update(dataSlice);
		jsOnFrame();
	}
};

FASTLED_NAMESPACE_END

