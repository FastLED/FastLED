#include "../shared/defs.h"


#if ENABLE_SKETCH


#include "./ui_state.h"
#include "shared/Painter.h"
#include "fl/dbg.h"
#include "fl/unused.h"

#include <Arduino.h>

//#define UI_V1  // Based off the old midi shield with hand assmebled buttons.
#define UI_V2  // Based on a new midi shield with buttons. https://learn.sparkfun.com/tutorials/midi-shield-hookup-guide
#define UI_DBG


#ifndef A3
#define A3 3
#warning "A3 is not defined, using 3"
#endif
#ifndef A4
#define A4 4
#warning "A4 is not defined, using 4"
#endif

#ifdef __STM32F1__
// Missing A-type pins, just use digital pins mapped to analog.
#define PIN_POT_COLOR_SENSOR D3
#define PIN_POT_VEL_SENSOR D4
#else
#define PIN_POT_COLOR_SENSOR A3
#define PIN_POT_VEL_SENSOR A4
#endif

#define PIN_VIS_SELECT 2
#define PIN_COLOR_SELECT 4

Potentiometer velocity_pot(PIN_POT_VEL_SENSOR);
Potentiometer color_pot(PIN_POT_COLOR_SENSOR);

float read_color_selector() {
  return color_pot.Read();
}

float read_velocity_bias() {
  return velocity_pot.Read();
}


//DigitalButton custom_notecolor_select(4);
ColorSelector color_selector(PIN_COLOR_SELECT);
CountingButton vis_selector(PIN_VIS_SELECT);

void ui_init() {
}


ui_state ui_update(uint32_t now_ms, uint32_t delta_ms) {
  FL_UNUSED(delta_ms);
  
  ui_state out;
  vis_selector.Update(now_ms);
  color_selector.Update();
  int32_t curr_val = vis_selector.curr_val();
  FASTLED_DBG("curr_val: " << curr_val);

  out.color_scheme = color_selector.curr_val();

  //bool notecolor_on = custom_notecolor_select.Read();

  //Serial.print("color selector: "); Serial.println(out.color_scheme);

  //float velocity = read_velocity_bias();
  //float color_selector = read_color_selector();

  //Serial.print("velocity: "); Serial.print(velocity); Serial.print(", color_selector: "); Serial.print(color_selector);

  //if (notecolor_on) {
  //  Serial.print(", notecolor_on: "); Serial.print(notecolor_on);
  //}

  //Serial.print("color_scheme: "); Serial.print(out.color_scheme); Serial.print(", vis selector: "); Serial.print(curr_val);

  //Serial.println("");

  //Serial.print("curr_val: "); Serial.print(curr_val);// Serial.print(", button state: "); Serial.println(vis_selector.button_.curr_val());;

  out.which_visualizer = static_cast<Painter::VisState>(curr_val % Painter::kNumVisStates);
  return out;
}


#endif  // ENABLE_SKETCH
