#include "fl/fx/2d/animartrix2_detail/viz/module_experiment9.h"

namespace fl {

void Module_Experiment9(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;

    float w = 0.3;

    e->timings.ratio[0] = 0.1;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.013;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;
    e->timings.ratio[5] = 0.38 * w;
    e->timings.ratio[6] = 0.0003;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1];
            e->animation.z = 5;
            e->animation.scale_x = 0.001;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_x = 20;
            e->animation.offset_z = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = 10 * e->show1;
            e->pixel.green = 0;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
