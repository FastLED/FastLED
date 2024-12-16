#pragma once

#include "fl/namespace.h"
#include "eorder.h"
#include "fl/unused.h"

FASTLED_NAMESPACE_BEGIN

#define FASTLED_HAS_CLOCKLESS 1

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
public:
	virtual void init() { }

protected:
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		FASTLED_UNUSED(pixels);
	}
};

FASTLED_NAMESPACE_END

