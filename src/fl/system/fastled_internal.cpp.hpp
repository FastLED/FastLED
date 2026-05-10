// ok no header
#define FASTLED_INTERNAL
#include "fl/system/fastled.h"
#include "cled_controller.h"
#include "fl/stl/noexcept.h"

namespace fl {
fl::u16 cled_contoller_size() FL_NOEXCEPT {
	return sizeof(CLEDController);
}
}  // namespace fl
