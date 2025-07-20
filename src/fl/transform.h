#pragma once

/*
Efficient transform classes for floating point and alpha16 coordinate systems.
Note that component transforms are used because it's easy to skip calculations
for components that are not used. For example, if the rotation is 0 then no
expensive trig functions are needed. Same with scale and offset.

*/

#include "fl/lut.h"
#include "fl/math_macros.h"
#include "fl/memory.h"
#include "fl/xymap.h"
#include "lib8tion/types.h"

namespace fl {

FASTLED_SMART_PTR(TransformFloatImpl);

using alpha16 =
    u16; // fixed point representation of 0->1 in the range [0, 65535]

// This transform assumes the coordinates are in the range [0,65535].
struct Transform16 {
    // Make a transform that maps a rectangle to the given bounds from
    // (0,0) to (max_value,max_value), inclusive.
    static Transform16 ToBounds(alpha16 max_value);
    static Transform16 ToBounds(const vec2<alpha16> &min,
                                const vec2<alpha16> &max, alpha16 rotation = 0);

    static Transform16 From(u16 width, u16 height) {
        vec2<alpha16> min = vec2<alpha16>(0, 0);
        vec2<alpha16> max = vec2<alpha16>(width, height);
        return Transform16::ToBounds(min, max);
    }

    // static Transform16 From(const XYMap &map) {
    //     return Transform16::ToBounds(map.getWidth(), map.getHeight());
    // }
    Transform16() = default;
    
    // Use default move constructor and assignment operator for POD data
    Transform16(Transform16 &&other) noexcept = default;
    Transform16 &operator=(Transform16 &&other) noexcept = default;
    
    alpha16 scale_x = 0xffff;
    alpha16 scale_y = 0xffff;
    alpha16 offset_x = 0;
    alpha16 offset_y = 0;
    alpha16 rotation = 0;

    vec2<alpha16> transform(const vec2<alpha16> &xy) const;
};

// This transform assumes the coordinates are in the range [0,1].
class TransformFloatImpl {
  public:
    static TransformFloatImplPtr Identity() {
        TransformFloatImplPtr tx = fl::make_shared<TransformFloatImpl>();
        return tx;
    }
    TransformFloatImpl() = default;
    virtual ~TransformFloatImpl() = default; // Add virtual destructor for proper cleanup
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float rotation = 0.0f; // rotation range is [0,1], not [0,2*PI]!
    float scale() const;
    void set_scale(float scale);
    vec2f transform(const vec2f &xy) const;
    bool is_identity() const;
};

// Future usage.
struct Matrix3x3f {
    Matrix3x3f() = default;
    Matrix3x3f(const Matrix3x3f &) = default;
    Matrix3x3f &operator=(const Matrix3x3f &) = default;
    Matrix3x3f(Matrix3x3f &&) noexcept = default;
    Matrix3x3f &operator=(Matrix3x3f &&) noexcept = default;
    
    static Matrix3x3f Identity() {
        Matrix3x3f m;
        return m;
    }
    
    vec2<float> transform(const vec2<float> &xy) const {
        vec2<float> out;
        out.x = m[0][0] * xy.x + m[0][1] * xy.y + m[0][2];
        out.y = m[1][0] * xy.x + m[1][1] * xy.y + m[1][2];
        return out;
    }
    float m[3][3] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
    };
};

// TransformFloat is a wrapper around the smart ptr. This version allows for
// easy use and fast / well behaved copy.
struct TransformFloat {
    TransformFloat() = default;
    float scale_x() const { return mImpl->scale_x; }
    float scale_y() const { return mImpl->scale_y; }
    float offset_x() const { return mImpl->offset_x; }
    float offset_y() const { return mImpl->offset_y; }
    // rotation range is [0,1], not [0,2*PI]!
    float rotation() const { return mImpl->rotation; }
    float scale() const { return MIN(scale_x(), scale_y()); }
    void set_scale(float scale) { mImpl->set_scale(scale); }
    void set_scale_x(float scale) { mImpl->scale_x = scale; }
    void set_scale_y(float scale) { mImpl->scale_y = scale; }
    void set_offset_x(float offset) { mImpl->offset_x = offset; }
    void set_offset_y(float offset) { mImpl->offset_y = offset; }
    void set_rotation(float rotation) { mImpl->rotation = rotation; }

    vec2f transform(const vec2f &xy) const {
        // mDirty = true; // always recompile.
        // compileIfNecessary();
        // return mCompiled.transform(xy);
        return mImpl->transform(xy);
    }
    bool is_identity() const { return mImpl->is_identity(); }

    Matrix3x3f compile() const;
    void compileIfNecessary() const {
        // if (mDirty) {
        //     mCompiled = compile();
        //     mDirty = false;
        // }
    }

  private:
    TransformFloatImplPtr mImpl = TransformFloatImpl::Identity();
    // Matrix3x3f mCompiled;  // future use.
    // mutable bool mDirty = true;   // future use.
    mutable Matrix3x3f mCompiled; // future use.
};

} // namespace fl
