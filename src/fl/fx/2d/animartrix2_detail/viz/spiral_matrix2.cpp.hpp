#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix2.h"

namespace fl {

void SpiralMatrix2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y] * (e->move.directional[0]);
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[1];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[2];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[2];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show3 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show2;
            e->pixel.blue = show3;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
