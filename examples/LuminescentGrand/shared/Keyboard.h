
#ifndef KEYBOARD_H_
#define KEYBOARD_H_

#include <Arduino.h>
#include "color.h"
#include "./util.h"

class KeyboardState;

//   // NOTE: AS OF NOV-12-2013 we've disable all of the auto-sustained
//  notes in the high end of the keyboard.
enum {
  // kFirstNoteNoDamp = 69,  // First key that has no dampener
  kFirstNoteNoDamp = 89,  // DISABLED - Greater than last key.
};

inline uint8_t KeyIndex(int midi_pitch) {
  return constrain(midi_pitch, 21, 108) - 21;
}

struct Key {
  Key();
  void SetOn(uint8_t vel, const ColorHSV& color, uint32_t now_ms);
  void SetOff(uint32_t now_ms);
  void SetSustained();

  void Update(uint32_t now_ms, uint32_t delta_ms, bool sustain_pedal_on);

  float VelocityFactor() const;
  float CalcAttackDecayFactor(uint32_t delta_ms) const;
  float AttackRemapFactor(uint32_t now_ms);
  float IntensityFactor() const;
  void UpdateIntensity(uint32_t now_ms, uint32_t delta_ms);

  bool on_;  // Max number of MIDI keys.
  bool sustained_;
  bool sustain_pedal_on_;
  uint8_t velocity_;
  int idx_;
  unsigned long event_time_;

  // 0.0 -> 1.0 How intense the key is, used for light sequences to represent
  // 0 -> 0% of lights on to 1.0 -> 100% of lights on. this is a smooth
  // value through time.
  float intensity_;
  ColorHSV orig_color_;
  ColorHSV curr_color_;
};

// Interface into the Keyboard state.
// Convenience class which holds all the keys in the keyboard. Also
// has a convenience function will allows one to map the midi notes
// (21-108) to the midi keys (0-88).
class KeyboardState {
 public:
 
  // NOTE: AS OF NOV-12-2013 we've disable all of the auto-sustained
  // notes in the high end of the keyboard.
  //enum {
    // kFirstNoteNoDamp = 69,  // First key that has no dampener
  //  kFirstNoteNoDamp = 89,  // DISABLED - Greater than last key.
  //};

  KeyboardState();
  void Update(uint32_t now_ms, uint32_t delta_ms);


  ////////////////////////////////////
  // Called when the note is pressed.
  // Input:
  //  channel - Ignored.
  //  midi_note - Value between 21-108 which maps to the keyboard keys.
  //  velocity - Value between 0-127
  void HandleNoteOn(uint8_t midi_note, uint8_t velocity, int color_selector_value, uint32_t now_ms);

  /////////////////////////////////////////////////////////
  // Called when the note is released.
  // Input:
  //  channel - Ignored.
  //  midi_note - Value between 21-108 which maps to the keyboard keys.
  //  velocity - Value between 0-127
  void HandleNoteOff(uint8_t midi_note, uint8_t velocity, uint32_t now_ms);

  /////////////////////////////////////////////////////////
  // This is uninmplemented because the test keyboard didn't
  // have this functionality. Right now the only thing it does is
  // print out that the key was pressed.
  void HandleAfterTouchPoly(uint8_t note, uint8_t pressure);


  /////////////////////////////////////////////////////////
  // Detects whether the foot pedal has been touched.
  void HandleControlChange(uint8_t d1, uint8_t d2);


  static uint8_t KeyIndex(int midi_pitch);
 
  Key* GetKey(int midi_pitch);
 
  static const int kNumKeys = 88;
  bool sustain_pedal_;
  Key keys_[kNumKeys];
};


#endif  // KEYBOARD_H_

