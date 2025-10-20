#pragma once

#include "eorder.h"
#include "fl/unused.h"
#include "fl/chipsets/timing_traits.h"
namespace fl {
#define FASTLED_HAS_CLOCKLESS 1

// Template for new-style API accepting ChipsetTiming reference
// This is the primary template
template <int DATA_PIN, const ChipsetTiming& TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
public:
	virtual void init() { }

protected:
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		FASTLED_UNUSED(pixels);
	}
};

// Adapter struct that accepts timing-like objects via duck typing
// TIMING_LIKE can be ChipsetTiming& or any type that quacks like a timing object
// Preserves all platform-specific parameters: XTRA0, FLIP, WAIT_TIME
template <int DATA_PIN, typename TIMING_LIKE, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
struct ClocklessControllerAdapter : public CPixelLEDController<RGB_ORDER> {
	virtual void init() { }

protected:
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		FASTLED_UNUSED(pixels);
	}
};
}  // namespace fl