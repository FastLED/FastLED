#include "fl/fx/2d/animartrix2_detail/viz/water.h"

namespace fl {

void Water(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.037;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.1;
    e->timings.ratio[6] = 0.41;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist =
                e->distance[x][y] +
                4 * fl::sinf(e->move.directional[5] * PI + (float)x / 2) +
                4 * fl::cosf(e->move.directional[6] * PI + float(y) / 2);
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.06;
            e->animation.scale_y = 0.06;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_y = 10;
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = (10 + e->move.directional[0]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[0] +
                                      (e->distance[x][y] / (3)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = (10 + e->move.directional[1]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[1] +
                                      (e->distance[x][y] / (3)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[1];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = (10 + e->move.directional[2]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[2] +
                                      (e->distance[x][y] / (3)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->pixel.blue = (0.7 * e->show2 + 0.6 * e->show3 + 0.5 * e->show4);
            e->pixel.red = e->pixel.blue - 40;
            e->pixel.green = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
