


#include <Arduino.h>

#include "./util.h"
#include "./color_mapper.h"
#include "./Keyboard.h"
#include "./dprint.h"

Key::Key() : on_(false), sustained_(false), sustain_pedal_on_(false),
             velocity_(0), idx_(0), event_time_(0) {}

void Key::SetOn(uint8_t vel, const ColorHSV& color, uint32_t now_ms) {
  if (curr_color_.v_ < color.v_) {  // if the new color is "brighter" than the current color.
    velocity_ = vel;
    curr_color_ = color;
    event_time_ = now_ms;
  }
  orig_color_ = curr_color_;
  event_time_ = now_ms;
  on_ = true;
}

void Key::SetOff(uint32_t now_ms) {
  orig_color_ = curr_color_;
  on_ = false;
  event_time_ = now_ms;
  sustained_ = false;
}

void Key::SetSustained() {
  sustained_ = true;
}

void Key::Update(uint32_t now_ms, uint32_t delta_ms, bool sustain_pedal_on) {
  if (sustained_ && !sustain_pedal_on) {
    sustained_ = false;
    SetOff(now_ms);
  }
  sustain_pedal_on_ = sustain_pedal_on;
  UpdateIntensity(now_ms, delta_ms);
}

float Key::VelocityFactor() const { return velocity_ / 127.f; }

float Key::CalcAttackDecayFactor(uint32_t delta_ms) const {
  bool dampened_key = (idx_ < kFirstNoteNoDamp);
  float active_lights_factor = ::CalcDecayFactor(
      sustain_pedal_on_,
      on_,
      idx_,
      VelocityFactor(),
      dampened_key,
      delta_ms);
  return active_lights_factor;
}

float Key::AttackRemapFactor(uint32_t now_ms) {
  if (on_) {
    return ::AttackRemapFactor(now_ms - event_time_);
  } else {
    return 1.0;
  }
}

float Key::IntensityFactor() const {
  return intensity_;
}

void Key::UpdateIntensity(uint32_t now_ms, uint32_t delta_ms) {
  if (on_) {
    // Intensity can be calculated by a 
    float intensity = 
        CalcAttackDecayFactor(now_ms - event_time_) *
        VelocityFactor() *
        AttackRemapFactor(now_ms);

    // This is FRAME RATE DEPENDENT FUNCTION!!!!
    // CHANGE TO TIME INDEPENDENT BEFORE SUBMIT.
    intensity_ = (.9f * intensity) + (.1f * intensity_);
  } else if(intensity_ > 0.0f) {  // major cpu hotspot.

    if (sustain_pedal_on_) {
      float delta_s = delta_ms / 1000.f;
      if (intensity_ > .5f) {
        const float kRate = .12f;
        // Time flexible decay function. Stays accurate
        // even as the frame rate changes.
        // Formula: A = Pe^(r*t)
        intensity_ = intensity_ * exp(-delta_s * kRate);
      } else {
        // Quickly fade at the bottom end of the transition.
        const float kRate = .05f;
        intensity_ -= delta_s * kRate;
      }
    } else {
      float delta_s = delta_ms / 1000.f;
      if (intensity_ > .5f) {
        const float kRate = 12.0f;
        // Time flexible decay function. Stays accurate
        // even as the frame rate changes.
        // Formula: A = Pe^(r*t)
        intensity_ = intensity_ * exp(-delta_s * kRate);
      } else {
        // Quickly fade at the bottom end of the transition.
        const float kRate = 2.0f;
        intensity_ -= delta_s * kRate;
      }

    }
    intensity_ = constrain(intensity_, 0.0f, 1.0f);
  }
}

void KeyboardState::HandleNoteOn(uint8_t midi_note, uint8_t velocity, int color_selector_value, uint32_t now_ms) {
  if (0 == velocity) {
    // Some keyboards signify "NoteOff" with a velocity of zero.
    HandleNoteOff(midi_note, velocity, now_ms);
    return;
  }

#ifdef DEBUG_KEYBOARD
  dprint("HandleNoteOn:");
  
  dprint("midi_note = ");
  dprint(midi_note);
  
  dprint(", velocity = ");
  dprintln(velocity);
 #endif
  
  float brightness = ToBrightness(velocity);

  dprint("brightness: "); dprintln(brightness);
    
  ColorHSV pixel_color_hsv = SelectColor(midi_note, brightness,
                                         color_selector_value);  
                                        
  // TODO: Give a key access to the Keyboard owner, therefore it could inspect the
  // sustained variable instead of passing it here. 
  Key* key = GetKey(midi_note);

  dprint("key indx: "); dprintln(key->idx_);
  
  key->SetOn(velocity, pixel_color_hsv, now_ms);
}

void KeyboardState::HandleNoteOff(uint8_t midi_note, uint8_t /*velocity*/, uint32_t now_ms) {
#ifdef DEBUG_KEYBOARD
  dprint("HandleNoteOff:");
  
  dprint("midi_note = ");
  dprint(midi_note);
  
  dprint(", velocity = ");
  dprintln(velocity);
#endif

  Key* key = GetKey(midi_note);

  if (sustain_pedal_) {
    key->SetSustained();
  } else {
    key->SetOff(now_ms);
  }
}

void KeyboardState::HandleControlChange(uint8_t d1, uint8_t d2) {
  // Note that d1 and d2 just mean "data-1" and "data-2".
  // TODO: Find out what d1 and d2 should be called.
  const bool foot_pedal = (d1 == kMidiFootPedal);
  
  if (foot_pedal) {
    // Spec says that if that values 0-63 are OFF, otherwise ON.
    sustain_pedal_ = (d2 >= 64);
  }
}

void KeyboardState::HandleAfterTouchPoly(uint8_t note, uint8_t pressure) { 
  dprintln("HandleAfterTouchPoly");

  dprint("\tnote = ");
  dprint(note);

  dprint(", pressure = ");
  dprintln(pressure);
}

KeyboardState::KeyboardState() : sustain_pedal_(false), keys_() {
  for (int i = 0; i < kNumKeys; ++i) {
    keys_[i].idx_ = i;
  }
}

void KeyboardState::Update(uint32_t now_ms, uint32_t delta_ms) {
  for (int i = 0; i < kNumKeys; ++i) {
    keys_[i].Update(now_ms, delta_ms, sustain_pedal_);
  }
}

uint8_t KeyboardState::KeyIndex(int midi_pitch) {
  //return constrain(midi_pitch, 21, 108) - 21;
  return ::KeyIndex(midi_pitch);
}

Key* KeyboardState::GetKey(int midi_pitch) {
  uint8_t idx = KeyIndex(midi_pitch);
  return &keys_[idx];
}



