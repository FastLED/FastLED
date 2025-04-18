
// Parameterized xypaths that can be generated from an alpha value
// or LUT.

#include <math.h>

#include "fl/lut.h"
#include "fl/math_macros.h"
#include "fl/ptr.h"
#include "fl/warn.h"

namespace fl {

FASTLED_SMART_PTR(XYPath);
FASTLED_SMART_PTR(HeartPath);
FASTLED_SMART_PTR(LissajousPath);

class XYPath : public Referent {
  public:
    XYPath(uint16_t steps = 0); // 0 steps means no LUT.
    // α in [0,1] → (x,y) on the path, both in [0,1].
    virtual pair_xy<float> at(float alpha) = 0;
    virtual pair_xy<uint16_t> at16(uint16_t alpha, uint16_t scale = 0xffff);

  protected:
    // allow sub classes to access the LUT.
    uint32_t mSteps;
    LUTXY16Ptr mLut;

  private:
    void initLutOnce();
    LUTXY16Ptr generateLUT(uint16_t steps);
};

class HeartPath : public XYPath {
  public:
    HeartPath(uint16_t steps = 0);
    pair_xy<float> at(float alpha) override;
};

class LissajousPath : public XYPath {
  public:
    // Tweakable paramterized Lissajous path. Often used for led animations.
    // a, b are frequency ratios; delta is phase offset
    LissajousPath(uint8_t a = 3, uint8_t b = 2, float delta = PI / 2,
                  uint16_t steps = 0);
    pair_xy<float> at(float alpha) override;

  private:
    uint8_t mA, mB;
    float mDelta;
};

class ArchimedeanSpiralPath : public XYPath {
  public:
    /**
     * @param turns   Number of full revolutions around the center.
     * @param radius  Maximum radius (in normalized [0,1] units) from center.
     * @param steps   Number of LUT steps (0 = no LUT).
     */
    ArchimedeanSpiralPath(uint8_t turns = 3, float radius = 0.5f,
                          uint16_t steps = 0);

    pair_xy<float> at(float alpha) override;

  private:
    uint8_t mTurns;
    float mRadius;
};

} // namespace fl