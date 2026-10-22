/// @file fl/fs/embedded/embedded_fs.cpp.hpp
/// @brief Body of `fl::getEmbeddedFs()`.
///
/// The public API says "embedded storage"; this file is where that maps
/// onto a concrete backend. It includes `platforms/embedded_fs.h`, which
/// selects exactly one platform fragment — LittleFS on ESP, a no-op
/// elsewhere — so the technology name never reaches `fl/`, and no platform
/// has to hold an opinion about a filesystem it does not have (#4007).
///
/// Everything lands in one translation unit on purpose. The entry point
/// and the backend it pulls in are compiled together into
/// `fl.fs.embedded+.cpp.o`, so a sketch that never calls
/// `getEmbeddedFs()` leaves the whole chain unreferenced and the linker
/// drops the object outright. Splitting the entry point from its backend
/// would defeat that: the backend would ride in on `platforms+.cpp.o`,
/// which every sketch links.

#include "fl/fs/embedded/embedded_fs.h"
#include "fl/stl/has_include.h"

// Selects the platform fragment. A plain header, not a `.cpp.hpp`, so it
// composes into this TU rather than needing its own build aggregate.
#include "platforms/embedded_fs.h"
#ifdef FL_IS_ESP32
#include "platforms/embedded_fs_provider.h"
#endif

namespace fl {

#if defined(FL_IS_ESP32) && FL_HAS_INCLUDE("esp_littlefs.h")
namespace platforms {
namespace detail {
EmbeddedMountRegistry& EmbeddedMountRegistry::instance() FL_NO_EXCEPT {
    // One POD registry in the isolated storage object, never per-header state.
    static EmbeddedMountRegistry registry = {nullptr, 0, false};
    return registry;
}
} // namespace detail
} // namespace platforms
#endif

#ifdef FL_IS_ESP32
namespace platforms {
FL_LINK_WEAK FsImplPtr fastled_embedded_fs_provider(bool formatOnFail) FL_NO_EXCEPT {
    return makeEmbeddedFs(formatOnFail);
}
} // namespace platforms
#endif

FsImplPtr getEmbeddedFs(bool format_on_fail) FL_NO_EXCEPT {
#ifdef FL_IS_ESP32
    return platforms::fastled_embedded_fs_provider(format_on_fail);
#else
    return platforms::makeEmbeddedFs(format_on_fail);
#endif
}

} // namespace fl
