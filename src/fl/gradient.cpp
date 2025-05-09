
#include "fl/gradient.h"
#include "fl/colorutils.h"

namespace fl {

CRGB Gradient::at(uint8_t index) const {
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
            CRGB c = ColorFromPalette(*palette, index);
            return_val = c;
        }

        CRGB return_val;
        uint8_t index;
    };

    Visitor visitor(index);
    mVariant.visit(visitor);
    return visitor.return_val;
}

} // namespace fl