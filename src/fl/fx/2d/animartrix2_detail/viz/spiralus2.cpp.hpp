#include "fl/fx/2d/animartrix2_detail/viz/spiralus2.h"

namespace fl {

void Spiralus2::draw(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.0015;
    e->timings.ratio[0] = 1.5;
    e->timings.ratio[1] = 2.3;
    e->timings.ratio[2] = 3;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.2;
    e->timings.ratio[5] = 0.05;
    e->timings.ratio[6] = 0.055;
    e->timings.ratio[7] = 0.06;
    e->timings.ratio[8] = 0.027;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + e->move.noise_angle[5] +
                                 e->move.directional[3] * e->move.noise_angle[6] *
                                     e->animation.dist / 10;
            e->animation.scale_x = 0.08;
            e->animation.scale_y = 0.08;
            e->animation.scale_z = 0.02;
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[1];
            float show1 = e->render_value(e->animation);

            e->animation.angle = 6 * e->polar_theta[x][y] + e->move.noise_angle[7] +
                                 e->move.directional[5] * e->move.noise_angle[8] *
                                     e->animation.dist / 10;
            e->animation.offset_y = -e->move.linear[1];
            e->animation.z = e->move.linear[2];
            float show2 = e->render_value(e->animation);

            e->animation.angle = 6 * e->polar_theta[x][y] + e->move.noise_angle[6] +
                                 e->move.directional[6] * e->move.noise_angle[7] *
                                     e->animation.dist / 10;
            e->animation.offset_y = e->move.linear[2];
            e->animation.z = e->move.linear[0];
            e->animation.dist = e->distance[x][y] * 0.8;
            float show3 = e->render_value(e->animation);

            float f = 1;

            e->pixel.red = f * (show1 + show2);
            e->pixel.green = f * (show1 - show2);
            e->pixel.blue = f * (show3 - show1);

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
