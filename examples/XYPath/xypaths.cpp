

#include "fl/xypath.h"
#include "fl/vector.h"


#include "xypaths.h"

using namespace fl;

fl::vector<XYPathPtr> CreateXYPaths(int width, int height) {
    fl::vector<XYPathPtr> out;

    out.push_back(XYPath::NewRosePath(width, height));
    out.push_back(XYPath::NewCirclePath(width, height));
    out.push_back(XYPath::NewHeartPath(width, height));
    out.push_back(XYPath::NewArchimedeanSpiralPath(width, height));
    return out;
}