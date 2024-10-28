
#include "ptr.h"
#include "fx/storage/fs.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

// Platforms eed to implement this to create an instance of the filesystem.
FsImplPtr make_filesystem(int cs_pin) {
    return FsImplPtr::Null();
}

FASTLED_NAMESPACE_END
