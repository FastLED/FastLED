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
    Gradient() FL_NOEXCEPT = default;
    Gradient(const GradientInlined &other);

    template <typename T> Gradient(T *palette);
    Gradient(const Gradient &other) FL_NOEXCEPT;
    Gradient &operator=(const Gradient &other);

    Gradient(Gradient &&other) FL_NOEXCEPT;

    // non template allows carefull control of what can be set.
    void set(const CRGBPalette16 *palette);
    void set(const CRGBPalette32 *palette);
    void set(const CRGBPalette256 *palette);
    void set(const GradientFunction &func);

    CRGB colorAt(u8 index) const;
    void fill(span<const u8> input, span<CRGB> output) const;

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
    GradientInlined() FL_NOEXCEPT = default;

    template <typename T> GradientInlined(const T &palette) { set(palette); }

    GradientInlined(const GradientInlined &other) FL_NOEXCEPT = default;
    GradientInlined &operator=(const GradientInlined &other) FL_NOEXCEPT = default;

    void set(const CRGBPalette16 &palette) { mVariant = palette; }
    void set(const CRGBPalette32 &palette) { mVariant = palette; }
    void set(const CRGBPalette256 &palette) { mVariant = palette; }
    void set(const GradientFunction &func) { mVariant = func; }

    CRGB colorAt(u8 index) const;
    void fill(span<const u8> input, span<CRGB> output) const;

    GradientVariant &getVariant() { return mVariant; }
    const GradientVariant &getVariant() const { return mVariant; }

  private:
    GradientVariant mVariant;
};

} // namespace fl
