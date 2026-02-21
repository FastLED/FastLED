#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix5.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void SpiralMatrix5::draw(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y] * (e->move.directional[0]);
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[1];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[2];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[2];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (e->move.directional[3]);
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[3];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[3];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[4];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[4];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[4];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show5 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[5];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[5];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[5];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show6 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * e->add(show1, show4);
            e->pixel.green = radial * e->colordodge(show2, show5);
            e->pixel.blue = radial * e->screen(show3, show6);

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
