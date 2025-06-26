#include <Arduino.h>

#include "buttons.h"

#define FASTLED_BUTTON_DEBOUNCE_MS 4

// ToggleButton implementation
ToggleButton::ToggleButton(int pin) : pin_(pin), on_(false), debounce_timestamp_(0), changed_(false) {
  pinMode(pin_, OUTPUT);
  digitalWrite(pin_, LOW);
  delay(1);
}

bool ToggleButton::Read() {
  Update(millis());
  return changed_;
}

void ToggleButton::Update(uint32_t time_now) {
  if ((time_now - debounce_timestamp_) < FASTLED_BUTTON_DEBOUNCE_MS) {
    changed_ = false;
    return;
  }

  int val = Read_Internal();
  changed_ = on_ != val;

  if (changed_) {  // Has the toggle switch changed?
    on_ = val;       // Set the new toggle switch value.
    // Protect against debouncing artifacts by putting a 200ms delay from the
    // last time the value changed.
    if ((time_now - debounce_timestamp_) > FASTLED_BUTTON_DEBOUNCE_MS) {
      //++curr_val_;     // ... and increment the value.
      debounce_timestamp_ = time_now;
    }
  }
}

bool ToggleButton::Read_Internal() {
  // Toggle the pin back to INPUT and take a reading.
  pinMode(pin_, INPUT);
  bool on = (digitalRead(pin_) == HIGH);
  // Switch the pin back to output so that we can enable the
  // pulldown resister.
  pinMode(pin_, OUTPUT);
  digitalWrite(pin_, LOW);
  return on;
}

// MidiShieldButton implementation
MidiShieldButton::MidiShieldButton(int pin) : pin_(pin) {
  pinMode(pin_, INPUT_PULLUP);
  delay(1);
}

bool MidiShieldButton::Read() {
  // Toggle the pin back to INPUT and take a reading.
  int val = digitalRead(pin_) == LOW;
  return val;
}

// Potentiometer implementation
Potentiometer::Potentiometer(int sensor_pin) : sensor_pin_(sensor_pin) {}

float Potentiometer::Read() {
  float avg = 0.0;
  // Filter by reading the value multiple times and taking
  // the average.
  for (int i = 0; i < 8; ++i) {
    avg += analogRead(sensor_pin_);
  }
  avg = avg / 8.0f;
  return avg;
}

// CountingButton implementation
CountingButton::CountingButton(int but_pin) : button_(but_pin), curr_val_(0), mButton("Counting UIButton") {
  debounce_timestamp_ = millis();
  on_ = Read();
}

void CountingButton::Update(uint32_t time_now) {
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

int CountingButton::curr_val() const { return curr_val_; }

bool CountingButton::Read() {
  return button_.Read();
}

// ColorSelector implementation
ColorSelector::ColorSelector(int sensor_pin) : but_(sensor_pin) {}

void ColorSelector::Update() {
  but_.Update(millis());
}

int ColorSelector::curr_val() const { return but_.curr_val() % 7; }
