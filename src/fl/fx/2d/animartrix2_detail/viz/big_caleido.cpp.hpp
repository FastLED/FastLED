#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix2_detail/viz/big_caleido.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void Big_Caleido::draw(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] +
                                 5 * e->move.noise_angle[0] +
                                 e->animation.dist * 0.1;
            e->animation.z = 5;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 50 * e->move.linear[0];
            e->animation.offset_x = 50 * e->move.noise_angle[0];
            e->animation.offset_y = 50 * e->move.noise_angle[1];
            float show1 = e->render_value(e->animation);

            e->animation.angle = 6 * e->polar_theta[x][y] +
                                 5 * e->move.noise_angle[1] +
                                 e->animation.dist * 0.15;
            e->animation.z = 5;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 50 * e->move.linear[1];
            e->animation.offset_x = 50 * e->move.noise_angle[1];
            e->animation.offset_y = 50 * e->move.noise_angle[2];
            float show2 = e->render_value(e->animation);

            e->animation.angle = 5;
            e->animation.z = 5;
            e->animation.scale_x = 0.10;
            e->animation.scale_y = 0.10;
            e->animation.offset_z = 10 * e->move.linear[2];
            e->animation.offset_x = 10 * e->move.noise_angle[2];
            e->animation.offset_y = 10 * e->move.noise_angle[3];
            float show3 = e->render_value(e->animation);

            e->animation.angle = 15;
            e->animation.z = 15;
            e->animation.scale_x = 0.10;
            e->animation.scale_y = 0.10;
            e->animation.offset_z = 10 * e->move.linear[3];
            e->animation.offset_x = 10 * e->move.noise_angle[3];
            e->animation.offset_y = 10 * e->move.noise_angle[4];
            float show4 = e->render_value(e->animation);

            e->animation.angle = 2;
            e->animation.z = 15;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_z = 10 * e->move.linear[4];
            e->animation.offset_x = 10 * e->move.noise_angle[4];
            e->animation.offset_y = 10 * e->move.noise_angle[5];
            float show5 = e->render_value(e->animation);

            e->pixel.red = show1 - show4;
            e->pixel.green = show2 - show5;
            e->pixel.blue = show3 - show2 + show1;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
