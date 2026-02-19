#pragma once

// Rings visualizer class
// Extracted from animartrix2_detail.hpp

#include "fl/fx/2d/animartrix2_detail/viz/viz_base.h"

namespace fl {

class Rings : public IAnimartrix2Viz {
public:
    void draw(Context &ctx) override;
};

}  // namespace fl
