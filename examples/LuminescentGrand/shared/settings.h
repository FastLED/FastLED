#ifndef CONSTAINTS_H_
#define CONSTAINTS_H_

enum {
  kNumKeys = 88,  // Don't change this. 88 keys on a keyboard.
  kNumLightsPerNote = 20,
  
  // Controls the speed of the light rope. Higher values result in
  // slower draw time, however the data integrity increases.
  kLightClockDivisor = 12,
  kNewLightClockDivisor = 16,

  // Led Curtain is a mode that we used on the bus. When this is
  // zero it's assume that we are using the TCL led lighting.
  kUseLedCurtin = 0,
  
  kShowFps = 0,  // If true then the fps is printed to the console.
  
  // Coda's keyboard indicates that this is the value when the
  // foot pedal is pressed. There is probably a more universal
  // way of detecting this value that works with more keyboards.
  kMidiFootPedal = 64,
};

#endif  // CONSTAINTS_H_

