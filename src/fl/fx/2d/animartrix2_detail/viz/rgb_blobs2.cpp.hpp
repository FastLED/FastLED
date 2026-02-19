#include "fl/fx/2d/animartrix2_detail/viz/rgb_blobs2.h"

namespace fl {

void RGB_Blobs2::draw(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.12;
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
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3] +
                                 e->move.noise_angle[1];
            e->animation.z = (fl::sqrtf(e->animation.dist));
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 10;
            e->animation.offset_x = 10 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4] +
                                 e->move.noise_angle[2];
            e->animation.offset_x = 11 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5] +
                                 e->move.noise_angle[3];
            e->animation.offset_x = 12 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (show1 - show3);
            e->pixel.green = radial * (show2 - show1);
            e->pixel.blue = radial * (show3 - show2);

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
