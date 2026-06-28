#include "fl/ui/help.h"
#include "fl/stl/noexcept.h"

namespace fl {

UIHelp::UIHelp(const char *markdownContent) FL_NO_EXCEPT : mImpl(markdownContent) {}
UIHelp::~UIHelp() FL_NO_EXCEPT {}

} // namespace fl
