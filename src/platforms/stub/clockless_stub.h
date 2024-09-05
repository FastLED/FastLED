#ifndef __INC_CLOCKLESS_STUB_H
#define __INC_CLOCKLESS_STUB_H

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_STUB_IMPL)

#define FASTLED_HAS_CLOCKLESS 1

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
public:
	virtual void init() { }

protected:
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) { }
};

#endif // defined(FASTLED_STUB_IMPL)

FASTLED_NAMESPACE_END

#endif // __INC_CLOCKLESS_STUB_H