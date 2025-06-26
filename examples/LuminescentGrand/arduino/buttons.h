#pragma once

#include <Arduino.h>
#include "fl/ui.h"
#include "fl/dbg.h"

using namespace fl;

// Done by hand. Old school.
class ToggleButton {
 public:
  ToggleButton(int pin) : pin_(pin), on_(false), debounce_timestamp_(0), changed_(false) {
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, LOW);
    delay(1);
  }

  // true - button is pressed.
  bool Read() {
    Update(millis());
    return changed_;
  }

  void Update(uint32_t time_now) {
    if ((time_now - debounce_timestamp_) < 150) {
      changed_ = false;
      return;
    }

    int val = Read_Internal();
    changed_ = on_ != val;

    if (changed_) {  // Has the toggle switch changed?
      on_ = val;       // Set the new toggle switch value.
      // Protect against debouncing artifacts by putting a 200ms delay from the
      // last time the value changed.
      if ((time_now - debounce_timestamp_) > 150) {
        //++curr_val_;     // ... and increment the value.
        debounce_timestamp_ = time_now;
      }
    }
  }

 private:
  bool Read_Internal() {
    // Toggle the pin back to INPUT and take a reading.
    pinMode(pin_, INPUT);
    bool on = (digitalRead(pin_) == HIGH);
    // Switch the pin back to output so that we can enable the
    // pulldown resister.
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, LOW);
    return on;
  }


  int pin_;
  bool on_;
  uint32_t debounce_timestamp_;
  bool changed_;
};

// This is the new type that is built into the midi shield.
class MidiShieldButton {
 public:
  MidiShieldButton(int pin) : pin_(pin) {
    pinMode(pin_, INPUT_PULLUP);
    delay(1);
  }

  bool Read() {
    // Toggle the pin back to INPUT and take a reading.
    int val = digitalRead(pin_) == LOW;

    return val;
  }
 private:
  int pin_;
};

class Potentiometer {
 public:
  Potentiometer(int sensor_pin) : sensor_pin_(sensor_pin) {}
  float Read() {
    float avg = 0.0;
    // Filter by reading the value multiple times and taking
    // the average.
    for (int i = 0; i < 8; ++i) {
      avg += analogRead(sensor_pin_);
    }
    avg = avg / 8.0f;
    return avg;
  }
 private:
  int sensor_pin_;
};

typedef MidiShieldButton DigitalButton;


class CountingButton {
 public:
  explicit CountingButton(int but_pin) : button_(but_pin), curr_val_(0), mButton("Counting UIButton") {
    debounce_timestamp_ = millis();
    on_ = Read();
  }
  
  void Update(uint32_t time_now) {
    bool clicked = mButton.clicked();
    bool val = Read() || mButton.clicked();
    bool changed = val != on_;

    if (clicked) {
      ++curr_val_;
      debounce_timestamp_ = time_now;
      return;
    }

    if (changed) {  // Has the toggle switch changed?
      on_ = val;       // Set the new toggle switch value.
      // Protect against debouncing artifacts by putting a 200ms delay from the
      // last time the value changed.
      if ((time_now - debounce_timestamp_) > 16) {
        if (on_) {
          ++curr_val_;     // ... and increment the value.
        }
        debounce_timestamp_ = time_now;
      }
    }
  }
  
  int curr_val() const { return curr_val_; }
  
 private:
  bool Read() {
  	return button_.Read();
  }
 
  DigitalButton button_;
  bool on_;
  int curr_val_;
  unsigned long debounce_timestamp_;
  UIButton mButton;
};

class ColorSelector {
 public:
  ColorSelector(int sensor_pin) : but_(sensor_pin) {}
  
  void Update() {
  	but_.Update(millis());
  }
  
  int curr_val() const { return but_.curr_val() % 7; }
 private:
  CountingButton but_;
};