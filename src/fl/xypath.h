
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
template <typename T> class function;

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

FASTLED_SMART_PTR(LinePathParams);
FASTLED_SMART_PTR(RosePathParams);

class XYPathGenerator : public Referent {
  public:
    virtual const Str name() const = 0;
    virtual point_xy_float compute(float alpha) = 0;
};

class XYPathParams: public Referent {
    TransformFloat transform;
    float brightness = 1.0f;  // 0: off, 1: full brightness
};

class LinePathParams: public XYPathParams {
  public:
    float x0 = -1.0f;  // Start x coordinate
    float y0 = 0.0f;   // Start y coordinate
    float x1 = 1.0f;   // End x coordinate
    float y1 = 0.0f;   // End y coordinate
};

class PhyllotaxisParams: public XYPathParams {
  public:
    float c = 4.0f;       // Scaling factor
    float angle = 137.5f; // Divergence angle in degrees
};

class RosePathParams: public XYPathParams {
  public:
    uint8_t n = 3;     // Numerator parameter (number of petals)
    uint8_t d = 1;     // Denominator parameter
};

class GielisCurveParams: public XYPathParams {
  public:
    float a = 1.0f;    // Scaling parameter a
    float b = 1.0f;    // Scaling parameter b
    float m = 3.0f;    // Symmetry parameter (number of rotational symmetries)
    float n1 = 1.0f;   // Shape parameter n1
    float n2 = 1.0f;   // Shape parameter n2
    float n3 = 100.0f; // Shape parameter n3
};

class XYPath : public Referent {
  public:
    static XYPathPtr NewPointPath(float x, float y) {
        auto path = PointPathPtr::New(x, y);
        return XYPathPtr::New(path);
    }

    static XYPathPtr NewLinePath(float x0, float y0, float x1, float y1) {
        LinePathParamsPtr p = LinePathParamsPtr::New();
        auto& params = *p;
        params.x0 = x0;
        params.y0 = y0;
        params.x1 = x1;
        params.y1 = y1;
        auto path = LinePathPtr::New(p);
        return XYPathPtr::New(path);
    }
    
    static XYPathPtr NewLinePath(const Ptr<LinePathParams>& params = NewPtr<LinePathParams>()) {
        auto path = NewPtr<LinePath>(params);
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


    static XYPathPtr
    NewRosePath(uint16_t width = 0, uint16_t height = 0,
                const Ptr<RosePathParams> &params = NewPtr<RosePathParams>()) {
        RosePathPtr path = RosePathPtr::New(params);
        XYPathPtr out = XYPathPtr::New(path);
        if (width > 0 && height > 0) {
            out->setDrawBounds(width, height);
        }
        return out;
    }

    static XYPathPtr
    NewPhyllotaxisPath(uint16_t width = 0, uint16_t height = 0,
                       const Ptr<PhyllotaxisParams> &args = NewPtr<PhyllotaxisParams>()) {
        PhyllotaxisPathPtr path = PhyllotaxisPathPtr::New(args);
        XYPathPtr out = XYPathPtr::New(path);
        if (width > 0 && height > 0) {
            out->setDrawBounds(width, height);
        }
        return out;
    }

    static XYPathPtr
    NewGielisCurvePath(uint16_t width = 0, uint16_t height = 0,
                       const Ptr<GielisCurveParams> &params = NewPtr<GielisCurveParams>()) {
        GielisCurvePathPtr path = GielisCurvePathPtr::New(params);
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

    void rasterize(float from, float to, int steps, XYRasterSparse &raster,
                   fl::function<uint8_t(float)> *optional_alpha_gen = nullptr);
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
        // Future use to allow recomputing the LUT.
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


  private:
    XYPathGeneratorPtr mPath;
    TransformFloat mTransform;
    TransformFloat mGridTransform;
    uint32_t mSteps = 0;
    LUTXY16Ptr mLut;
    bool mDrawBoundsSet = false;
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
    LinePath(const LinePathParamsPtr& params = NewPtr<LinePathParams>())
        : mParams(params) {};
    LinePath(float x0, float y0, float x1, float y1);
    point_xy_float compute(float alpha) override;
    const Str name() const override { return "LinePath"; }
    void set(float x0, float y0, float x1, float y1);
    void set(const LinePathParams &p);
    
    LinePathParams& params() { return *mParams; }
    const LinePathParams& params() const { return *mParams; }

  private:
    Ptr<LinePathParams> mParams;
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
    RosePath(const Ptr<RosePathParams> &p = NewPtr<RosePathParams>())
        : mParams(p) {}
    RosePath(uint8_t n = 3, uint8_t d = 1);
    point_xy_float compute(float alpha) override;
    const Str name() const override { return "RosePath"; }

    RosePathParams& params() { return *mParams; }
    const RosePathParams& params() const { return *mParams; }
    
    void setN(uint8_t n) { params().n = n; }
    void setD(uint8_t d) { params().d = d; }

  private:
    Ptr<RosePathParams> mParams;
};

class PhyllotaxisPath : public XYPathGenerator {
  public:
    // c is a scaling factor, angle is the divergence angle in degrees (often
    // 137.5° - the golden angle)
    PhyllotaxisPath(const Ptr<PhyllotaxisParams> &p = NewPtr<PhyllotaxisParams>())
        : mParams(p) {}
    point_xy_float compute(float alpha) override;
    const Str name() const override { return "PhyllotaxisPath"; }

    PhyllotaxisParams& params() { return *mParams; }
    const PhyllotaxisParams& params() const { return *mParams; }

  private:
    Ptr<PhyllotaxisParams> mParams;
};

class GielisCurvePath : public XYPathGenerator {
  public:
    // Gielis superformula parameters:
    // a, b: scaling parameters
    // m: symmetry parameter (number of rotational symmetries)
    // n1, n2, n3: shape parameters
    GielisCurvePath(const Ptr<GielisCurveParams> &p = NewPtr<GielisCurveParams>())
        : mParams(p) {}
    point_xy_float compute(float alpha) override;
    const Str name() const override { return "GielisCurvePath"; }

    GielisCurveParams& params() { return *mParams; }
    const GielisCurveParams& params() const { return *mParams; }

    void setA(float a) { params().a = a; }
    void setB(float b) { params().b = b; }
    void setM(float m) { params().m = m; }
    void setN1(float n1) { params().n1 = n1; }
    void setN2(float n2) { params().n2 = n2; }
    void setN3(float n3) { params().n3 = n3; }

  private:
    Ptr<GielisCurveParams> mParams;
};

} // namespace fl
