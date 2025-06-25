#pragma once

#include <Arduino.h>
#include "fl/ui.h"
#include "fl/dbg.h"

using namespace fl;

// Done by hand. Old school.
class ToggleButton {
 public:
  ToggleButton(int pin);

  // true - button is pressed.
  bool Read();
  void Update(uint32_t time_now);

 private:
  bool Read_Internal();

  int pin_;
  bool on_;
  uint32_t debounce_timestamp_;
  bool changed_;
};

// This is the new type that is built into the midi shield.
class MidiShieldButton {
 public:
  MidiShieldButton(int pin);
  bool Read();
 private:
  int pin_;
};

class Potentiometer {
 public:
  Potentiometer(int sensor_pin);
  float Read();
 private:
  int sensor_pin_;
};

typedef MidiShieldButton DigitalButton;

class CountingButton {
 public:
  explicit CountingButton(int but_pin);
  void Update(uint32_t time_now);
  int curr_val() const;
  
 private:
  bool Read();
 
  DigitalButton button_;
  bool on_;
  int curr_val_;
  unsigned long debounce_timestamp_;
  UIButton mButton;
};

class ColorSelector {
 public:
  ColorSelector(int sensor_pin);
  void Update();
  int curr_val() const;
 private:
  CountingButton but_;
};
