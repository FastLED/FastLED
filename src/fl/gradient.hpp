
#include "fl/gradient.h"
#include "fl/assert.h"
#include "fl/colorutils.h"

namespace fl {

namespace {
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

    void accept(const Gradient::GradientFunction &func) {
        CRGB c = func(index);
        return_val = c;
    }

    template <typename T> void accept(const T &obj) {
        // This should never be called, but we need to provide a default
        // implementation to avoid compilation errors.
        accept(&obj);
    }

    CRGB return_val;
    uint8_t index;
};

struct VisitorFill {
    VisitorFill(Slice<const uint8_t> indices, Slice<CRGB> output)
        : output(output), indices(indices) {
        FASTLED_ASSERT(
            indices.size() == output.size(),
            "Gradient::fill: indices and output must be the same size");
        n = MIN(indices.size(), output.size());
    }
    void accept(const CRGBPalette16 *palette) {
        for (size_t i = 0; i < n; ++i) {
            output[i] = ColorFromPalette(*palette, indices[i]);
        }
    }

    void accept(const CRGBPalette32 *palette) {
        for (size_t i = 0; i < n; ++i) {
            output[i] = ColorFromPalette(*palette, indices[i]);
        }
    }

    void accept(const CRGBPalette256 *palette) {
        for (size_t i = 0; i < n; ++i) {
            output[i] = ColorFromPaletteExtended(*palette, indices[i]);
        }
    }

    void accept(const Gradient::GradientFunction &func) {
        for (size_t i = 0; i < n; ++i) {
            output[i] = func(indices[i]);
        }
    }

    template <typename T> void accept(const T &obj) {
        // This should never be called, but we need to provide a default
        // implementation to avoid compilation errors.
        accept(&obj);
    }

    Slice<CRGB> output;
    Slice<const uint8_t> indices;
    uint8_t n = 0;
};

} // namespace

CRGB Gradient::colorAt(uint8_t index) const {
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

void Gradient::fill(Slice<const uint8_t> input, Slice<CRGB> output) const {
    VisitorFill visitor(input, output);
    mVariant.visit(visitor);
}

CRGB GradientInlined::colorAt(uint8_t index) const {
    Visitor visitor(index);
    mVariant.visit(visitor);
    return visitor.return_val;
}
void GradientInlined::fill(Slice<const uint8_t> input,
                           Slice<CRGB> output) const {
    VisitorFill visitor(input, output);
    mVariant.visit(visitor);
}

Gradient::Gradient(const GradientInlined &other) {
    // Visitor is cumbersome but guarantees all paths are handled.
    struct Copy {
        Copy(Gradient &owner) : mOwner(owner) {}
        void accept(const CRGBPalette16 &palette) { mOwner.set(&palette); }
        void accept(const CRGBPalette32 &palette) { mOwner.set(&palette); }
        void accept(const CRGBPalette256 &palette) { mOwner.set(&palette); }
        void accept(const GradientFunction &func) { mOwner.set(func); }
        Gradient &mOwner;
    };
    Copy copy_to_self(*this);
    other.variant().visit(copy_to_self);
}

} // namespace fl