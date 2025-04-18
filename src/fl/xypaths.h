
// This is a drawing/graphics related class.
//
// XYPath represents a parameterized (x,y) path. The input will always be
// an alpha value between 0->1 (float) or 0->0xffff (uint16_t).
// A look up table can be used to optimize path calculations when steps > 0.
//
// We provide common paths discovered throughout human history, for use in
// your animations.

#include "fl/lut.h"
#include "fl/math_macros.h"
#include "fl/ptr.h"
#include "fl/vector.h"
#include "fl/warn.h"

namespace fl {

// Smart pointers for the XYPath family.
FASTLED_SMART_PTR(XYPath);
FASTLED_SMART_PTR(TransformPath);
FASTLED_SMART_PTR(LinePath);
FASTLED_SMART_PTR(CirclePath);
FASTLED_SMART_PTR(HeartPath);
FASTLED_SMART_PTR(LissajousPath);
FASTLED_SMART_PTR(ArchimedeanSpiralPath);
FASTLED_SMART_PTR(RosePath);
FASTLED_SMART_PTR(PhyllotaxisPath);
FASTLED_SMART_PTR(GielisCurvePath);
FASTLED_SMART_PTR(CatmullRomPath);

struct TransformFloat {
    TransformFloat() = default;
    float scale = 1.0f;
    float x_offset = 0.0f;
    float y_offset = 0.0f;
    float rotation = 0.0f;
    pair_xy<float> transform(const pair_xy<float> &xy) const;
};

struct Transform16 {
    Transform16() = default;
    uint16_t scale = 0xffff;
    uint16_t x_offset = 0;
    uint16_t y_offset = 0;
    uint16_t rotation = 0;

    pair_xy<uint16_t> transform(const pair_xy<uint16_t> &xy) const;
};

class XYPath : public Referent {
  public:
    XYPath(uint16_t steps = 0); // 0 steps means no LUT.
    // α in [0,1] → (x,y) on the path, both in [0,1].
    virtual pair_xy<float> at(float alpha) = 0;
    pair_xy<float> at(float alpha, const TransformFloat &tx);

    // α in [0,65535] → (x,y) on the path, both in [0,65535].
    // This default implementation will build a LUT if mSteps > 0.
    // Subclasses may override this to avoid the LUT.
    virtual pair_xy<uint16_t> at16(uint16_t alpha,
                                   const Transform16 &tx = Transform16());

    // optimizes at16(...).
    void buildLut(uint16_t steps);

    // Called by subclasses when something changes. The LUT will be rebuilt on
    // the next call to at16(...) if mSteps > 0.
    void clearLut();

    // Clears lut and sets new steps. LUT will be rebuilt on next call to
    // at16(...) if mSteps > 0.
    void clearLut(uint16_t steps) {
        mSteps = steps;
        mLut.reset();
    }

    void output(float alpha_start, float alpha_end, pair_xy<float> *out,
                uint16_t out_size, const TransformFloat &tx = TransformFloat());

  protected:
    uint32_t mSteps;
    LUTXY16Ptr mLut;

  private:
    void initLutOnce();
    LUTXY16Ptr generateLUT(uint16_t steps);
};

// TransformPath is a wrapper for XYPath that applies a transform. This is for
// people that are lazy and don't want to pass around a transform.
class TransformPath : public XYPath {
  public:
    using Params = TransformFloat;
    TransformPath(XYPathPtr path, const Params &params = Params());
    pair_xy<float> at(float alpha) override;

    void setPath(XYPathPtr path);
    XYPathPtr getPath() const;
    Params &params();

  private:
    XYPathPtr mPath;
    Params mParams;
};

class LinePath : public XYPath {
  public:
    LinePath(float x0, float y0, float x1, float y1, uint16_t steps = 0);
    pair_xy<float> at(float alpha) override;
    void set(float x0, float y0, float x1, float y1);

  private:
    float mX0, mY0, mX1, mY1;
};

/// Catmull–Rom spline through arbitrary points.
/// Simply add control points and at(α) will smoothly interpolate through them.
class CatmullRomPath : public XYPath {
  public:
    /**
     * @param steps  LUT resolution (0 = no LUT)
     */
    CatmullRomPath(uint16_t steps = 0);

    /// Add a point in [0,1]² to the path
    void addPoint(pair_xy<float> p);

    pair_xy<float> at(float alpha) override;

  private:
    HeapVector<pair_xy<float>> mPoints;
};

class CirclePath : public XYPath {
  public:
    CirclePath(uint16_t steps = 0);
    pair_xy<float> at(float alpha) override;

  private:
    float mRadius;
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

class RosePath : public XYPath {
  public:
    /**
     * @param petals  Number of petals (integer k)
     * @param steps   LUT steps (0 = no LUT)
     */
    RosePath(uint8_t petals = 5, uint16_t steps = 0);
    pair_xy<float> at(float alpha) override;

  private:
    uint8_t mPetals;
};

/// “Superformula” (Gielis curve), can turn into many shapes.
/// r(θ) = [ |cos(m·θ/4)/a|ⁿ² + |sin(m·θ/4)/b|ⁿ³ ]^(–1/n¹)
class GielisCurvePath : public XYPath {
  public:
    /**
     * @param m      Symmetry count (repetitions)
     * @param a,b    Shape control (usually 1.0)
     * @param n1,n2,n3 Exponents shaping the curve
     * @param steps  LUT resolution (0 = no LUT)
     */
    GielisCurvePath(uint8_t m = 6, float a = 1.0f, float b = 1.0f,
                    float n1 = 1.0f, float n2 = 1.0f, float n3 = 1.0f,
                    uint16_t steps = 0);

    pair_xy<float> at(float alpha) override;

  private:
    uint8_t mM;
    float mA, mB, mN1, mN2, mN3;
};

/// “Phyllotaxis” / Sunflower spiral:
///  n = α·(count−1),
///  θ = n·goldenAngle,
///  r = √(n/(count−1))
/// then (x,y) = (0.5+0.5·r·cosθ, 0.5+0.5·r·sinθ)
class PhyllotaxisPath : public XYPath {
  public:
    /**
     * @param count  Number of seeds (controls density)
     * @param angle  Angular increment in radians (default ≈137.508°)
     * @param steps  LUT resolution (0 = no LUT)
     */
    PhyllotaxisPath(uint16_t count = 500,
                    float angle = 137.508f * (PI / 180.0f), uint16_t steps = 0);

    pair_xy<float> at(float alpha) override;

  private:
    uint16_t mCount;
    float mAngle;
};

} // namespace fl