#pragma once

#include "fl/colorutils.h"
#include "fl/function.h"
#include "fl/type_traits.h"
#include "fl/variant.h"

namespace fl {

class CRGBPalette16;
class CRGBPalette32;
class CRGBPalette256;

class Gradient {
  public:
    using GradientFunction = fl::function<CRGB(uint8_t index)>;
    Gradient() = default;

    template <typename T> Gradient(T *palette);
    Gradient(const Gradient &other);
    Gradient &operator=(const Gradient &other);

    Gradient(Gradient &&other) noexcept;

    // non template allows carefull control of what can be set.
    void set(const CRGBPalette16 *palette);
    void set(const CRGBPalette32 *palette);
    void set(const CRGBPalette256 *palette);
    void set(const GradientFunction &func);

    CRGB colorAt(uint8_t index) const;

  private:
    using GradientVariant =
        Variant<const CRGBPalette16 *, const CRGBPalette32 *,
                const CRGBPalette256 *, GradientFunction>;
    GradientVariant mVariant;
};

} // namespace fl