#pragma once
#include <stdint.h>
#include "ptr.h"
#include "fs.h"
#include "slice.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

FsImplPtr make_filesystem(int cs_pin);

// Called from the browser side, this is intended to create a file
// at the given path with the given data.
void jsInjectFile(const char* path, const uint8_t* data, size_t len);

FASTLED_NAMESPACE_END
