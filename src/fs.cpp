#include "fs.h"

FASTLED_NAMESPACE_BEGIN

// Override this if you want to supply a file system for your platform.
__attribute__((weak)) FsImplPtr make_filesystem(int cs_pin) {
    return FsImplPtr::Null();
}

FASTLED_NAMESPACE_END
