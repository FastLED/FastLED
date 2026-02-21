#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido_3.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void Complex_Kaleido_3::draw(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.001;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.038;
    e->timings.ratio[5] = 0.041;

    e->calculate_oscillators(e->timings);

    float size = 0.4 + e->move.directional[0] * 0.1;

    float q = 2;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                5 * e->polar_theta[x][y] + 10 * e->move.radial[0] +
                e->animation.dist / (((e->move.directional[0] + 3) * 2)) +
                e->move.noise_angle[0] * q;
            e->animation.z = 5;
            e->animation.scale_x = 0.08 * size * (e->move.directional[0] + 1.5);
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_x = -30 * e->move.linear[0];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                -5 * e->polar_theta[x][y] + 10 * e->move.radial[1] +
                e->animation.dist / (((e->move.directional[1] + 3) * 2)) +
                e->move.noise_angle[1] * q;
            e->animation.z = 500;
            e->animation.scale_x = 0.07 * size * (e->move.directional[1] + 1.1);
            e->animation.scale_y = 0.07 * size * (e->move.directional[2] + 1.3);
            ;
            e->animation.offset_z = -12 * e->move.linear[1];
            ;
            e->animation.offset_x = -(e->num_x - 1) * e->move.linear[1];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                -5 * e->polar_theta[x][y] + 12 * e->move.radial[2] +
                e->animation.dist / (((e->move.directional[3] + 3) * 2)) +
                e->move.noise_angle[2] * q;
            e->animation.z = 500;
            e->animation.scale_x = 0.05 * size * (e->move.directional[3] + 1.5);
            ;
            e->animation.scale_y = 0.05 * size * (e->move.directional[4] + 1.5);
            ;
            e->animation.offset_z = -12 * e->move.linear[3];
            e->animation.offset_x = -40 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                5 * e->polar_theta[x][y] + 12 * e->move.radial[3] +
                e->animation.dist / (((e->move.directional[5] + 3) * 2)) +
                e->move.noise_angle[3] * q;
            e->animation.z = 500;
            e->animation.scale_x = 0.09 * size * (e->move.directional[5] + 1.5);
            ;
            ;
            e->animation.scale_y = 0.09 * size * (e->move.directional[6] + 1.5);
            ;
            ;
            e->animation.offset_z = 0;
            e->animation.offset_x = -35 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3) - e->show2;
            e->show6 = e->colordodge(e->show4, e->show1);

            e->show7 = e->multiply(e->show1, e->show2);

            float linear1 = y / 32.f;

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->show7 = e->multiply(e->show1, e->show2) * linear1 * 2;
            e->show8 = e->subtract(e->show7, e->show5);

            e->pixel.green = 0.2 * e->show8;
            e->pixel.blue = e->show5 * radial;
            e->pixel.red = (1 * e->show1 + 1 * e->show2) - e->show7 / 2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
