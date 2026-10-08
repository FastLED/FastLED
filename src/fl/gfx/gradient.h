#pragma once

#include "fl/gfx/colorutils.h"
#include "fl/stl/function.h"
#include "fl/stl/span.h"
#include "fl/stl/variant.h"
#include "fl/stl/noexcept.h"

namespace fl {

class CRGBPalette16;  // IWYU pragma: keep
class CRGBPalette32;  // IWYU pragma: keep
class CRGBPalette256;  // IWYU pragma: keep
class GradientInlined;

class Gradient {
  public:
    using GradientFunction = fl::function<CRGB(u8 index)>;
    Gradient() FL_NO_EXCEPT = default;
    Gradient(const GradientInlined &other) FL_NO_EXCEPT;

    template <typename T> Gradient(T *palette) FL_NO_EXCEPT;
    Gradient(const Gradient &other) FL_NO_EXCEPT;
    Gradient &operator=(const Gradient &other) FL_NO_EXCEPT;

    Gradient(Gradient &&other) FL_NO_EXCEPT;

    // non template allows carefull control of what can be set.
    void set(const CRGBPalette16 *palette) FL_NO_EXCEPT;
    void set(const CRGBPalette32 *palette) FL_NO_EXCEPT;
    void set(const CRGBPalette256 *palette) FL_NO_EXCEPT;
    void set(const GradientFunction &func) FL_NO_EXCEPT;

    CRGB colorAt(u8 index) const FL_NO_EXCEPT;
    void fill(span<const u8> input, span<CRGB> output) const FL_NO_EXCEPT;

  private:
    using GradientVariant =
        variant<const CRGBPalette16 *, const CRGBPalette32 *,
                const CRGBPalette256 *, GradientFunction>;
    GradientVariant mVariant;
};

class GradientInlined {
  public:
    using GradientFunction = fl::function<CRGB(u8 index)>;
    using GradientVariant =
        variant<CRGBPalette16, CRGBPalette32, CRGBPalette256, GradientFunction>;
    GradientInlined() FL_NO_EXCEPT = default;

    template <typename T> GradientInlined(const T &palette) FL_NO_EXCEPT { set(palette); }

    GradientInlined(const GradientInlined &other) FL_NO_EXCEPT = default;
    GradientInlined &operator=(const GradientInlined &other) FL_NO_EXCEPT = default;

    void set(const CRGBPalette16 &palette) FL_NO_EXCEPT { mVariant = palette; }
    void set(const CRGBPalette32 &palette) FL_NO_EXCEPT { mVariant = palette; }
    void set(const CRGBPalette256 &palette) FL_NO_EXCEPT { mVariant = palette; }
    void set(const GradientFunction &func) FL_NO_EXCEPT { mVariant = func; }

    CRGB colorAt(u8 index) const FL_NO_EXCEPT;
    void fill(span<const u8> input, span<CRGB> output) const FL_NO_EXCEPT;

    GradientVariant &getVariant() FL_NO_EXCEPT { return mVariant; }
    const GradientVariant &getVariant() const FL_NO_EXCEPT { return mVariant; }

  private:
    GradientVariant mVariant;
};

} // namespace fl
