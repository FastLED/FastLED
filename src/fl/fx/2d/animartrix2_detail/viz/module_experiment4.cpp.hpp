#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment4.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void Module_Experiment4::draw(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.031;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.036;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.8;

            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) * 0.7;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.004 * s;
            e->animation.scale_y = 0.003 * s;
            e->animation.offset_z = 0.1 * e->move.linear[2];
            e->animation.offset_y = -20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) * 0.8;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.004 * s;
            e->animation.scale_y = 0.003 * s;
            e->animation.offset_z = 0.1 * e->move.linear[3];
            e->animation.offset_y = -20 * e->move.linear[3];
            e->animation.offset_x = 100;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) * 0.9;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5000;
            e->animation.scale_x = 0.004 * s;
            e->animation.scale_y = 0.003 * s;
            e->animation.offset_z = 0.1 * e->move.linear[4];
            e->animation.offset_y = -20 * e->move.linear[4];
            e->animation.offset_x = 1000;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->pixel.red = e->show1 - e->show2 - e->show3;
            e->pixel.blue = e->show2 - e->show1 - e->show3;
            e->pixel.green = e->show3 - e->show1 - e->show2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
