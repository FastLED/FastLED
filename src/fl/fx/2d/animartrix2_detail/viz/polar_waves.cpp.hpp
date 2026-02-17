#include "fl/fx/2d/animartrix2_detail/viz/polar_waves.h"

namespace fl {

void Polar_Waves(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.5;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = (e->distance[x][y]);
            e->animation.angle =
                e->polar_theta[x][y] - e->animation.dist * 0.1 + e->move.radial[0];
            e->animation.z = (e->animation.dist * 1.5) - 10 * e->move.linear[0];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_x = e->move.linear[0];

            float show1 = e->render_value(e->animation);
            e->animation.angle =
                e->polar_theta[x][y] - e->animation.dist * 0.1 + e->move.radial[1];
            e->animation.z = (e->animation.dist * 1.5) - 10 * e->move.linear[1];
            e->animation.offset_x = e->move.linear[1];

            float show2 = e->render_value(e->animation);
            e->animation.angle =
                e->polar_theta[x][y] - e->animation.dist * 0.1 + e->move.radial[2];
            e->animation.z = (e->animation.dist * 1.5) - 10 * e->move.linear[2];
            e->animation.offset_x = e->move.linear[2];

            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * show1;
            e->pixel.green = radial * show2;
            e->pixel.blue = radial * show3;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
