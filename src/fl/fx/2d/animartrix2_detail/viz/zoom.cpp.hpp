#include "fl/fx/2d/animartrix2_detail/viz/zoom.h"

namespace fl {

void Zoom(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->run_default_oscillators();
    e->timings.master_speed = 0.003;
    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) / 2;
            e->animation.angle = e->polar_theta[x][y];

            e->animation.scale_x = 0.005;
            e->animation.scale_y = 0.005;

            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;

            e->animation.z = 0;
            e->animation.low_limit = 0;
            float show1 = e->render_value(e->animation);

            float linear = 1;

            e->pixel.red = show1 * linear;
            e->pixel.green = 0;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
