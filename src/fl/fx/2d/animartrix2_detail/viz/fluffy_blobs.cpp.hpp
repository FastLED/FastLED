#include "fl/fx/2d/animartrix2_detail/viz/fluffy_blobs.h"

namespace fl {

void Fluffy_Blobs::draw(Context &ctx) {
    auto *e = ctx.mEngine;

    e->timings.master_speed = 0.015;
    float size = 0.15;
    float radial_speed = 1;
    float linear_speed = 5;

    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.026;
    e->timings.ratio[2] = 0.027;
    e->timings.ratio[3] = 0.028;
    e->timings.ratio[4] = 0.029;
    e->timings.ratio[5] = 0.030;
    e->timings.ratio[6] = 0.031;
    e->timings.ratio[7] = 0.032;
    e->timings.ratio[8] = 0.033;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[0]);
            e->animation.z = 5;
            e->animation.scale_x = size;
            e->animation.scale_y = size;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = linear_speed * e->move.linear[0];
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[1]);
            e->animation.offset_y = linear_speed * e->move.linear[1];
            e->animation.offset_z = 200;
            e->animation.scale_x = size * 1.1;
            e->animation.scale_y = size * 1.1;
            e->show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[2]);
            e->animation.offset_y = linear_speed * e->move.linear[2];
            e->animation.offset_z = 400;
            e->animation.scale_x = size * 1.2;
            e->animation.scale_y = size * 1.2;
            e->show3 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[3]);
            e->animation.offset_y = linear_speed * e->move.linear[3];
            e->animation.offset_z = 600;
            e->animation.scale_x = size;
            e->animation.scale_y = size;
            e->show4 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[4]);
            e->animation.offset_y = linear_speed * e->move.linear[4];
            e->animation.offset_z = 800;
            e->animation.scale_x = size * 1.1;
            e->animation.scale_y = size * 1.1;
            e->show5 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[5]);
            e->animation.offset_y = linear_speed * e->move.linear[5];
            e->animation.offset_z = 1800;
            e->animation.scale_x = size * 1.2;
            e->animation.scale_y = size * 1.2;
            e->show6 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[6]);
            e->animation.offset_y = linear_speed * e->move.linear[6];
            e->animation.offset_z = 2800;
            e->animation.scale_x = size;
            e->animation.scale_y = size;
            e->show7 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[7]);
            e->animation.offset_y = linear_speed * e->move.linear[7];
            e->animation.offset_z = 3800;
            e->animation.scale_x = size * 1.1;
            e->animation.scale_y = size * 1.1;
            e->show8 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[8]);
            e->animation.offset_y = linear_speed * e->move.linear[8];
            e->animation.offset_z = 4800;
            e->animation.scale_x = size * 1.2;
            e->animation.scale_y = size * 1.2;
            e->show9 = e->render_value(e->animation);

            e->pixel.red = 0.8 * (e->show1 + e->show2 + e->show3) + (e->show4 + e->show5 + e->show6);
            e->pixel.green = 0.8 * (e->show4 + e->show5 + e->show6);
            e->pixel.blue = 0.3 * (e->show7 + e->show8 + e->show9);

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

}  // namespace fl
