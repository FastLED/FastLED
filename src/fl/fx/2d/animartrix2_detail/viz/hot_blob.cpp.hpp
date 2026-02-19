#include "fl/fx/2d/animartrix2_detail/viz/hot_blob.h"

namespace fl {

void Hot_Blob::draw(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();
    e->run_default_oscillators(0.001);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];

            e->animation.scale_x = 0.07 + e->move.directional[0] * 0.002;
            e->animation.scale_y = 0.07;

            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;

            e->animation.z = 0;
            e->animation.low_limit = -1;
            float show1 = e->render_value(e->animation);

            e->animation.offset_y = -e->move.linear[1];
            float show3 = e->render_value(e->animation);

            e->animation.offset_x = show3 / 20;
            e->animation.offset_y = -e->move.linear[0] / 2 + show1 / 70;
            e->animation.low_limit = 0;
            float show2 = e->render_value(e->animation);

            e->animation.offset_x = show3 / 20;
            e->animation.offset_y = -e->move.linear[0] / 2 + show1 / 70;
            e->animation.z = 100;
            float show4 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->animation.dist) / e->animation.dist;

            float linear = (y + 1) / (e->num_y - 1.f);

            e->pixel.red = radial * show2;
            e->pixel.green = linear * radial * 0.3 * (show2 - show4);
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
