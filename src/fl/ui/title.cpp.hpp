#include "fl/ui/title.h"
#include "fl/stl/noexcept.h"

namespace fl {

#if FASTLED_USE_JSON_UI
UITitle::UITitle(const char *name) FL_NOEXCEPT : mImpl(fl::string(name), fl::string(name)) {}
#else
UITitle::UITitle(const char *name) FL_NOEXCEPT : mImpl(name) {}
#endif

UITitle::~UITitle() FL_NOEXCEPT {}

} // namespace fl
