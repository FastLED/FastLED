#pragma once

#include "ptr.h"
#include "fx/storage/fs.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

FsImplPtr make_filesystem(int cs_pin);

FASTLED_NAMESPACE_END
