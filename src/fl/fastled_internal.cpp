#define FASTLED_INTERNAL
#include "fl/fastled.h"
#include "cled_controller.h"

namespace fl {
fl::u16 cled_contoller_size() {
	return sizeof(CLEDController);
}
}  // namespace fl
