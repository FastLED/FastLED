#pragma once

#include "fl/colorutils.h"
#include "fl/function.h"
#include "fl/span.h"
#include "fl/type_traits.h"
#include "fl/variant.h"

namespace fl {

class CRGBPalette16;
class CRGBPalette32;
class CRGBPalette256;
class GradientInlined;

class Gradient {
  public:
    using GradientFunction = fl::function<CRGB(u8 index)>;
    Gradient() = default;
    Gradient(const GradientInlined &other);

    template <typename T> Gradient(T *palette);
    Gradient(const Gradient &other);
    Gradient &operator=(const Gradient &other);

    Gradient(Gradient &&other) noexcept;

    // non template allows carefull control of what can be set.
    void set(const CRGBPalette16 *palette);
    void set(const CRGBPalette32 *palette);
    void set(const CRGBPalette256 *palette);
    void set(const GradientFunction &func);

    CRGB colorAt(u8 index) const;
    void fill(span<const u8> input, span<CRGB> output) const;

  private:
    using GradientVariant =
        Variant<const CRGBPalette16 *, const CRGBPalette32 *,
                const CRGBPalette256 *, GradientFunction>;
    GradientVariant mVariant;
};

class GradientInlined {
  public:
    using GradientFunction = fl::function<CRGB(u8 index)>;
    using GradientVariant =
        Variant<CRGBPalette16, CRGBPalette32, CRGBPalette256, GradientFunction>;
    GradientInlined() = default;

    template <typename T> GradientInlined(const T &palette) { set(palette); }

    GradientInlined(const GradientInlined &other) = default;
    GradientInlined &operator=(const GradientInlined &other) = default;

    void set(const CRGBPalette16 &palette) { mVariant = palette; }
    void set(const CRGBPalette32 &palette) { mVariant = palette; }
    void set(const CRGBPalette256 &palette) { mVariant = palette; }
    void set(const GradientFunction &func) { mVariant = func; }

    CRGB colorAt(u8 index) const;
    void fill(span<const u8> input, span<CRGB> output) const;

    GradientVariant &variant() { return mVariant; }
    const GradientVariant &variant() const { return mVariant; }

  private:
    GradientVariant mVariant;
};

} // namespace fl
