#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido_4.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void Complex_Kaleido_4::draw(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.038;
    e->timings.ratio[6] = 0.041;

    e->calculate_oscillators(e->timings);

    float size = 0.6;

    float q = 1;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 1 + e->move.directional[6] * 0.3;

            e->animation.dist = e->distance[x][y] * s;
            e->animation.angle =
                5 * e->polar_theta[x][y] + 1 * e->move.radial[0] -
                e->animation.dist / (3 + e->move.directional[0] * 0.5);
            e->animation.z = 5;
            e->animation.scale_x = 0.08 * size + (e->move.directional[0] * 0.01);
            e->animation.scale_y = 0.07 * size + (e->move.directional[1] * 0.01);
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * s;
            e->animation.angle =
                5 * e->polar_theta[x][y] + 1 * e->move.radial[1] +
                e->animation.dist / (3 + e->move.directional[1] * 0.5);
            e->animation.z = 50;
            e->animation.scale_x = 0.08 * size + (e->move.directional[1] * 0.01);
            e->animation.scale_y = 0.07 * size + (e->move.directional[2] * 0.01);
            e->animation.offset_z = -10 * e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 1;
            e->animation.z = 500;
            e->animation.scale_x = 0.2 * size;
            e->animation.scale_y = 0.2 * size;
            e->animation.offset_z = 0;
            e->animation.offset_y = +7 * e->move.linear[3] + e->move.noise_angle[3];
            e->animation.offset_x = 0;
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

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->show5 = ((e->show1 + e->show2)) - e->show3;
            if (e->show5 > 255)
                e->show5 = 255;
            if (e->show5 < 0)
                e->show5 = 0;

            e->show6 = e->colordodge(e->show1, e->show2);

            e->pixel.red = e->show5 * radial;
            e->pixel.blue = (64 - e->show5 - e->show3) * radial;
            e->pixel.green = 0.5 * (e->show6);

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
