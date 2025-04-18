
// Parameterized xypaths that can be generated from an alpha value
// or LUT.

#include <math.h>

#include "fl/lut.h"
#include "fl/ptr.h"
#include "fl/warn.h"

namespace fl {

FASTLED_SMART_PTR(XYPath);
FASTLED_SMART_PTR(HeartPath);

class XYPath : public Referent {
  public:
    XYPath(uint16_t steps = 0);  // 0 steps means no LUT.
    // α in [0,1] → (x,y) on the path, both in [0,1].
    virtual pair_xy<float> at(float alpha) = 0;
    virtual pair_xy<uint16_t> at16(uint16_t alpha, uint16_t scale = 0xffff);

  private:
    LUTXY16Ptr generateLUT(uint16_t steps);

    void initLutOnce();
    uint32_t mSteps;
    LUTXY16Ptr mLut;
};

class HeartPath : public XYPath {
  public:
    HeartPath(uint16_t steps = 0): XYPath(steps) {}
    pair_xy<float> at(float alpha) override;
};

} // namespace fl