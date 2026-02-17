#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix3.h"

namespace fl {

void SpiralMatrix3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = -20 * e->move.linear[0];
            ;
            e->animation.low_limit = -1;
            e->animation.high_limit = 1;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = -20 * e->move.linear[0];
            ;
            e->animation.low_limit = -1;
            e->animation.high_limit = 1;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 500 + e->show1 / 20;
            e->animation.offset_y = -4 * e->move.linear[0] + e->show2 / 20;
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 500 + e->show1 / 18;
            e->animation.offset_y = -4 * e->move.linear[0] + e->show2 / 18;
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 500 + e->show1 / 19;
            e->animation.offset_y = -4 * e->move.linear[0] + e->show2 / 19;
            e->animation.low_limit = 0.3;
            e->animation.high_limit = 1;
            e->show5 = e->render_value(e->animation);

            e->pixel.red = e->show4;
            e->pixel.green = e->show3;
            e->pixel.blue = e->show5;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
