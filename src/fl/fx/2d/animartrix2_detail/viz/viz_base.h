#pragma once

// IAnimartrix2Viz: abstract base class for all Animartrix2 visualizers.
//
// By default visualizers have no member variables — draw() bodies are identical
// to the former free functions. Stateful visualizers (e.g. Chasing_Spirals_SIMD)
// declare their cache as private members so state is owned per-instance rather
// than as a global singleton.
//
// Lifetime is managed by Animartrix2 via fl::unique_ptr<IAnimartrix2Viz>:
// a new instance is constructed when the selected animation changes, and the
// previous instance (with its cached state) is automatically destroyed.

#include "fl/fx/2d/animartrix2_detail/context.h"

namespace fl {

class IAnimartrix2Viz {
public:
    virtual ~IAnimartrix2Viz() = default;
    virtual void draw(Context &ctx) = 0;
};

} // namespace fl
