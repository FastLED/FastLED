
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
FASTLED_SMART_PTR(XYPathRenderer);
FASTLED_SMART_PTR(XYPathGenerator);
FASTLED_SMART_PTR(PointPath);
FASTLED_SMART_PTR(LinePath);
FASTLED_SMART_PTR(CirclePath);
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

class XYPathParams : public Referent {
  public:
    // Reserved for future use.
    TransformFloat transform;
    float brightness = 1.0f; // 0: off, 1: full brightness
};

class LinePathParams : public XYPathParams {
  public:
    float x0 = -1.0f; // Start x coordinate
    float y0 = 0.0f;  // Start y coordinate
    float x1 = 1.0f;  // End x coordinate
    float y1 = 0.0f;  // End y coordinate
};

class PhyllotaxisParams : public XYPathParams {
  public:
    float c = 4.0f;       // Scaling factor
    float angle = 137.5f; // Divergence angle in degrees
};

class RosePathParams : public XYPathParams {
  public:
    uint8_t n = 3; // Numerator parameter (number of petals)
    uint8_t d = 1; // Denominator parameter
};

class GielisCurveParams : public XYPathParams {
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
    static XYPathPtr NewPointPath(float x, float y);
    static XYPathPtr NewLinePath(float x0, float y0, float x1, float y1);
    static XYPathPtr
    NewLinePath(const Ptr<LinePathParams> &params = NewPtr<LinePathParams>());

    static XYPathPtr NewCirclePath();
    static XYPathPtr NewCirclePath(uint16_t width, uint16_t height);
    static XYPathPtr NewHeartPath();
    static XYPathPtr NewHeartPath(uint16_t width, uint16_t height);
    static XYPathPtr NewArchimedeanSpiralPath(uint16_t width, uint16_t height);
    static XYPathPtr NewArchimedeanSpiralPath();

    static XYPathPtr
    NewRosePath(uint16_t width = 0, uint16_t height = 0,
                const Ptr<RosePathParams> &params = NewPtr<RosePathParams>());

    static XYPathPtr NewPhyllotaxisPath(
        uint16_t width = 0, uint16_t height = 0,
        const Ptr<PhyllotaxisParams> &args = NewPtr<PhyllotaxisParams>());

    static XYPathPtr NewGielisCurvePath(
        uint16_t width = 0, uint16_t height = 0,
        const Ptr<GielisCurveParams> &params = NewPtr<GielisCurveParams>());

    virtual ~XYPath();
    point_xy_float at(float alpha);
    Tile2x2_u8 at_subpixel(float alpha);
    void rasterize(float from, float to, int steps, XYRasterSparse &raster,
                   fl::function<uint8_t(float)> *optional_alpha_gen = nullptr);
    void setScale(float scale);
    Str name() const;
    // Overloaded to allow transform to be passed in.
    point_xy_float at(float alpha, const TransformFloat &tx);
    // Needed for drawing to the screen. When this called the rendering will
    // be centered on the width and height such that 0,0 -> maps to .5,.5,
    // which is convenient for drawing since each float pixel can be truncated
    // to an integer type.
    void setDrawBounds(uint16_t width, uint16_t height);
    TransformFloat &transform();

    XYPath(XYPathGeneratorPtr path,
           TransformFloat transform = TransformFloat());

  private:
    XYPathGeneratorPtr mPath;
    XYPathRendererPtr mPathRenderer;
};

///////////////// Implementations of common XYPaths ///////////////////

class PointPath : public XYPathGenerator {
  public:
    PointPath(float x, float y);
    PointPath(point_xy_float p);
    point_xy_float compute(float alpha) override;
    const Str name() const override;
    void set(float x, float y);
    void set(point_xy_float p);

  private:
    point_xy_float mPoint;
};

class LinePath : public XYPathGenerator {
  public:
    LinePath(const LinePathParamsPtr &params = NewPtr<LinePathParams>());
    LinePath(float x0, float y0, float x1, float y1);
    point_xy_float compute(float alpha) override;
    const Str name() const override;
    void set(float x0, float y0, float x1, float y1);
    void set(const LinePathParams &p);

    LinePathParams &params();
    const LinePathParams &params() const;

  private:
    Ptr<LinePathParams> mParams;
};

#if 0

/// Catmull–Rom spline through arbitrary points.
/// Simply add control points and compute(α) will smoothly interpolate through them.
class CatmullRomPath : public XYPathRenderer {
  public:
    /**
     * @param steps  LUT resolution (0 = no LUT)
     */
    CatmullRomPath(uint16_t steps = 0);

    /// Add a point in [0,1]² to the path
    void addPoint(point_xy_float p);

    point_xy_float compute(float alpha) override;
    const char *name() const override ;

  private:
    HeapVector<point_xy_float> mPoints;
};
#endif

class CirclePath : public XYPathGenerator {
  public:
    CirclePath();
    point_xy_float compute(float alpha) override;
    const Str name() const override;
};

class HeartPath : public XYPathGenerator {
  public:
    HeartPath();
    point_xy_float compute(float alpha) override;
    const Str name() const override;
};

class ArchimedeanSpiralPath : public XYPathGenerator {
  public:
    ArchimedeanSpiralPath(uint8_t turns = 3, float radius = 1.0f);
    point_xy_float compute(float alpha) override;
    const Str name() const override;

    void setTurns(uint8_t turns);
    void setRadius(float radius);

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
    RosePath(const Ptr<RosePathParams> &p = NewPtr<RosePathParams>());
    RosePath(uint8_t n = 3, uint8_t d = 1);
    point_xy_float compute(float alpha) override;
    const Str name() const override;

    RosePathParams &params();
    const RosePathParams &params() const;

    void setN(uint8_t n);
    void setD(uint8_t d);

  private:
    Ptr<RosePathParams> mParams;
};

class PhyllotaxisPath : public XYPathGenerator {
  public:
    // c is a scaling factor, angle is the divergence angle in degrees (often
    // 137.5° - the golden angle)
    PhyllotaxisPath(
        const Ptr<PhyllotaxisParams> &p = NewPtr<PhyllotaxisParams>());
    point_xy_float compute(float alpha) override;
    const Str name() const override;

    PhyllotaxisParams &params();
    const PhyllotaxisParams &params() const;

  private:
    Ptr<PhyllotaxisParams> mParams;
};

class GielisCurvePath : public XYPathGenerator {
  public:
    // Gielis superformula parameters:
    // a, b: scaling parameters
    // m: symmetry parameter (number of rotational symmetries)
    // n1, n2, n3: shape parameters
    GielisCurvePath(
        const Ptr<GielisCurveParams> &p = NewPtr<GielisCurveParams>());
    point_xy_float compute(float alpha) override;
    const Str name() const override;

    GielisCurveParams &params();
    const GielisCurveParams &params() const;

    void setA(float a);
    void setB(float b);
    void setM(float m);
    void setN1(float n1);
    void setN2(float n2);
    void setN3(float n3);

  private:
    Ptr<GielisCurveParams> mParams;
};

} // namespace fl
