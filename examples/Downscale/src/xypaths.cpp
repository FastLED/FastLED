

#include "fl/xypath.h"
#include "fl/stl/vector.h"
#include "fl/map_range.h"


#include "xypaths.h"

namespace {
    fl::shared_ptr<fl::CatmullRomParams> make_path(int width, int height) {
        // make a triangle.
        fl::shared_ptr<fl::CatmullRomParams> params = fl::make_shared<fl::CatmullRomParams>();
        fl::vector_inlined<fl::vec2f, 5> points;
        points.push_back(fl::vec2f(0.0f, 0.0f));
        points.push_back(fl::vec2f(width / 3, height / 2));
        points.push_back(fl::vec2f(width - 3, height - 1));
        points.push_back(fl::vec2f(0.0f, height - 1));
        points.push_back(fl::vec2f(0.0f, 0.0f));
        for (auto &p : points) {
            p.x = fl::map_range<float, float>(p.x, 0.0f, width - 1, -1.0f, 1.0f);
            p.y = fl::map_range<float, float>(p.y, 0.0f, height - 1, -1.0f, 1.0f);
            params->addPoint(p);
        }
        return params;
    }
}

fl::vector<fl::XYPathPtr> CreateXYPaths(int width, int height) {
    fl::vector<fl::XYPathPtr> out;
    out.push_back(fl::XYPath::NewCirclePath(width, height));
    out.push_back(fl::XYPath::NewRosePath(width, height));
    out.push_back(fl::XYPath::NewHeartPath(width, height));
    out.push_back(fl::XYPath::NewArchimedeanSpiralPath(width, height));
    out.push_back(fl::XYPath::NewPhyllotaxisPath(width, height));
    out.push_back(fl::XYPath::NewGielisCurvePath(width, height));
    out.push_back(fl::XYPath::NewCatmullRomPath(width, height, make_path(width, height)));
    return out;
}
