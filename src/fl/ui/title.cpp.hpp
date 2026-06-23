#include "fl/ui/title.h"
#include "fl/stl/noexcept.h"

namespace fl {

#if FASTLED_USE_JSON_UI
// JsonTitleImpl takes (name, text) where text defaults to name for the simple
// const-char* UITitle ctor. Both arguments are const-ref and are copied into
// the internal JsonUiTitleInternal, so we pass two short-lived fl::string
// temporaries built from the same C-string source.
UITitle::UITitle(const char *name) FL_NOEXCEPT : mImpl(fl::string(name), fl::string(name)) {}
#else
UITitle::UITitle(const char *name) FL_NOEXCEPT : mImpl(name) {}
#endif

UITitle::~UITitle() FL_NOEXCEPT {}

} // namespace fl
