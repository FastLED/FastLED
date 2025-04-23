
#pragma once

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
#include "fl/tile2x2.h"
#include "fl/transform.h"
#include "fl/unused.h"
#include "fl/vector.h"
#include "fl/warn.h"

namespace fl {

class XYRasterSparse;
template <typename T> class Function;

// Smart pointers for the XYPath family.
FASTLED_SMART_PTR(XYPath);
FASTLED_SMART_PTR(PointPath);
FASTLED_SMART_PTR(LinePath);
FASTLED_SMART_PTR(CirclePath);
FASTLED_SMART_PTR(XYPathGenerator);
FASTLED_SMART_PTR(HeartPath);
FASTLED_SMART_PTR(ArchimedeanSpiralPath);
FASTLED_SMART_PTR(RosePath);
FASTLED_SMART_PTR(PhyllotaxisPath);
FASTLED_SMART_PTR(GielisCurvePath);
// FASTLED_SMART_PTR(LissajousPath);
// FASTLED_SMART_PTR(CatmullRomPath);

class XYPathGenerator : public Referent {
  public:
    virtual const Str name() const = 0;
    virtual point_xy_float compute(float alpha) = 0;
};

class XYPath : public Referent {
  public:
    static XYPathPtr NewPointPath(float x, float y) {
        auto path = PointPathPtr::New(x, y);
        return XYPathPtr::New(path);
    }

    static XYPathPtr NewLinePath(float x0, float y0, float x1, float y1) {
        auto path = LinePathPtr::New(x0, y0, x1, y1);
        return XYPathPtr::New(path);
    }
    static XYPathPtr NewCirclePath() {
        auto path = CirclePathPtr::New();
        return XYPathPtr::New(path);
    }

    static XYPathPtr NewCirclePath(uint16_t width, uint16_t height) {
        CirclePathPtr path = CirclePathPtr::New();
        XYPathPtr out = XYPathPtr::New(path);
        out->setDrawBounds(width, height);
        return out;
    }

    static XYPathPtr NewHeartPath() {
        HeartPathPtr path = HeartPathPtr::New();
        return XYPathPtr::New(path);
    }

    static XYPathPtr NewHeartPath(uint16_t width, uint16_t height) {
        HeartPathPtr path = HeartPathPtr::New();
        XYPathPtr out = XYPathPtr::New(path);
        out->setDrawBounds(width, height);
        return out;
    }

    static XYPathPtr NewArchimedeanSpiralPath(uint16_t width, uint16_t height) {
        ArchimedeanSpiralPathPtr path = ArchimedeanSpiralPathPtr::New();
        XYPathPtr out = XYPathPtr::New(path);
        out->setDrawBounds(width, height);
        return out;
    }

    static XYPathPtr NewArchimedeanSpiralPath() {
        ArchimedeanSpiralPathPtr path = ArchimedeanSpiralPathPtr::New();
        XYPathPtr out = XYPathPtr::New(path);
        return out;
    }

    static XYPathPtr NewRosePath(uint16_t width = 0, uint16_t height = 0,
                                 uint8_t n = 3, uint8_t d = 1) {
        RosePathPtr path = RosePathPtr::New(n, d);
        XYPathPtr out = XYPathPtr::New(path);
        if (width > 0 && height > 0) {
            out->setDrawBounds(width, height);
        }
        return out;
    }

    static XYPathPtr NewPhyllotaxisPath(uint16_t width = 0, uint16_t height = 0,
                                        float c = 4.0f, float angle = 1.5f) {
        PhyllotaxisPathPtr path = PhyllotaxisPathPtr::New(c, angle);
        XYPathPtr out = XYPathPtr::New(path);
        if (width > 0 && height > 0) {
            out->setDrawBounds(width, height);
        }
        return out;
    }

    static XYPathPtr NewGielisCurvePath(uint16_t width = 0, uint16_t height = 0,
                                        float a = 1.0f, float b = 1.0f,
                                        float m = 3.0f, float n1 = 1.0f,
                                        float n2 = 1.0f, float n3 = 1.0f) {
        GielisCurvePathPtr path = GielisCurvePathPtr::New(a, b, m, n1, n2, n3);
        XYPathPtr out = XYPathPtr::New(path);
        if (width > 0 && height > 0) {
            out->setDrawBounds(width, height);
        }
        return out;
    }

    Str name() { return mPath->name(); }

    // static LissajousPathPtr NewLissajousPath(uint8_t a, uint8_t b,
    //                                          float delta, uint16_t steps = 0)
    //                                          {
    //     return LissajousPathPtr::New(a, b, delta, steps);
    // }
    // static PhyllotaxisPathPtr NewPhyllotaxisPath(uint16_t count, float angle,
    //                                             uint16_t steps = 0) {
    //     return PhyllotaxisPathPtr::New(count, angle, steps);
    // }

    // static GielisCurvePathPtr NewGielisCurvePath(uint8_t m, float a, float b,
    //                                              float n1, float n2, float
    //                                              n3, uint16_t steps = 0) {
    //     return GielisCurvePathPtr::New(m, a, b, n1, n2, n3, steps);
    // }

    // static CatmullRomPathPtr NewCatmullRomPath(uint16_t steps = 0) {
    //     return CatmullRomPathPtr::New(steps);
    // }

    XYPath(XYPathGeneratorPtr path, TransformFloat transform = TransformFloat(),
           uint16_t steps = 0); // 0 steps means no LUT.

    point_xy_float at(float alpha) { return at(alpha, mTransform); }

    Tile2x2_u8 at_subpixel(float alpha);

    void
    rasterize(float from, float to, int steps, XYRasterSparse &raster,
              fl::Function<uint8_t(float)> *optional_alpha_gen = nullptr);
    ;

    // Overloaded to allow transform to be passed in.
    point_xy_float at(float alpha, const TransformFloat &tx) {
        return compute_float(alpha, tx);
    }

    // Needed for drawing to the screen. When this called the rendering will
    // be centered on the width and height such that 0,0 -> maps to .5,.5,
    // which is convenient for drawing since each float pixel can be truncated
    // to an integer type.
    void setDrawBounds(uint16_t width, uint16_t height) {
        // auto &tx = *(mGridTransform.mImpl);
        auto &tx = mGridTransform;

        // 1) map world‑X ∈ [–1..+1] → pixel‑X ∈ [0.5 .. width–0.5]
        //    scale_x  = ( (width–0.5) – 0.5 ) / 2 = (width–1)/2
        //    offset_x = (width–0.5 + 0.5) / 2 = width/2
        tx.set_scale_x((width - 1.0f) * 0.5f);
        // tx.scale_x = (width - 1.0f) * 0.5f;
        // tx.offset_x = width * 0.5f;
        tx.set_offset_x(width * 0.5f);

        // 2) map world‑Y ∈ [ -1 .. 1 ] → pixel‑Y ∈ [0.5 .. height–0.5]
        //    (your LinePath lives at Y=0, so it will sit at row‑0 center = 0.5)
        //    scale_y  = (height–0.5) – 0.5     = height–1
        //    offset_y = 0.5
        // tx.scale_y = (height - 1.0f) * 0.5f;
        // tx.offset_y = height * 0.5f;

        tx.set_scale_y((height - 1.0f) * 0.5f);
        tx.set_offset_y(height * 0.5f);

        onTransformFloatChanged();
        mDrawBoundsSet = true;
    }

    void onTransformFloatChanged() {
        // This is called when the transform changes. We need to clear the LUT
        // so that it will be rebuilt with the new transform.
        clearLut();
        // mTransform.validate();
        // Just recompute unconditionally. If this is a performance issue,
        // we can add a flag to make it lazy.
        // mTransform16 = mTransform.toTransform16();
    }

    TransformFloat &transform() { return mTransform; }

    void setScale(float scale) {
        // mTransform.scale_x = scale;
        // mTransform.scale_y = scale;
        mTransform.set_scale(scale);
        onTransformFloatChanged();
    }

    point_xy_float compute(float alpha) {
        return compute_float(alpha, mTransform);
    }

    // Optimizing for LUT transformations is work in progress. There are some
    // tests on these algorithms so they are kept in the code base to try out.

    // α in [0,65535] → (x,y) on the path, both in [0,65535].
    // This default implementation will build a LUT if mSteps > 0.
    // Subclasses may override this to avoid the LUT.
    point_xy<uint16_t> at16(uint16_t alpha,
                            const Transform16 &tx = Transform16());

    // optimizes at16(...).
    void buildLut(uint16_t steps);

    // Called by subclasses when something changes. The LUT will be rebuilt on
    // the next call to at16(...) if mSteps > 0.
    void clearLut() { mLut.reset(); }

    // Clears lut and sets new steps. LUT will be rebuilt on next call to
    // at16(...) if mSteps > 0.
    void clearLut(uint16_t steps) {
        mSteps = steps;
        mLut.reset();
    }

#if 0
    // Outputs the path as a series of points in floating poitn. The first and last points are
    // always the start and end points. The middle points are evenly spaced
    // according to the alpha range.
    void output(float alpha_start, float alpha_end, point_xy_float *out,
                uint16_t out_size, const TransformFloat &tx);

    // Outputs the path as a series of points in uint16_t. The first and last points are
    // always the start and end points. The middle points are evenly spaced
    // according to the alpha range.
    void output16(uint16_t alpha_start, uint16_t alpha_end,
                  point_xy<uint16_t> *out, uint16_t out_size,
                  const Transform16 &tx);
#endif

    LUTXY16Ptr getLut() const { return mLut; }

  private:
    XYPathGeneratorPtr mPath;
    TransformFloat mTransform;
    TransformFloat mGridTransform;

    uint32_t mSteps = 0;
    LUTXY16Ptr mLut;
    bool mDrawBoundsSet = false;

    // Transform16 mTransform16;
    void initLutOnce();
    LUTXY16Ptr generateLUT(uint16_t steps);
    point_xy_float compute_float(float alpha, const TransformFloat &tx);
};

///////////////// Implementations of common XYPaths ///////////////////

class PointPath : public XYPathGenerator {
  public:
    PointPath(float x, float y) : mPoint(x, y) {}
    PointPath(point_xy_float p) : mPoint(p) {}
    point_xy_float compute(float alpha) override {
        FASTLED_UNUSED(alpha);
        return mPoint;
    }
    const Str name() const override { return "PointPath"; }
    void set(float x, float y) { set(point_xy_float(x, y)); }
    void set(point_xy_float p) { mPoint = p; }

  private:
    point_xy_float mPoint;
};

class LinePath : public XYPathGenerator {
  public:
    LinePath(float x0, float y0, float x1, float y1);
    point_xy_float compute(float alpha) override;
    const Str name() const override { return "LinePath"; }
    void set(float x0, float y0, float x1, float y1);

  private:
    float mX0, mY0, mX1, mY1;
};

#if 0

/// Catmull–Rom spline through arbitrary points.
/// Simply add control points and compute(α) will smoothly interpolate through them.
class CatmullRomPath : public XYPath {
  public:
    /**
     * @param steps  LUT resolution (0 = no LUT)
     */
    CatmullRomPath(uint16_t steps = 0);

    /// Add a point in [0,1]² to the path
    void addPoint(point_xy_float p);

    point_xy_float compute(float alpha) override;
    const char *name() const override { return "CatmullRomPath"; }

  private:
    HeapVector<point_xy_float> mPoints;
};
#endif

class CirclePath : public XYPathGenerator {
  public:
    CirclePath();
    point_xy_float compute(float alpha) override;
    const Str name() const override { return "CirclePath"; }

  private:
    float mRadius;
};

class HeartPath : public XYPathGenerator {
  public:
    HeartPath();
    point_xy_float compute(float alpha) override;
    const Str name() const override { return "HeartPath"; }
};

class ArchimedeanSpiralPath : public XYPathGenerator {
  public:
    ArchimedeanSpiralPath(uint8_t turns = 3, float radius = 1.0f);
    point_xy_float compute(float alpha) override;
    const Str name() const override { return "ArchimedeanSpiralPath"; }

    void setTurns(uint8_t turns) { mTurns = turns; }
    void setRadius(float radius) { mRadius = radius; }

  private:
    uint8_t mTurns; // Number of spiral turns
    float mRadius;  // Maximum radius of the spiral
};

class RosePath : public XYPathGenerator {
  public:
    // n and d determine the shape of the rose curve
    // For n/d odd: produces n petals
    // For n/d even: produces 2n petals
    // For n and d coprime: produces n petals if n is odd, 2n petals if n is
    // even
    RosePath(uint8_t n = 3, uint8_t d = 1);
    point_xy_float compute(float alpha) override;
    const Str name() const override { return "RosePath"; }

    void setN(uint8_t n) { mN = n; }
    void setD(uint8_t d) { mD = d; }

  private:
    uint8_t mN; // Numerator parameter (number of petals)
    uint8_t mD; // Denominator parameter
};

class PhyllotaxisPath : public XYPathGenerator {
  public:
    // c is a scaling factor, angle is the divergence angle in degrees (often
    // 137.5° - the golden angle)
    PhyllotaxisPath(float c = 4.0f, float angle = 137.5f);
    point_xy_float compute(float alpha) override;
    const Str name() const override { return "PhyllotaxisPath"; }

    void setC(float c) { mC = c; }
    void setAngle(float angle) { mAngle = angle * PI / 180.0f; }

  private:
    float mC;     // Scaling factor
    float mAngle; // Angle between consecutive points in radians
    int mCount = 10;
};

class GielisCurvePath : public XYPathGenerator {
  public:
    // Gielis superformula parameters:
    // a, b: scaling parameters
    // m: symmetry parameter (number of rotational symmetries)
    // n1, n2, n3: shape parameters
    GielisCurvePath(float a = 1.0f, float b = 1.0f, float m = 3.0f,
                    float n1 = 1.0f, float n2 = 1.0f, float n3 = 100.0f) {
        mA = a;
        mB = b;
        mM = m;
        mN1 = n1;
        mN2 = n2;
        mN3 = n3;
    }
    point_xy_float compute(float alpha) override;
    const Str name() const override { return "GielisCurvePath"; }

    void setA(float a) { mA = a; }
    void setB(float b) { mB = b; }
    void setM(float m) { mM = m; }
    void setN1(float n1) { mN1 = n1; }
    void setN2(float n2) { mN2 = n2; }
    void setN3(float n3) { mN3 = n3; }

  private:
    float mA;  // Scaling parameter a
    float mB;  // Scaling parameter b
    float mM;  // Symmetry parameter (number of rotational symmetries)
    float mN1; // Shape parameter n1
    float mN2; // Shape parameter n2
    float mN3; // Shape parameter n3
};

} // namespace fl
