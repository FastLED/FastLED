#include "fl/ui/help.h"
#include "fl/stl/noexcept.h"

namespace fl {

UIHelp::UIHelp(const char *markdownContent) FL_NOEXCEPT : mImpl(markdownContent) {}
UIHelp::~UIHelp() FL_NOEXCEPT {}

} // namespace fl
