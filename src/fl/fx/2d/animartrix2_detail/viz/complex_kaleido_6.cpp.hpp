#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido_6.h"

namespace fl {

void Complex_Kaleido_6::draw(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.0038;
    e->timings.ratio[6] = 0.041;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 16 * e->polar_theta[x][y] + 16 * e->move.radial[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.06;
            e->animation.scale_y = 0.06;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_y = 10 * e->move.noise_angle[0];
            e->animation.offset_x = 10 * e->move.noise_angle[4];
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 16 * e->polar_theta[x][y] + 16 * e->move.radial[1];
            e->animation.z = 500;
            e->animation.scale_x = 0.06;
            e->animation.scale_y = 0.06;
            e->animation.offset_z = -10 * e->move.linear[1];
            e->animation.offset_y = 10 * e->move.noise_angle[1];
            e->animation.offset_x = 10 * e->move.noise_angle[3];
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->pixel.red = e->show1;
            e->pixel.green = 0;
            e->pixel.blue = e->show2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
