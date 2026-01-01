#include <Arduino.h>

#include "./util.h"
#include "./color_mapper.h"
#include "./Keyboard.h"
#include "./dprint.h"
#include "fl/unused.h"
#include "fl/stl/math.h"

Key::Key() : mOn(false), mSustained(false), mSustainPedalOn(false),
             mVelocity(0), mIdx(0), mEventTime(0) {}

void Key::SetOn(uint8_t vel, const ColorHSV& color, uint32_t now_ms) {
  if (mCurrColor.v_ < color.v_) {  // if the new color is "brighter" than the current color.
    mVelocity = vel;
    mCurrColor = color;
    mEventTime = now_ms;
  }
  mOrigColor = mCurrColor;
  mEventTime = now_ms;
  mOn = true;
}

void Key::SetOff(uint32_t now_ms) {
  mOrigColor = mCurrColor;
  mOn = false;
  mEventTime = now_ms;
  mSustained = false;
}

void Key::SetSustained() {
  mSustained = true;
}

void Key::Update(uint32_t now_ms, uint32_t delta_ms, bool sustain_pedal_on) {
  if (mSustained && !sustain_pedal_on) {
    mSustained = false;
    SetOff(now_ms);
  }
  mSustainPedalOn = sustain_pedal_on;
  UpdateIntensity(now_ms, delta_ms);
}

float Key::VelocityFactor() const { return mVelocity / 127.f; }

float Key::CalcAttackDecayFactor(uint32_t delta_ms) const {
  bool dampened_key = (mIdx < kFirstNoteNoDamp);
  float active_lights_factor = ::CalcDecayFactor(
      mSustainPedalOn,
      mOn,
      mIdx,
      VelocityFactor(),
      dampened_key,
      delta_ms);
  return active_lights_factor;
}

float Key::AttackRemapFactor(uint32_t now_ms) {
  if (mOn) {
    return ::AttackRemapFactor(now_ms - mEventTime);
  } else {
    return 1.0;
  }
}

float Key::IntensityFactor() const {
  return mIntensity;
}

void Key::UpdateIntensity(uint32_t now_ms, uint32_t delta_ms) {
  if (mOn) {
    // Intensity can be calculated by a
    float intensity =
        CalcAttackDecayFactor(now_ms - mEventTime) *
        VelocityFactor() *
        AttackRemapFactor(now_ms);

    // This is FRAME RATE DEPENDENT FUNCTION!!!!
    // CHANGE TO TIME INDEPENDENT BEFORE SUBMIT.
    mIntensity = (.9f * intensity) + (.1f * mIntensity);
  } else if(mIntensity > 0.0f) {  // major cpu hotspot.

    if (mSustainPedalOn) {
      float delta_s = delta_ms / 1000.f;
      if (mIntensity > .5f) {
        const float kRate = .12f;
        // Time flexible decay function. Stays accurate
        // even as the frame rate changes.
        // Formula: A = Pe^(r*t)
        mIntensity = mIntensity * fl::expf(-delta_s * kRate);
      } else {
        // Quickly fade at the bottom end of the transition.
        const float kRate = .05f;
        mIntensity -= delta_s * kRate;
      }
    } else {
      float delta_s = delta_ms / 1000.f;
      if (mIntensity > .5f) {
        const float kRate = 12.0f;
        // Time flexible decay function. Stays accurate
        // even as the frame rate changes.
        // Formula: A = Pe^(r*t)
        mIntensity = mIntensity * fl::expf(-delta_s * kRate);
      } else {
        // Quickly fade at the bottom end of the transition.
        const float kRate = 2.0f;
        mIntensity -= delta_s * kRate;
      }

    }
    mIntensity = constrain(mIntensity, 0.0f, 1.0f);
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

  dprint("key indx: "); dprintln(key->mIdx);

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

  if (mSustainPedal) {
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
    mSustainPedal = (d2 >= 64);
  }
}

void KeyboardState::HandleAfterTouchPoly(uint8_t note, uint8_t pressure) { 
  FL_UNUSED(note);
  FL_UNUSED(pressure);
  
  dprintln("HandleAfterTouchPoly");

  dprint("\tnote = ");
  dprint(note);

  dprint(", pressure = ");
  dprintln(pressure);
}

KeyboardState::KeyboardState() : mSustainPedal(false), mKeys() {
  for (int i = 0; i < kNumKeys; ++i) {
    mKeys[i].mIdx = i;
  }
}

void KeyboardState::Update(uint32_t now_ms, uint32_t delta_ms) {
  for (int i = 0; i < kNumKeys; ++i) {
    mKeys[i].Update(now_ms, delta_ms, mSustainPedal);
  }
}

uint8_t KeyboardState::KeyIndex(int midi_pitch) {
  //return constrain(midi_pitch, 21, 108) - 21;
  return ::KeyIndex(midi_pitch);
}

Key* KeyboardState::GetKey(int midi_pitch) {
  uint8_t idx = KeyIndex(midi_pitch);
  return &mKeys[idx];
}
