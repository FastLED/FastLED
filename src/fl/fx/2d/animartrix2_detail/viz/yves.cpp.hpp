#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix2_detail/viz/yves.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void Yves::draw(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->a = fl::micros();

    e->timings.master_speed = 0.001;
    e->timings.ratio[0] = 3;
    e->timings.ratio[1] = 2;
    e->timings.ratio[2] = 1;
    e->timings.ratio[3] = 0.13;
    e->timings.ratio[4] = 0.15;
    e->timings.ratio[5] = 0.03;
    e->timings.ratio[6] = 0.025;
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
            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                e->polar_theta[x][y] + 2 * PI + e->move.noise_angle[5];
            e->animation.scale_x = 0.08;
            e->animation.scale_y = 0.08;
            e->animation.scale_z = 0.08;
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                e->polar_theta[x][y] + 2 * PI + e->move.noise_angle[6];
            ;
            e->animation.scale_x = 0.08;
            e->animation.scale_y = 0.08;
            e->animation.scale_z = 0.08;
            e->animation.offset_y = -e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + show1 / 100 +
                                 e->move.noise_angle[3] + e->move.noise_angle[4];
            e->animation.dist = e->distance[x][y] + show2 / 50;
            e->animation.offset_y = -e->move.linear[2];

            e->animation.offset_y += show1 / 100;
            e->animation.offset_x += show2 / 100;

            float show3 = e->render_value(e->animation);

            e->animation.offset_y = 0;
            e->animation.offset_x = 0;

            float show4 = e->render_value(e->animation);

            e->pixel.red = show3;
            e->pixel.green = show3 * show4 / 255;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
