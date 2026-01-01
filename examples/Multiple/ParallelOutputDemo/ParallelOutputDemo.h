

/// @file    ParallelOutputDemo.ino
/// @brief   Demonstrates how to write to multiple strips simultaneously
/// @example ParallelOutputDemo.ino
/// @filter (platform is teensy) or (platform is due)

#include <FastLED.h>

#define NUM_LEDS_PER_STRIP 16
// Note: this can be 12 if you're using a teensy 3 and don't mind soldering the pads on the back
#define NUM_STRIPS 16

CRGB leds[NUM_STRIPS * NUM_LEDS_PER_STRIP];

// ============================================================================
// PORT CONTROLLER BACKEND - Compile-time Template Specialization
// ============================================================================
//
// IMPORTANT CONCEPT: WS2811_PORTDC, WS2811_PORTA, etc. are NOT runtime port
// variables. They are compile-time template aliases that select specialized
// FastLED controller classes for parallel pin output.
//
// How it works:
// 1. WS2811_PORTDC is a type alias expanding to a controller class template
// 2. This class is specialized to know the exact pin layout for PORTD+C pins
// 3. The pins are baked in as FastPin<2,14,7,8,6,20,21,5,15,22,23,9,10,13,11,12>
// 4. At compile time, FastLED generates optimized assembly with direct
//    hardware register writes (GPIOA_PDOR, etc.) with precise timing
// 5. This achieves parallel LED output with minimal overhead
//
// The "port" is a symbolic handle that tells the compiler:
// "Use the CWS2811Controller implementation optimized for THIS specific
//  group of pins on THIS hardware port"
//
// This is why it compiles as template<typename CHIPSET, int NUM_LANES>
// — all specialization happens at compile-time, not runtime.
//
// ============================================================================
// PIN LAYOUTS
// ============================================================================
//
// Teensy 3/3.1 (16-way parallel):
//   WS2811_PORTD:  2,14,7,8,6,20,21,5
//   WS2811_PORTC:  15,22,23,9,10,13,11,12,28,27,29,30
//                  (last 4 are pads on bottom of Teensy)
//   WS2811_PORTDC: 2,14,7,8,6,20,21,5,15,22,23,9,10,13,11,12
//                  (combined D+C for 16-way parallel output)
//
// Arduino Due (port variants):
//   WS2811_PORTA: 69,68,61,60,59,100,58,31
//                 (pin 100 only available on the digix)
//   WS2811_PORTB: 90,91,92,93,94,95,96,97
//                 (only available on the digix)
//   WS2811_PORTD: 25,26,27,28,14,15,29,11
//


// IBCC<WS2811, 1, 16> outputs;

void setup() {
  delay(5000);
  Serial.begin(57600);
  Serial.println("Starting...");
#if defined(HAS_PORTDC)
  // Teensy 3 parallel output example using port controller backend.
  //
  // fastLED.addLeds<WS2811_PORTDC, NUM_STRIPS>(...) invokes the compiler
  // to select and specialize the CWS2811Controller<> template for the
  // PORTDC pin configuration. The template is instantiated with knowledge
  // of which pins belong to which ports, enabling direct hardware register
  // access with precise timing for all 16 parallel LED outputs.
  //
  // Alternative port controller options (commented out):
  // FastLED.addLeds<WS2811_PORTA,NUM_STRIPS>(leds, NUM_LEDS_PER_STRIP);
  // FastLED.addLeds<WS2811_PORTB,NUM_STRIPS>(leds, NUM_LEDS_PER_STRIP);
  // FastLED.addLeds<WS2811_PORTD,NUM_STRIPS>(leds, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
  //
  // Using PORTDC for 16-way parallel output (combined ports D and C):
  FastLED.addLeds<WS2811_PORTDC,NUM_STRIPS>(leds, NUM_LEDS_PER_STRIP);
#else
  // NOTE: Parallel port output requires HAS_PORTDC support (Teensy 3.x only).
  // Teensy 4.x does NOT support the WS2811_PORTDC style parallel output.
  // For Teensy 4.x, use single-lane output on GPIO pins or consider
  // alternative approaches for parallel LED control.
  Serial.println("Parallel port output not supported on this platform");
#endif
}

void loop() {
  Serial.println("Loop....");
  static uint8_t hue = 0; // okay static in header
  for(int i = 0; i < NUM_STRIPS; i++) {
    for(int j = 0; j < NUM_LEDS_PER_STRIP; j++) {
      leds[(i*NUM_LEDS_PER_STRIP) + j] = CHSV((32*i) + hue+j,192,255);
    }
  }

  // Set the first n leds on each strip to show which strip it is
  for(int i = 0; i < NUM_STRIPS; i++) {
    for(int j = 0; j <= i; j++) {
      leds[(i*NUM_LEDS_PER_STRIP) + j] = CRGB::Red;
    }
  }

  hue++;

  FastLED.show();
  // FastLED.delay(100);
}
