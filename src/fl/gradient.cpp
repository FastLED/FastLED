
#include "fl/gradient.h"
#include "fl/colorutils.h"

namespace fl {

CRGB Gradient::colorAt(uint8_t index) const {
    struct Visitor {
        Visitor(uint8_t index) : index(index) {}
        void accept(const CRGBPalette16 *palette) {
            CRGB c = ColorFromPalette(*palette, index);
            return_val = c;
        }

        void accept(const CRGBPalette32 *palette) {
            CRGB c = ColorFromPalette(*palette, index);
            return_val = c;
        }

        void accept(const CRGBPalette256 *palette) {
            CRGB c = ColorFromPaletteExtended(*palette, index);
            return_val = c;
        }

        void accept(const GradientFunction &func) {
            CRGB c = func(index);
            return_val = c;
        }

        CRGB return_val;
        uint8_t index;
    };

    Visitor visitor(index);
    mVariant.visit(visitor);
    return visitor.return_val;
}

template <typename T> Gradient::Gradient(T *palette) { set(palette); }

Gradient::Gradient(const Gradient &other) : mVariant(other.mVariant) {}

Gradient::Gradient(Gradient &&other) noexcept
    : mVariant(move(other.mVariant)) {}

void Gradient::set(const CRGBPalette32 *palette) { mVariant = palette; }

void Gradient::set(const CRGBPalette256 *palette) { mVariant = palette; }

void Gradient::set(const CRGBPalette16 *palette) { mVariant = palette; }

void Gradient::set(const GradientFunction &func) { mVariant = func; }

Gradient &Gradient::operator=(const Gradient &other) {
    if (this != &other) {
        mVariant = other.mVariant;
    }
    return *this;
}

} // namespace fl