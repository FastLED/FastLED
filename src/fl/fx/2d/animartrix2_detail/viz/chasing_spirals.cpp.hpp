#include "fl/fx/2d/animartrix2_detail/viz/chasing_spirals.h"

namespace fl {

void Chasing_Spirals(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.1;
    e->timings.ratio[1] = 0.13;
    e->timings.ratio[2] = 0.16;

    e->timings.offset[1] = 10;
    e->timings.offset[2] = 20;
    e->timings.offset[3] = 30;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[0] - e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.scale_z = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_x = 0.1;
            e->animation.offset_x = e->move.linear[0];
            e->animation.offset_y = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[1] - e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_x = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[2] - e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_x = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial_filter = (radius - e->distance[x][y]) / radius;

            e->pixel.red = 3 * show1 * radial_filter;
            e->pixel.green = show2 * radial_filter / 2;
            e->pixel.blue = show3 * radial_filter / 4;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
