
#pragma once

// This is a drawing/graphics related class.
//
// XYPath represents a parameterized (x,y) path. The input will always be
// an alpha value between 0->1 (float) or 0->0xffff (u16).
// A look up table can be used to optimize path calculations when steps > 0.
//
// We provide common paths discovered throughout human history, for use in
// your animations.

#include "fl/lut.h"
#include "fl/math_macros.h"
#include "fl/memory.h"
#include "fl/tile2x2.h"
#include "fl/transform.h"
#include "fl/unused.h"
#include "fl/vector.h"
#include "fl/warn.h"

namespace fl {

class XYRasterU8Sparse;
template <typename T> class function;

// Smart pointers for the XYPath family.
FASTLED_SMART_PTR(XYPath);
FASTLED_SMART_PTR(PointPath);
FASTLED_SMART_PTR(LinePath);
FASTLED_SMART_PTR(CirclePath);
FASTLED_SMART_PTR(HeartPath);
FASTLED_SMART_PTR(ArchimedeanSpiralPath);
FASTLED_SMART_PTR(RosePath);
FASTLED_SMART_PTR(PhyllotaxisPath);
FASTLED_SMART_PTR(GielisCurvePath);
FASTLED_SMART_PTR(LinePathParams);
FASTLED_SMART_PTR(RosePathParams);
FASTLED_SMART_PTR(CatmullRomParams);
FASTLED_SMART_PTR(CatmullRomPath);

// FASTLED_SMART_PTR(LissajousPath);

// BaseClasses.
// Controllable parameter base class. Each subtype has a transform and
// brightness.
class XYPathParams {
  public:
    // Reserved for future use.
    TransformFloat transform;
    float brightness = 1.0f; // 0: off, 1: full brightness
};

// Base class for the actual path generator.
class XYPathGenerator {
  public:
    virtual ~XYPathGenerator() = default; // Add virtual destructor for proper cleanup
    virtual const string name() const = 0;
    virtual vec2f compute(float alpha) = 0;
    // No writes when returning false.
    virtual bool hasDrawBounds(rect<i16> *bounds) {
        FASTLED_UNUSED(bounds);
        return false;
    }
};

/////////////////////////////////////////////////
// Begin parameter classes.
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
    u8 n = 3; // Numerator parameter (number of petals)
    u8 d = 1; // Denominator parameter
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

class CatmullRomParams : public XYPathParams {
  public:
    CatmullRomParams() {}

    // Add a point to the path
    void addPoint(vec2f p) { points.push_back(p); }

    // Add a point with separate x,y coordinates
    void addPoint(float x, float y) { points.push_back(vec2f(x, y)); }

    // Clear all control points
    void clear() { points.clear(); }

    // Get the number of control points
    fl::size size() const { return points.size(); }

    // Vector of control points
    HeapVector<vec2f> points;
};

/////////////////////////////////////////////////
// Begin implementations of common XYPaths

class PointPath : public XYPathGenerator {
  public:
    PointPath(float x, float y);
    PointPath(vec2f p);
    vec2f compute(float alpha) override;
    const string name() const override;
    void set(float x, float y);
    void set(vec2f p);

  private:
    vec2f mPoint;
};

class LinePath : public XYPathGenerator {
  public:
    LinePath(const LinePathParamsPtr &params = fl::make_shared<LinePathParams>());
    LinePath(float x0, float y0, float x1, float y1);
    vec2f compute(float alpha) override;
    const string name() const override;
    void set(float x0, float y0, float x1, float y1);
    void set(const LinePathParams &p);

    LinePathParams &params();
    const LinePathParams &params() const;

  private:
    fl::shared_ptr<LinePathParams> mParams;
};

class CirclePath : public XYPathGenerator {
  public:
    CirclePath();
    vec2f compute(float alpha) override;
    const string name() const override;
};

class HeartPath : public XYPathGenerator {
  public:
    HeartPath();
    vec2f compute(float alpha) override;
    const string name() const override;
};

class ArchimedeanSpiralPath : public XYPathGenerator {
  public:
    ArchimedeanSpiralPath(u8 turns = 3, float radius = 1.0f);
    vec2f compute(float alpha) override;
    const string name() const override;

    void setTurns(u8 turns);
    void setRadius(float radius);

  private:
    u8 mTurns; // Number of spiral turns
    float mRadius;  // Maximum radius of the spiral
};

class RosePath : public XYPathGenerator {
  public:
    // n and d determine the shape of the rose curve
    // For n/d odd: produces n petals
    // For n/d even: produces 2n petals
    // For n and d coprime: produces n petals if n is odd, 2n petals if n is
    // even
    RosePath(const fl::shared_ptr<RosePathParams> &p = fl::make_shared<RosePathParams>());
    RosePath(u8 n = 3, u8 d = 1);
    vec2f compute(float alpha) override;
    const string name() const override;

    RosePathParams &params();
    const RosePathParams &params() const;

    void setN(u8 n);
    void setD(u8 d);

  private:
    fl::shared_ptr<RosePathParams> mParams;
};

class PhyllotaxisPath : public XYPathGenerator {
  public:
    // c is a scaling factor, angle is the divergence angle in degrees (often
    // 137.5° - the golden angle)
    PhyllotaxisPath(
        const fl::shared_ptr<PhyllotaxisParams> &p = fl::make_shared<PhyllotaxisParams>());
    vec2f compute(float alpha) override;
    const string name() const override;

    PhyllotaxisParams &params();
    const PhyllotaxisParams &params() const;

  private:
    fl::shared_ptr<PhyllotaxisParams> mParams;
};

class GielisCurvePath : public XYPathGenerator {
  public:
    // Gielis superformula parameters:
    // a, b: scaling parameters
    // m: symmetry parameter (number of rotational symmetries)
    // n1, n2, n3: shape parameters
    GielisCurvePath(
        const fl::shared_ptr<GielisCurveParams> &p = fl::make_shared<GielisCurveParams>());
    vec2f compute(float alpha) override;
    const string name() const override;

    GielisCurveParams &params();
    const GielisCurveParams &params() const;

    void setA(float a);
    void setB(float b);
    void setM(float m);
    void setN1(float n1);
    void setN2(float n2);
    void setN3(float n3);

  private:
    fl::shared_ptr<GielisCurveParams> mParams;
};

/// Catmull–Rom spline through arbitrary points.
/// Simply add control points and compute(α) will smoothly interpolate through
/// them.
class CatmullRomPath : public XYPathGenerator {
  public:
    CatmullRomPath(const fl::shared_ptr<CatmullRomParams> &p = fl::make_shared<CatmullRomParams>());

    /// Add a point in [0,1]² to the path
    void addPoint(vec2f p);

    /// Add a point with separate x,y coordinates
    void addPoint(float x, float y);

    /// Clear all control points
    void clear();

    /// Get the number of control points
    fl::size size() const;

    vec2f compute(float alpha) override;
    const string name() const override;

    CatmullRomParams &params();
    const CatmullRomParams &params() const;

  private:
    fl::shared_ptr<CatmullRomParams> mParams;

    // Helper function to interpolate between points using Catmull-Rom spline
    vec2f interpolate(const vec2f &p0, const vec2f &p1, const vec2f &p2,
                      const vec2f &p3, float t) const;
};

// Smart pointer for CatmullRomPath
FASTLED_SMART_PTR(CatmullRomPath);

} // namespace fl
