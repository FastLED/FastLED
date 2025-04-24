

#include "fl/xypath.h"
#include "fl/vector.h"


#include "xypaths.h"

using namespace fl;

namespace {
    Ptr<CatmullRomParams> make_path(int width, int height) {
        // make a triangle.
        Ptr<CatmullRomParams> params = NewPtr<CatmullRomParams>();
        params->addPoint(0.0f, 0.0f);
        params->addPoint(width - 1, height / 2);
        params->addPoint(width - 1, height - 1);
        params->addPoint(0.0f, height - 1);
        params->addPoint(0.0f, 0.0f);
        return params;
    }
}

fl::vector<XYPathPtr> CreateXYPaths(int width, int height) {
    fl::vector<XYPathPtr> out;
    out.push_back(XYPath::NewCirclePath(width, height));
    out.push_back(XYPath::NewRosePath(width, height));
    out.push_back(XYPath::NewHeartPath(width, height));
    out.push_back(XYPath::NewArchimedeanSpiralPath(width, height));
    out.push_back(XYPath::NewPhyllotaxisPath(width, height));
    out.push_back(XYPath::NewGielisCurvePath(width, height));
    out.push_back(XYPath::NewCatmullRomPath(make_path(width, height)));
    return out;
}