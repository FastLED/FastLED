#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix10.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void SpiralMatrix10::draw(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.006;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float scale = 0.6;

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -30 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = -1;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -30 * e->move.linear[1];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = -1;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 2 + (e->show1 / 255) * PI;
            e->animation.z = 5;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 2 + (e->show2 / 255) * PI;
            ;
            e->animation.z = 5;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3);
            e->show6 = e->colordodge(e->show5, e->show3);

            e->pixel.red = (e->show5 + e->show6) / 2;
            e->pixel.green = (e->show5 - 50) + (e->show6 / 16);
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
