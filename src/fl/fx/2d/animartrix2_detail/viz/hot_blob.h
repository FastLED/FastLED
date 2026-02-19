#pragma once

// Hot_Blob visualizer class
// Extracted from animartrix2_detail.hpp

#include "fl/fx/2d/animartrix2_detail/viz/viz_base.h"

namespace fl {

class Hot_Blob : public IAnimartrix2Viz {
public:
    void draw(Context &ctx) override;
};

}  // namespace fl
