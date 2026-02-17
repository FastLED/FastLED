#include "fl/fx/2d/animartrix2_detail/viz/center_field.h"

namespace fl {

void Center_Field(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 1;
    e->timings.ratio[1] = 1.1;
    e->timings.ratio[2] = 1.2;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.dist = 5 * fl::sqrtf(e->distance[x][y]);
            e->animation.offset_y = e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.dist = 4 * fl::sqrtf(e->distance[x][y]);
            e->animation.offset_y = e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.z = 0;
            float show2 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show2;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
