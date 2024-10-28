#pragma once
#include "fs.h"
#include "namespace.h"
#include "ptr.h"
#include "slice.h"
#include <stdint.h>

FASTLED_NAMESPACE_BEGIN

FsImplPtr make_filesystem(int cs_pin);

FASTLED_NAMESPACE_END

extern "C" {
// Called from the browser side, this is intended to create a file
// at the given path with the given data.
void jsInjectFile(const char *path, const uint8_t *data, size_t len);
}
