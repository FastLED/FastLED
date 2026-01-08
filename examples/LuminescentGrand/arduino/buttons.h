#pragma once

#include <Arduino.h>
#include "fl/ui.h"
#include "fl/dbg.h"

using namespace fl;

// Done by hand. Old school.
class ToggleButton {
 public:
  ToggleButton(int pin) : mPin(pin), mOn(false), mDebounceTimestamp(0), mChanged(false) {
    pinMode(mPin, OUTPUT);
    digitalWrite(mPin, LOW);
    delay(1);
  }

  // true - button is pressed.
  bool Read() {
    Update(fl::millis());
    return mChanged;
  }

  void Update(uint32_t time_now) {
    if ((time_now - mDebounceTimestamp) < 150) {
      mChanged = false;
      return;
    }

    int val = Read_Internal();
    mChanged = mOn != val;

    if (mChanged) {  // Has the toggle switch changed?
      mOn = val;       // Set the new toggle switch value.
      // Protect against debouncing artifacts by putting a 200ms delay from the
      // last time the value changed.
      if ((time_now - mDebounceTimestamp) > 150) {
        //++curr_val_;     // ... and increment the value.
        mDebounceTimestamp = time_now;
      }
    }
  }

 private:
  bool Read_Internal() {
    // Toggle the pin back to INPUT and take a reading.
    pinMode(mPin, INPUT);
    bool on = (digitalRead(mPin) == HIGH);
    // Switch the pin back to output so that we can enable the
    // pulldown resister.
    pinMode(mPin, OUTPUT);
    digitalWrite(mPin, LOW);
    return on;
  }


  int mPin;
  bool mOn;
  uint32_t mDebounceTimestamp;
  bool mChanged;
};

// This is the new type that is built into the midi shield.
class MidiShieldButton {
 public:
  MidiShieldButton(int pin) : mPin(pin) {
    pinMode(mPin, INPUT_PULLUP);
    delay(1);
  }

  bool Read() {
    // Toggle the pin back to INPUT and take a reading.
    int val = digitalRead(mPin) == LOW;

    return val;
  }
 private:
  int mPin;
};

class Potentiometer {
 public:
  Potentiometer(int sensor_pin) : mSensorPin(sensor_pin) {}
  float Read() {
    float avg = 0.0;
    // Filter by reading the value multiple times and taking
    // the average.
    for (int i = 0; i < 8; ++i) {
      avg += analogRead(mSensorPin);
    }
    avg = avg / 8.0f;
    return avg;
  }
 private:
  int mSensorPin;
};

typedef MidiShieldButton DigitalButton;


class CountingButton {
 public:
  explicit CountingButton(int but_pin) : mButton(but_pin), mCurrVal(0), mUIButton("Counting UIButton") {
    mDebounceTimestamp = fl::millis();
    mOn = Read();
  }

  void Update(uint32_t time_now) {
    bool clicked = mUIButton.clicked();
    bool val = Read() || mUIButton.clicked();
    bool changed = val != mOn;

    if (clicked) {
      ++mCurrVal;
      mDebounceTimestamp = time_now;
      return;
    }

    if (changed) {  // Has the toggle switch changed?
      mOn = val;       // Set the new toggle switch value.
      // Protect against debouncing artifacts by putting a 200ms delay from the
      // last time the value changed.
      if ((time_now - mDebounceTimestamp) > 16) {
        if (mOn) {
          ++mCurrVal;     // ... and increment the value.
        }
        mDebounceTimestamp = time_now;
      }
    }
  }

  int curr_val() const { return mCurrVal; }

 private:
  bool Read() {
  	return mButton.Read();
  }

  DigitalButton mButton;
  bool mOn;
  int mCurrVal;
  unsigned long mDebounceTimestamp;
  UIButton mUIButton;
};

class ColorSelector {
 public:
  ColorSelector(int sensor_pin) : mBut(sensor_pin) {}

  void Update() {
  	mBut.Update(fl::millis());
  }

  int curr_val() const { return mBut.curr_val() % 7; }
 private:
  CountingButton mBut;
};