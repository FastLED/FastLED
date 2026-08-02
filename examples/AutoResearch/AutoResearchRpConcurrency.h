#pragma once

#include "fl/stl/json.h"

namespace autoresearch {

/// Exercise the RP dual-core mutex and semaphore backends from both cores.
fl::json runRpConcurrencyTest();

} // namespace autoresearch
