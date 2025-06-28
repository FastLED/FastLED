/// This is a work in progress showcasing a complex MIDI keyboard
/// visualizer.
/// To run this compiler use
/// > pip install fastled
/// Then go to your sketch directory and run
/// > fastled

#include "shared/defs.h"

#if !ENABLE_SKETCH
// avr can't compile this, neither can the esp8266.
void setup() {}
void loop() {}
#else


//#define DEBUG_PAINTER
//#define DEBUG_KEYBOARD 1

// Repeated keyboard presses in the main loop
#define DEBUG_FORCED_KEYBOARD
// #define DEBUG_MIDI_KEY 72

#define MIDI_SERIAL_PORT Serial1

#define FASTLED_UI  // Brings in the UI components.
#include "FastLED.h"

// H
#include <Arduino.h>
#include "shared/Keyboard.h"
#include "shared/color.h"
#include "shared/led_layout_array.h"
#include "shared/Keyboard.h"
#include "shared/Painter.h"
#include "shared/settings.h"
#include "arduino/LedRopeTCL.h"
#include "arduino/ui_state.h"
#include "shared/dprint.h"
#include "fl/dbg.h"
#include "fl/ui.h"
#include "fl/unused.h"

// Spoof the midi library so it thinks it's running on an arduino.
//#ifndef ARDUINO
//#define ARDUINO 1
//#endif

#ifdef MIDI_AUTO_INSTANCIATE
#undef MIDI_AUTO_INSTANCIATE
#define MIDI_AUTO_INSTANCIATE 0
#endif

#include "arduino/MIDI.h"


MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MY_MIDI);


FASTLED_TITLE("Luminescent Grand");
FASTLED_DESCRIPTION("A midi keyboard visualizer.");


/////////////////////////////////////////////////////////
// Light rope and keyboard.
LedRopeTCL led_rope(kNumKeys);
KeyboardState keyboard;


////////////////////////////////////
// Called when the note is pressed.
// Input:
//  channel - Ignored.
//  midi_note - Value between 21-108 which maps to the keyboard keys.
//  velocity - Value between 0-127
void HandleNoteOn(byte channel, byte midi_note, byte velocity) {
  FL_UNUSED(channel);
  FASTLED_DBG("HandleNoteOn: midi_note = " << int(midi_note) << ", velocity = " << int(velocity));
  keyboard.HandleNoteOn(midi_note, velocity, color_selector.curr_val(), millis());
}

/////////////////////////////////////////////////////////
// Called when the note is released.
// Input:
//  channel - Ignored.
//  midi_note - Value between 21-108 which maps to the keyboard keys.
//  velocity - Value between 0-127
void HandleNoteOff(byte channel, byte midi_note, byte velocity) {
  FL_UNUSED(channel);
  FASTLED_DBG("HandleNoteOn: midi_note = " << int(midi_note) << ", velocity = " << int(velocity));
  keyboard.HandleNoteOff(midi_note, velocity, millis());
}

/////////////////////////////////////////////////////////
// This is uninmplemented because the test keyboard didn't
// have this functionality. Right now the only thing it does is
// print out that the key was pressed.
void HandleAfterTouchPoly(byte channel, byte note, byte pressure) { 
  FL_UNUSED(channel);
  keyboard.HandleAfterTouchPoly(note, pressure);
}


/////////////////////////////////////////////////////////
// Detects whether the foot pedal has been touched.
void HandleControlChange(byte channel, byte d1, byte d2) {
  FL_UNUSED(channel);
  keyboard.HandleControlChange(d1, d2);
}

void HandleAfterTouchChannel(byte channel, byte pressure) {
  FL_UNUSED(channel);
  FL_UNUSED(pressure);
  #if 0  // Disabled for now.
  if (0 == pressure) {
    for (int i = 0; i < kNumKeys; ++i) {
      Key& key = keyboard.keys_[i];
      key.SetOff();
    }
  }
  #endif
}



/////////////////////////////////////////////////////////
// Called once when the app starts.
void setup() {
  FASTLED_DBG("setup");
  // Serial port for logging.
  Serial.begin(57600);
  //start serial with midi baudrate 31250
  // Initiate MIDI communications, listen to all channels
  MY_MIDI.begin(MIDI_CHANNEL_OMNI);
  
  // Connect the HandleNoteOn function to the library, so it is called upon reception of a NoteOn.
  MY_MIDI.setHandleNoteOn(HandleNoteOn);
  MY_MIDI.setHandleNoteOff(HandleNoteOff);
  MY_MIDI.setHandleAfterTouchPoly(HandleAfterTouchPoly);
  MY_MIDI.setHandleAfterTouchChannel(HandleAfterTouchChannel);
  MY_MIDI.setHandleControlChange(HandleControlChange);

  ui_init();
}

void DbgDoSimulatedKeyboardPress() {
#ifdef DEBUG_FORCED_KEYBOARD
  static uint32_t start_time = 0;
  static bool toggle = 0;

  const uint32_t time_on = 25;
  const uint32_t time_off = 500;

  // Just force it on whenever this function is called.
  is_debugging = true;

  uint32_t now = millis();
  uint32_t delta_time = now - start_time;


  uint32_t threshold_time = toggle ? time_off : time_on;
  if (delta_time < threshold_time) {
    return;
  }

  int random_key = random(0, 88);

  start_time = now;
  if (toggle) {
    HandleNoteOn(0, random_key, 64);
  } else {
    HandleNoteOff(0, random_key, 82);
  }
  toggle = !toggle;
#endif
}

/////////////////////////////////////////////////////////;p
// Repeatedly called by the app.
void loop() {
  //FASTLED_DBG("loop");


  // Calculate dt.
  static uint32_t s_prev_time = 0;
  uint32_t prev_time = 0;
  FASTLED_UNUSED(prev_time);  // actually used in perf tests.
  uint32_t now_ms = millis();
  uint32_t delta_ms = now_ms - s_prev_time;
  s_prev_time = now_ms;

  if (!is_debugging) {
    if (Serial.available() > 0) {
      int v = Serial.read();
      if (v == 'd') {
        is_debugging = true;
      }
    }
  }

  DbgDoSimulatedKeyboardPress();
  
  const unsigned long start_time = millis();
  // Each frame we call the midi processor 100 times to make sure that all notes
  // are processed.
  for (int i = 0; i < 100; ++i) {
    MY_MIDI.read();
  }
 
  const unsigned long midi_time = millis() - start_time;
  
  // Updates keyboard: releases sustained keys that.

  const uint32_t keyboard_time_start = millis();

  // This is kind of a hack... but give the keyboard a future time
  // so that all keys just pressed get a value > 0 for their time
  // durations.
  keyboard.Update(now_ms + delta_ms, delta_ms);
  const uint32_t keyboard_delta_time = millis() - keyboard_time_start;
  
  ui_state ui_st = ui_update(now_ms, delta_ms);

  //dprint("vis selector = ");
  //dprintln(vis_state);
  
  
  // These int values are for desting the performance of the
  // app. If the app ever runs slow then set kShowFps to 1
  // in the settings.h file.
  const unsigned long start_painting = millis();
  FASTLED_UNUSED(start_painting);


  // Paints the keyboard using the led_rope.
  Painter::VisState which_vis = Painter::VisState(ui_st.which_visualizer);
  Painter::Paint(now_ms, delta_ms, which_vis, &keyboard, &led_rope);

  const unsigned long paint_time = millis() - start_time;
  const unsigned long total_time = midi_time + paint_time + keyboard_delta_time;
  
  if (kShowFps) {
    float fps = 1.0f/(float(total_time) / 1000.f);
    Serial.print("fps                  - "); Serial.println(fps);
    Serial.print("midi time            - "); Serial.println(midi_time);
    Serial.print("keyboard update time - "); Serial.println(keyboard_delta_time);
    Serial.print("draw & paint time    - "); Serial.println(paint_time);
  }

  EVERY_N_SECONDS(1) {
    FASTLED_DBG("is_debugging = " << is_debugging);
  }


  FastLED.show();
}


#endif  // __AVR__
