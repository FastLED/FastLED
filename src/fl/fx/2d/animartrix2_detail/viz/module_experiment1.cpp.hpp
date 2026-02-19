#include "fl/fx/2d/animartrix2_detail/viz/module_experiment1.h"

namespace fl {

void Module_Experiment1::draw(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y] + 20 * e->move.directional[0];
            e->animation.angle = e->move.noise_angle[0] + e->move.noise_angle[1] +
                                 e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = 0;
            e->pixel.green = 0;
            e->pixel.blue = e->show1;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
