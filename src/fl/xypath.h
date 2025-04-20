

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
#include "fl/transform.h"

namespace fl {

// Smart pointers for the XYPath family.
FASTLED_SMART_PTR(XYPath);
FASTLED_SMART_PTR(LinePath);
FASTLED_SMART_PTR(CirclePath);
// FASTLED_SMART_PTR(HeartPath);
// FASTLED_SMART_PTR(LissajousPath);
// FASTLED_SMART_PTR(ArchimedeanSpiralPath);
// FASTLED_SMART_PTR(RosePath);
// FASTLED_SMART_PTR(PhyllotaxisPath);
// FASTLED_SMART_PTR(GielisCurvePath);
// FASTLED_SMART_PTR(CatmullRomPath);


class XYPath : public Referent {
  public:
    static LinePathPtr NewLinePath(float x0, float y0, float x1, float y1,
                                    uint16_t steps = 0) {
        return LinePathPtr::New(x0, y0, x1, y1, steps);
    }
    static CirclePathPtr NewCirclePath(uint16_t steps = 0) {
        return CirclePathPtr::New(steps);
    }


    // static HeartPathPtr NewHeartPath(uint16_t steps = 0) {
    //     return HeartPathPtr::New(steps);
    // }

    // static LissajousPathPtr NewLissajousPath(uint8_t a, uint8_t b,
    //                                          float delta, uint16_t steps = 0) {
    //     return LissajousPathPtr::New(a, b, delta, steps);
    // }

    // static ArchimedeanSpiralPathPtr NewArchimedeanSpiralPath(
    //     uint8_t turns, float radius, uint16_t steps = 0) {
    //     return ArchimedeanSpiralPathPtr::New(turns, radius, steps);
    // }

    // static RosePathPtr NewRosePath(uint8_t petals, uint16_t steps = 0) {
    //     return RosePathPtr::New(petals, steps);
    // }
    // static PhyllotaxisPathPtr NewPhyllotaxisPath(uint16_t count, float angle,
    //                                             uint16_t steps = 0) {
    //     return PhyllotaxisPathPtr::New(count, angle, steps);
    // }

    // static GielisCurvePathPtr NewGielisCurvePath(uint8_t m, float a, float b,
    //                                              float n1, float n2, float n3,
    //                                              uint16_t steps = 0) {
    //     return GielisCurvePathPtr::New(m, a, b, n1, n2, n3, steps);
    // }

    // static CatmullRomPathPtr NewCatmullRomPath(uint16_t steps = 0) {
    //     return CatmullRomPathPtr::New(steps);
    // }

    XYPath(uint16_t steps = 0); // 0 steps means no LUT.
    // α in [0,1] → (x,y) on the path, both in [0,1].

    virtual const char *name() const = 0;

    point_xy_float at(float alpha) { return at(alpha, mTransform); }

    // Overloaded to allow transform to be passed in.
    point_xy_float at(float alpha, const TransformFloat &tx) {
        return compute_float(alpha, tx);
    }

    void set(uint16_t width, uint16_t height, float scale = 1.0f) {
        if (width > 0) {
          width -= 1;
        }
        if (height > 0) {
          height -= 1;
        }
        mTransform.scale_x = width * scale;
        mTransform.scale_y = height * scale;
        mTransform.x_offset = 0;
        mTransform.y_offset = 0;
        onTransformFloatChanged();
    }

    void onTransformFloatChanged() {
        // This is called when the transform changes. We need to clear the LUT
        // so that it will be rebuilt with the new transform.
        clearLut();
        // mTransform.validate();
        // Just recompute unconditionally. If this is a performance issue,
        // we can add a flag to make it lazy.
        //mTransform16 = mTransform.toTransform16();
    }

    void setTransform(const TransformFloat &tx) {
        mTransform = tx;
        onTransformFloatChanged();
    }

    float getScaleX() const { return mTransform.scale_x; }
    float getScaleY() const { return mTransform.scale_y; }
    float getXOffset() const { return mTransform.x_offset; }
    float getYOffset() const { return mTransform.y_offset; }
    float getRotation() const { return mTransform.rotation; }
    void setScaleX(float scale_x) { mTransform.scale_x = scale_x; onTransformFloatChanged(); }
    void setScaleY(float scale_y) { mTransform.scale_y = scale_y; onTransformFloatChanged(); }
    void setXOffset(float x_offset) { mTransform.x_offset = x_offset; onTransformFloatChanged(); }
    void setYOffset(float y_offset) { mTransform.y_offset = y_offset; onTransformFloatChanged(); }
    void setRotation(float rotation) { mTransform.rotation = rotation; onTransformFloatChanged(); }

    void addScaleX(float scale_x) {
        mTransform.scale_x += scale_x;
        onTransformFloatChanged();
    }
    void addScaleY(float scale_y) {
        mTransform.scale_y += scale_y;
        onTransformFloatChanged();
    }

    void addXOffset(float x_offset) {
        mTransform.x_offset += x_offset;
        onTransformFloatChanged();
    }

    void addYOffset(float y_offset) {
        mTransform.y_offset += y_offset;
        onTransformFloatChanged();
    }

    void addRotation(float rotation) {
        mTransform.rotation += rotation;
        onTransformFloatChanged();
    }

    // Subclasses must implement this method.
    virtual point_xy_float compute(float alpha) = 0;

    // α in [0,65535] → (x,y) on the path, both in [0,65535].
    // This default implementation will build a LUT if mSteps > 0.
    // Subclasses may override this to avoid the LUT.
    virtual point_xy<uint16_t> at16(uint16_t alpha,
                                    const Transform16 &tx = Transform16());

    // optimizes at16(...).
    void buildLut(uint16_t steps);

    // Called by subclasses when something changes. The LUT will be rebuilt on
    // the next call to at16(...) if mSteps > 0.
    void clearLut() {
        mLut.reset();
    }

    // Clears lut and sets new steps. LUT will be rebuilt on next call to
    // at16(...) if mSteps > 0.
    void clearLut(uint16_t steps) {
        mSteps = steps;
        mLut.reset();
    }

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

    LUTXY16Ptr getLut() const { return mLut; }

  protected:
    uint32_t mSteps;
    LUTXY16Ptr mLut;

  private:
    TransformFloat mTransform;
    //Transform16 mTransform16;
    void initLutOnce();
    LUTXY16Ptr generateLUT(uint16_t steps);
    point_xy_float compute_float(float alpha, const TransformFloat &tx);
};


///////////////// Implementations of common XYPaths ///////////////////

class LinePath : public XYPath {
  public:
    LinePath(float x0, float y0, float x1, float y1, uint16_t steps = 0);
    point_xy_float compute(float alpha) override;
    const char *name() const override { return "LinePath"; }
    void set(float x0, float y0, float x1, float y1);

  private:
    float mX0, mY0, mX1, mY1;
};

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

class CirclePath : public XYPath {
  public:
    CirclePath(uint16_t steps = 0);
    point_xy_float compute(float alpha) override;
    const char *name() const override { return "CirclePath"; }

  private:
    float mRadius;
};


} // namespace fl
