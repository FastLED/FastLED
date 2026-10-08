// Lint-only public STL inventory for #4773. Header-only entry points may be
// absent from canonical implementation routers; keep production unity intact.
#include "platforms/new.h"
#include "fl/system/arduino.h"
#include "FastLED.h"

#include "fl/stl/cerrno.h"
#include "fl/stl/format.h"
#include "fl/stl/range_access.h"
#include "fl/stl/detail/file_io.h"
