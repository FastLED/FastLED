// Lint-only public FX inventory for #4773. Production unity routers include
// implementations, which do not necessarily include header-only effects.
// Keep Animartrix through its public entry point so private header order is valid.
#include "platforms/new.h"
#include "fl/system/arduino.h"
#include "FastLED.h"

#include "fl/fx/1d/cylon.h"
#include "fl/fx/1d/demoreel100.h"
#include "fl/fx/1d/fire2012.h"
#include "fl/fx/1d/noisewave.h"
#include "fl/fx/1d/pacifica.h"
#include "fl/fx/1d/particles.h"
#include "fl/fx/1d/perlin_particle_punch.h"
#include "fl/fx/1d/pride2015.h"
#include "fl/fx/1d/twinklefox.h"
#include "fl/fx/2d/animartrix.hpp"
#include "fl/fx/2d/blend.h"
#include "fl/fx/2d/flowfield.h"
#include "fl/fx/2d/luminova.h"
#include "fl/fx/2d/noisepalette.h"
#include "fl/fx/2d/redsquare.h"
#include "fl/fx/2d/scale_up.h"
#include "fl/fx/2d/wave.h"
