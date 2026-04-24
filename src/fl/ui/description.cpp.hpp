#include "fl/ui/description.h"
#include "fl/stl/noexcept.h"

namespace fl {

UIDescription::UIDescription(const char *name) FL_NOEXCEPT : mImpl(name) {}
UIDescription::~UIDescription() FL_NOEXCEPT {}

} // namespace fl
