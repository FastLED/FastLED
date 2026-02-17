#include "fl/fx/2d/animartrix2_detail/viz/rotating_blob.h"

namespace fl {

void Rotating_Blob(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.1;
    e->timings.ratio[1] = 0.03;
    e->timings.ratio[2] = 0.03;
    e->timings.ratio[3] = 0.03;

    e->timings.offset[1] = 10;
    e->timings.offset[2] = 20;
    e->timings.offset[3] = 30;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.offset_z = 100;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.dist = e->distance[x][y];
            e->animation.z = e->move.linear[0];
            e->animation.low_limit = -1;
            float show1 = e->render_value(e->animation);

            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[1] + show1 / 512.0;
            e->animation.dist = e->distance[x][y] * show1 / 255.0;
            e->animation.low_limit = 0;
            e->animation.z = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[2] + show1 / 512.0;
            e->animation.dist = e->distance[x][y] * show1 / 220.0;
            e->animation.z = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[3] + show1 / 512.0;
            e->animation.dist = e->distance[x][y] * show1 / 200.0;
            e->animation.z = e->move.linear[3];
            float show4 = e->render_value(e->animation);

            e->pixel.red = (show2 + show4) / 2;
            e->pixel.green = show3 / 6;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
