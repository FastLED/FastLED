#include "fl/fx/2d/animartrix2_detail/viz/caleido3.h"

namespace fl {

void Caleido3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.004;
    e->timings.ratio[0] = 0.02;
    e->timings.ratio[1] = 0.03;
    e->timings.ratio[2] = 0.04;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[0]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[0] + e->move.radial[4];
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = 2 * e->move.linear[0];
            e->animation.offset_x = 2 * e->move.linear[1];
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[1]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[1] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[1];
            e->animation.offset_y = show1 / 20.0;
            e->animation.z = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[2]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[2] + e->move.radial[4];
            e->animation.offset_y = 2 * e->move.linear[2];
            e->animation.offset_x = show2 / 20.0;
            e->animation.z = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[3]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[3] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[3];
            e->animation.offset_y = show3 / 20.0;
            e->animation.z = e->move.linear[3];
            float show4 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;

            e->pixel.red = show1 * (y + 1) / e->num_y;
            e->pixel.green = show3 * e->distance[x][y] / 10;
            e->pixel.blue = (show2 + show4) / 2;
            if (e->distance[x][y] > radius) {
                e->pixel.red = 0;
                e->pixel.green = 0;
                e->pixel.blue = 0;
            }

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
