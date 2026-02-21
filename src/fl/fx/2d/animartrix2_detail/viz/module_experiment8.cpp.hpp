#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment8.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void Module_Experiment8::draw(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;

    float w = 0.3;

    e->timings.ratio[0] = 0.01;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.013;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;
    e->timings.ratio[5] = 0.38 * w;
    e->timings.ratio[6] = 0.0003;

    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.4;
            float r = 1.5;

            e->animation.dist =
                3 + e->distance[x][y] +
                3 * fl::sinf(0.25 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[0] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 10 * e->move.linear[0];
            e->animation.offset_y = -5 * r * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist =
                4 + e->distance[x][y] +
                4 * fl::sinf(0.24 * e->distance[x][y] - e->move.radial[4]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[1] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[1];
            e->animation.offset_y = -5 * r * e->move.linear[1];
            e->animation.offset_x = 100;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist =
                5 + e->distance[x][y] +
                5 * fl::sinf(0.23 * e->distance[x][y] - e->move.radial[5]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[2] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[2];
            e->animation.offset_y = -5 * r * e->move.linear[2];
            e->animation.offset_x = 1000;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->show4 = e->colordodge(e->show1, e->show2);

            float rad = fl::sinf(PI / 2 +
                             e->distance[x][y] / 14);

            e->pixel.red = rad * ((e->show1 + e->show2) + e->show3);
            e->pixel.green = (((e->show2 + e->show3) * 0.8) - 90) * rad;
            e->pixel.blue = e->show4 * 0.2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
