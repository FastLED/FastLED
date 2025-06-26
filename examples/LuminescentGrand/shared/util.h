#ifndef UTIL_H_
#define UTIL_H_

#include <stdint.h>

#include "./ApproximatingFunction.h"
#include "./settings.h"

#ifndef round
#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))
#endif  // round

/*
// C - 0, C# - 1, D - 2, D# - 3... B - 11.
// http://cote.cc/w/wp-content/uploads/drupal/blog/logic-midi-note-numbers.png
*/
uint8_t FundamentalNote(int midi_note);

float mapf(float x, float in_min, float in_max, float out_min, float out_max);

// Given an input time. 
float AttackRemapFactor(uint32_t delta_t_ms);

float MapDecayTime(uint8_t key_idx);

// Returns a value in the range 1->0 indicating how intense the note is. This
// value will go to 0 as time progresses, and will be 1 when the note is first
// pressed.
float CalcDecayFactor(bool sustain_pedal_on,
                      bool key_on,
                      int key_idx,
                      float velocity,
                      bool dampened_key,
                      float time_elapsed_ms);

float ToBrightness(int velocity);

#endif  // UTIL_H_
