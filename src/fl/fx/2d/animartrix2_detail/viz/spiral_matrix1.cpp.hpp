#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix1.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

void SpiralMatrix1::draw(Context &ctx) {
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

    for (int x = 0; x < e->num_x / 2; x++) {
        for (int y = 0; y < e->num_y / 2; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 50 * e->move.linear[0];
            e->animation.offset_x = 150 * e->move.directional[0];
            e->animation.offset_y = 150 * e->move.directional[1];
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 4 * e->move.noise_angle[1];
            e->animation.z = 15;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_z = 50 * e->move.linear[1];
            e->animation.offset_x = 150 * e->move.directional[1];
            e->animation.offset_y = 150 * e->move.directional[2];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[2];
            e->animation.z = 25;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 50 * e->move.linear[2];
            e->animation.offset_x = 150 * e->move.directional[2];
            e->animation.offset_y = 150 * e->move.directional[3];
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[3];
            e->animation.z = 35;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_z = 50 * e->move.linear[3];
            e->animation.offset_x = 150 * e->move.directional[3];
            e->animation.offset_y = 150 * e->move.directional[4];
            float show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[4];
            e->animation.z = 45;
            e->animation.scale_x = 0.2;
            e->animation.scale_y = 0.2;
            e->animation.offset_z = 50 * e->move.linear[4];
            e->animation.offset_x = 150 * e->move.directional[4];
            e->animation.offset_y = 150 * e->move.directional[5];
            float show5 = e->render_value(e->animation);

            e->pixel.red = show1 + show2;
            e->pixel.green = show3 + show4;
            e->pixel.blue = show5;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);

            e->setPixelColorInternal((e->num_x - 1) - x, y, e->pixel);
            e->setPixelColorInternal((e->num_x - 1) - x, (e->num_y - 1) - y, e->pixel);
            e->setPixelColorInternal(x, (e->num_y - 1) - y, e->pixel);
        }
    }
}

}  // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
