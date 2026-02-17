#include "fl/fx/2d/animartrix2_detail/viz/slow_fade.h"

namespace fl {

void Slow_Fade(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->run_default_oscillators();
    e->timings.master_speed = 0.00005;
    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist =
                fl::sqrtf(e->distance[x][y]) * 0.7 * (e->move.directional[0] + 1.5);
            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[0] + e->distance[x][y] / 5;

            e->animation.scale_x = 0.11;
            e->animation.scale_y = 0.11;

            e->animation.offset_y = -50 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;

            e->animation.z = e->move.linear[0];
            e->animation.low_limit = -0.1;
            e->animation.high_limit = 1;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->animation.dist * 1.1;
            e->animation.angle += e->move.noise_angle[0] / 10;
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->animation.dist * 1.1;
            e->animation.angle += e->move.noise_angle[1] / 10;
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * show1;
            e->pixel.green = radial * (show1 - show2) / 6;
            e->pixel.blue = radial * (show1 - show3) / 5;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
