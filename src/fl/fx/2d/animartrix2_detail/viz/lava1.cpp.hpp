#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix2_detail/viz/lava1.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void Lava1::draw(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.0015;
    e->timings.ratio[0] = 4;
    e->timings.ratio[1] = 1;
    e->timings.ratio[2] = 1;
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
            e->animation.dist = e->distance[x][y] * 0.8;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.12;
            e->animation.scale_z = 0.01;
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 30;
            float show1 = e->render_value(e->animation);

            e->animation.offset_y = -e->move.linear[1];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.12;
            e->animation.offset_x = show1 / 100;
            e->animation.offset_y += show1 / 100;
            float show2 = e->render_value(e->animation);

            e->animation.offset_y = -e->move.linear[2];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.12;
            e->animation.offset_x = show2 / 100;
            e->animation.offset_y += show2 / 100;
            float show3 = e->render_value(e->animation);

            float linear = (y) / (e->num_y - 1.f);

            e->pixel.red = linear * show2;
            e->pixel.green = 0.1 * linear * (show2 - show3);
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
