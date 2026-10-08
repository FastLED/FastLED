#pragma once

// Caleido1 visualizer class
// Extracted from animartrix_detail.hpp

#include "fl/fx/2d/animartrix_detail/fp_state.h"
#include "fl/fx/2d/animartrix_detail/viz/viz_base.h"
#include "fl/stl/noexcept.h"

namespace fl {

class Caleido1 : public IAnimartrix2Viz {
public:
    void draw(Context &ctx) FL_NO_EXCEPT override;
};


// Fixed-point Q31 scalar implementation of Caleido1.
class Caleido1_FP : public IAnimartrix2Viz {
public:
    void draw(Context &ctx) FL_NO_EXCEPT override;
private:
    FPVizState mState;
};

}  // namespace fl
