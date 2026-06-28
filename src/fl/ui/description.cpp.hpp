#include "fl/ui/description.h"
#include "fl/stl/noexcept.h"

namespace fl {

UIDescription::UIDescription(const char *name) FL_NO_EXCEPT : mImpl(name) {}
UIDescription::~UIDescription() FL_NO_EXCEPT {}

} // namespace fl
