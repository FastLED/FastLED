#pragma once

// IWYU pragma: private

#include "fl/fs/backend.h"

namespace fl {
namespace platforms {

// The isolated ESP32 storage object supplies a weak native default. An
// explicitly linked application may provide this strong definition instead;
// the linker then discards the default and its native backend. Keep this
// declaration strong so an application's definition does not inherit weak.
// Returning null is a provider failure, never permission to mount another FS.
FsImplPtr fastled_embedded_fs_provider(bool formatOnFail) FL_NO_EXCEPT;

} // namespace platforms
} // namespace fl
