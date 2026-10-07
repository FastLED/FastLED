#include "fl/math/xymap.h" // ok no header - public declaration remains in parent directory.
#include "fl/math/xmap.h"
#include "fl/math/lut.h"
#include "fl/stl/shared_ptr.h"

namespace fl {

XYMap XYMap::fromXMap(const XMap& xmap) FL_NO_EXCEPT {
    // Create an XYMap with width=xmap.length and height=1
    // This treats the 1D strip as a 2D grid with height 1
    u16 length = xmap.getLength();

    // Null rather than a table: the LUT is allocated here and filled just
    // below, which is what the null case in `constructWithLookUpTable`
    // exists for.
    auto out = XYMap::constructWithLookUpTable(length, 1, nullptr);
    fl::shared_ptr<LUT16> lut = fl::make_shared<LUT16>(length);
    u16* data = lut->getDataMutable();

    // Fill the LUT with xmap's mappings
    for (u16 i = 0; i < length; i++) {
        data[i] = xmap.mapToIndex(i);
    }

    out.mLookUpTable = lut;
    return out;
}

} // namespace fl
