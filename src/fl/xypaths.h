
#include <math.h>

#include "fl/lut.h"
#include "fl/ptr.h"

namespace fl {

FASTLED_SMART_PTR(XYPath);
FASTLED_SMART_PTR(HeartPath);


class XYPath: public Referent {
  public:
    XYPath() = default;
    // α in [0,1] → (x,y) on the path, both in [0,1].
    virtual pair_xy<float> at(float alpha) = 0;
};

class HeartPath: public XYPath {
  public:
    HeartPath() = default;
    // α in [0,1] → (x,y) on the heart, both in [0,1].
    pair_xy<float> at(float alpha) override;
};

} // namespace fl