#include "fl/fx/2d/animartrix2_detail/viz/scaledemo1.h"

namespace fl {

void Scaledemo1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.000001;
    e->timings.ratio[0] = 0.4;
    e->timings.ratio[1] = 0.32;
    e->timings.ratio[2] = 0.10;
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
            e->animation.dist = 0.3 * e->distance[x][y] * 0.8;
            e->animation.angle = 3 * e->polar_theta[x][y] + e->move.radial[2];
            e->animation.scale_x = 0.1 + (e->move.noise_angle[0]) / 10;
            e->animation.scale_y = 0.1 + (e->move.noise_angle[1]) / 10;
            e->animation.scale_z = 0.01;
            e->animation.offset_y = 0;
            e->animation.offset_x = 0;
            e->animation.offset_z = 100 * e->move.linear[0];
            e->animation.z = 30;
            float show1 = e->render_value(e->animation);

            e->animation.angle = 3;
            float show2 = e->render_value(e->animation);

            float dist = 1;
            e->pixel.red = show1 * dist;
            e->pixel.green = (show1 - show2) * dist * 0.3;
            e->pixel.blue = (show2 - show1) * dist;

            if (e->distance[x][y] > 16) {
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
