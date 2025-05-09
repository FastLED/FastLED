#pragma once

#include "fl/colorutils.h"
#include "fl/type_traits.h"
#include "fl/variant.h"

namespace fl {

class CRGBPalette16;
class CRGBPalette32;
class CRGBPalette256;

class Gradient {
  public:
    Gradient() = default;

    template <typename T> Gradient(T *palette) { set(palette); }
    Gradient(const Gradient &other) : mVariant(other.mVariant) {}
    Gradient &operator=(const Gradient &other) {
        if (this != &other) {
            mVariant = other.mVariant;
        }
        return *this;
    }

    Gradient(Gradient &&other) noexcept : mVariant(fl::move(other.mVariant)) {}

    // non template allows carefull control of what can be set.
    void set(const CRGBPalette16 *palette) { mVariant = palette; }
    void set(const CRGBPalette32 *palette) { mVariant = palette; }
    void set(const CRGBPalette256 *palette) { mVariant = palette; }

    CRGB at(uint8_t index) const;

  private:
    using PalletteVariant =
        Variant<const CRGBPalette16 *, const CRGBPalette32 *,
                const CRGBPalette256 *>;
    PalletteVariant mVariant;
};

} // namespace fl