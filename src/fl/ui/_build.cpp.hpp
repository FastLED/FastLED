/// @file _build.hpp
/// @brief Unity build header for fl/ui/ directory
///
/// TODO(refactor/fl-ui-folder): This directory is a work-in-progress split of
/// the monolithic fl/ui.h + fl/ui.cpp.hpp into per-element headers and impls.
/// The includes below are intentionally gated off so that this unity build
/// file satisfies the unity-build linter (file exists, cpp.hpp siblings are
/// referenced, parent _build.cpp.hpp can include it) without producing
/// duplicate symbols that collide with the still-active fl/ui.cpp.hpp.
/// Remove the guard (and delete the legacy fl/ui.cpp.hpp / fl/ui.h content)
/// once the refactor is completed.

#ifdef FASTLED_ENABLE_UI_FOLDER_REFACTOR
#include "fl/ui/audio.cpp.hpp"
#include "fl/ui/button.cpp.hpp"
#include "fl/ui/checkbox.cpp.hpp"
#include "fl/ui/description.cpp.hpp"
#include "fl/ui/element.cpp.hpp"
#include "fl/ui/help.cpp.hpp"
#include "fl/ui/number_field.cpp.hpp"
#include "fl/ui/slider.cpp.hpp"
#include "fl/ui/title.cpp.hpp"
#endif
