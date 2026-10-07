// ok no header: Tile2x2 declarations remain in fl/gfx/tile2x2.h.

#include "fl/gfx/tile2x2.h"
#include "fl/gfx/raster_sparse.h"
#include "fl/stl/span.h"

namespace fl {

void Tile2x2_u8::Rasterize(const span<const Tile2x2_u8> &tiles,
                           XYRasterU8Sparse *out_raster) {
    out_raster->rasterize(tiles);
}

} // namespace fl
