#pragma once

#include "namespace.h"
#include "eorder.h"
#include <stdint.h>

#include "active_strip_data.h"
#include "crgb.h"
#include "exports.h"
#include "strip_id_map.h"
#include "singleton.h"
#include "pixel_controller.h"
#include "pixel_iterator.h"
#include <vector>

FASTLED_NAMESPACE_BEGIN

#define FASTLED_HAS_CLOCKLESS 1


template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
public:
	virtual void init() { }
	ClocklessController() {
		mId = StripIdMap::getId(this);
	}

protected:
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mRgb.clear();
		ActiveStripData& ch_data = Singleton<ActiveStripData>::instance();
		pixels.disableColorAdjustment();
		PixelController<RGB> pixels_rgb = pixels;  // Converts to RGB pixels
		auto iterator = pixels_rgb.as_iterator(RgbwInvalid());
		while (iterator.has(1)) {
			uint8_t r, g, b;
			iterator.loadAndScaleRGB(&r, &g, &b);
			mRgb.push_back(r);
			mRgb.push_back(g);
			mRgb.push_back(b);
			iterator.advanceData();
		}
		const uint8_t* rgb = mRgb.data();
		ch_data.update(mId, millis(), rgb, mRgb.size());
	}
	private:
	  int mId = 0;
	  std::vector<uint8_t> mRgb;
};

FASTLED_NAMESPACE_END

