

#include "fl/xypath.h"
#include "fl/vector.h"
#include "fl/map_range.h"


#include "xypaths.h"

using namespace fl;

namespace {
    fl::shared_ptr<CatmullRomParams> make_path(int width, int height) {
        // make a triangle.
        fl::shared_ptr<CatmullRomParams> params = fl::make_shared<CatmullRomParams>();
        vector_inlined<vec2f, 5> points;
        points.push_back(vec2f(0.0f, 0.0f));
        points.push_back(vec2f(width / 3, height / 2));
        points.push_back(vec2f(width - 3, height - 1));
        points.push_back(vec2f(0.0f, height - 1));
        points.push_back(vec2f(0.0f, 0.0f));
        for (auto &p : points) {
            p.x = map_range<float, float>(p.x, 0.0f, width - 1, -1.0f, 1.0f);
            p.y = map_range<float, float>(p.y, 0.0f, height - 1, -1.0f, 1.0f);
            params->addPoint(p);
        }
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
    out.push_back(XYPath::NewCatmullRomPath(width, height, make_path(width, height)));
    return out;
}
