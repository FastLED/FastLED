#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "ui.h"

class Pir {
  public:
    Pir(int pin) : pin(pin) { pinMode(pin, INPUT); }
    bool detect() {
        return digitalRead(pin) == HIGH || detectMotion.clicked();
    }

  private:
    Button detectMotion = Button("Detect Motion");
    int pin;
};