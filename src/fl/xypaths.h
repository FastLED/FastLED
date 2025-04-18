
#include <math.h>

#include "fl/lut.h"

namespace fl {

class HeartPath {
  public:
    HeartPath() = default;
    // α in [0,1] → (x,y) on the heart, both in [0,1].
    pair_xy_float at(float alpha) const;
};

} // namespace fl