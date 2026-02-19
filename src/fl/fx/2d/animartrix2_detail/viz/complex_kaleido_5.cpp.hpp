#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido_5.h"

namespace fl {

void Complex_Kaleido_5::draw(Context &ctx) {
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

    float size = 0.6;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 1 + e->move.directional[6] * 0.8;

            e->animation.dist = e->distance[x][y] * s;
            e->animation.angle = 10 * e->move.radial[6] +
                                 50 * e->move.directional[5] * e->polar_theta[x][y] -
                                 e->animation.dist / 3;
            e->animation.z = 5;
            e->animation.scale_x = 0.08 * size;
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.low_limit = -0.5;
            e->show1 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = e->show1 * radial;
            e->pixel.green = 0;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
