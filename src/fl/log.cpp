#include "fl/log.h"

namespace fl {

// Static storage for runtime logging state - default all disabled
fl::u8 LogState::sEnabledCategories = 0x00;

} // namespace fl
