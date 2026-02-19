#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido_2.h"

namespace fl {

void Complex_Kaleido_2::draw(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.009;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    float size = 0.5;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + 10 * e->move.radial[0] +
                                 e->animation.dist / 2;
            e->animation.z = 5;
            e->animation.scale_x = 0.07 * size;
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -30 * e->move.linear[0];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = -5 * e->polar_theta[x][y] + 12 * e->move.radial[1] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.07 * size;
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -30 * e->move.linear[1];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = -5 * e->polar_theta[x][y] + 12 * e->move.radial[2] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.05 * size;
            e->animation.scale_y = 0.05 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -40 * e->move.linear[2];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + 12 * e->move.radial[3] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.09 * size;
            e->animation.scale_y = 0.09 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -35 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3);
            e->show6 = e->colordodge(e->show2, e->show3);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (e->show1 + e->show2);
            e->pixel.green = 0.3 * radial * e->show6;
            e->pixel.blue = radial * e->show5;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
