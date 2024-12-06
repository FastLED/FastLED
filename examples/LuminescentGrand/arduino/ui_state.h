
#pragma once

#include <Arduino.h>
#include "buttons.h"


extern ColorSelector color_selector;
extern CountingButton vis_selector;

struct ui_state {
  ui_state() { memset(this, 0, sizeof(*this)); }
  int which_visualizer;
  int color_scheme;
  //float color_wheel_pos;
  //float velocity_bias_pos;  // default to 1.0f for buttons missing velocity.
};

void ui_init();
ui_state ui_update(uint32_t now_ms, uint32_t delta_ms);

