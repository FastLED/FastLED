
#include <Arduino.h>

#include "./util.h"

#include "ApproximatingFunction.h"
#include "settings.h"

/*
// C - 0, C# - 1, D - 2, D# - 3... B - 11.
// http://cote.cc/w/wp-content/uploads/drupal/blog/logic-midi-note-numbers.png
*/
uint8_t FundamentalNote(int midi_note) {
  return midi_note % 12;
}

float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Given an input time. 
float AttackRemapFactor(uint32_t delta_t_ms) {
  typedef InterpData<uint32_t, float> Datum;
  static const Datum kData[] = {
    Datum(0,  .5),
    Datum(80,  1.0),
  };

  static const int n = sizeof(kData) / sizeof(kData[0]);
  return Interp(delta_t_ms, kData, n);
}

float MapDecayTime(uint8_t key_idx) {
  typedef InterpData<uint8_t, float> Datum;
  static const float bias = 1.3f;
  // key then time for decay in milliseconds.
  // First value is the KEY on the keyboard, second value is the
  // time. The KEY must be IN ORDER or else the algorithm will fail.
  static const Datum kInterpData[] = {
      Datum(0, 21.0f * 1000.0f * bias),
      Datum(11, 19.4 * 1000.0f * bias),
      Datum(22, 15.1f * 1000.0f * bias),
      Datum(35, 12.5f * 1000.0f * bias),
      Datum(44, 10.f * 1000.0f * bias),
      Datum(50, 8.1f * 1000.0f * bias),
      Datum(53, 5.3f * 1000.0f * bias),
      Datum(61, 4.0f * 1000.0f * bias),
      Datum(66, 5.0f * 1000.0f * bias),
      Datum(69, 4.6f * 1000.0f * bias),
      Datum(70, 4.4f * 1000.0f * bias),
      Datum(71, 4.3f * 1000.0f * bias),
      Datum(74, 3.9f * 1000.0f * bias),
      Datum(80, 1.9f * 1000.0f * bias),
      Datum(81, 1.8f * 1000.0f * bias),
      Datum(82, 1.7f * 1000.0f * bias),
      Datum(83, 1.5f * 1000.0f * bias),
      Datum(84, 1.3f * 1000.0f * bias),
      Datum(86, 1.0f * 1000.0f * bias),
      Datum(87, 0.9f * 1000.0f * bias),
  };
  
  static const int n = sizeof(kInterpData) / sizeof(kInterpData[0]);    
  float approx_val = Interp(key_idx, kInterpData, n);    
  return approx_val;
}

// Returns a value in the range 1->0 indicating how intense the note is. This
// value will go to 0 as time progresses, and will be 1 when the note is first
// pressed.
float CalcDecayFactor(bool sustain_pedal_on,
                      bool key_on,
                      int key_idx,
                      float velocity,
                      bool dampened_key,
                      float time_elapsed_ms) {
    
  static const float kDefaultDecayTime = .2f * 1000.f;
  static const float kBias = 1.10;
  float decay_time = kDefaultDecayTime;  // default - no sustain.
  if (key_on || sustain_pedal_on || !dampened_key) {
    decay_time = MapDecayTime(key_idx) * max(0.25f, velocity);
  }
  // decay_interp is a value which starts off as 1.0 to signify the start of the
  // key press and gradually decreases to 0.0. For example, if the decay time is 1 second
  // then at the time = 0s, decay_interp is 1.0, and after one second decay_interp is 0.0
  float intensity_factor = mapf(time_elapsed_ms,
                                0.0, decay_time * kBias,
                                1.0, 0.0);  // Startup at full brightness -> no brighness.
                         

  // When decay_interp reaches 0, the lighting sequence is effectively finished. However
  // because this is time based and time keeps on going this value will move into negative
  // territory, we take care of this by simply clamping all negative values to 0.0.
  intensity_factor = constrain(intensity_factor, 0.0f, 1.0f);
  return intensity_factor;
}

float ToBrightness(int velocity) {
  typedef InterpData<int, float> Datum; 
  static const Datum kData[] = {
    Datum(0,   0.02),
    Datum(32,  0.02),
    Datum(64,  0.10),
    Datum(80,  0.30),
    Datum(90,  0.90),
    Datum(100, 1.00),
    Datum(120, 1.00),
    Datum(127, 1.00)
  };
  
  static const int n = sizeof(kData) / sizeof(kData[0]);    
  const float val = Interp(velocity, kData, n);
  
  return val;
}
