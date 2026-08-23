/// @file fl/system/embedded_fs/embedded_fs.cpp.hpp
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
/// `fl.system.embedded_fs+.cpp.o`, so a sketch that never calls
/// `getEmbeddedFs()` leaves the whole chain unreferenced and the linker
/// drops the object outright. Splitting the entry point from its backend
/// would defeat that: the backend would ride in on `platforms+.cpp.o`,
/// which every sketch links.

#include "fl/system/embedded_fs/embedded_fs.h"

// Selects the platform fragment. A plain header, not a `.cpp.hpp`, so it
// composes into this TU rather than needing its own build aggregate.
#include "platforms/embedded_fs.h"

namespace fl {

FsImplPtr getEmbeddedFs(bool format_on_fail) FL_NO_EXCEPT {
    return fl::platforms::makeEmbeddedFs(format_on_fail);
}

} // namespace fl
