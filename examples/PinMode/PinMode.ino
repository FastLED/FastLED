/// @file    PinMode.ino
/// @brief   Checks that pinMode, digitalWrite and digitalRead work correctly.
/// @example Pintest.ino

#include <FastLED.h>

#define PIN 1

void setup() {
  delay(1000);
  Serial.begin(9600);
}

void loop() {
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN, HIGH);
  delay(100);
  digitalWrite(PIN, LOW);
  pinMode(PIN, INPUT);
  bool on = digitalRead(PIN);
  Serial.print("Pin: ");
  Serial.print(PIN);
  Serial.print(" is ");
  Serial.println(on ? "HIGH" : "LOW");
  delay(1000);
}
