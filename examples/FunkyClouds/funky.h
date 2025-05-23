#pragma once


/*
TODO:
 
 example show oscis+p
 document caleidoscopes better
 write better caleidoscopes...
 improve and document emitters and oszillators
 explaining one example step by step:
 goal? what? how? why?
 gridmapping for rotation + zoom
 good interpolation for other matrix dimensions than 16*16
 more move & stream functions
 layers
 palettes
 link effects to areas
 1D examples
 2d example with more than 2 sines
 speed up MSGEQ7 readings
 
 
 DONE:
 25.6.      creating basic structure 
 setting up basic examples 
 26.6.      MSGEQ7 Support
 wrote more examples
 27.6.      improved documentation
 added Move
 added AutoRun
 TODO list
 Copy
 29.6.      rotate+mirror triangle
 more examples
 30.6.      RenderCustomMatrix
 added more comments
 alpha version released
 
 
/*
 
/*
 Funky Clouds Compendium (alpha version)
 by Stefan Petrick
 
 An ever growing list of examples, tools and toys 
 for creating one- and twodimensional LED effects.
 
 Dedicated to the users of the FastLED v2.1 library
 by Daniel Garcia and Mark Kriegsmann. 
 
 Provides basic and advanced helper functions.
 Contains many examples how to creatively combine them.
 
 Tested @ATmega2560 (runs propably NOT on an Uno or
 anything else with less than 4kB RAM)
 */

#include <Arduino.h>
#include "fl/stdint.h"
#include "crgb.h"
#include "defs.h"

// the rendering buffer (16*16)
// do not touch
extern CRGB leds[NUM_LEDS];



void InitFunky();


void ShowFrame();
int XY(int x, int y);
void Line(int x0, int y0, int x1, int y1, byte color);
void Pixel(int x, int y, byte color);
void ClearAll();
void MoveOscillators();
void ReadAudio();
void DimAll(byte value);
void Caleidoscope1();
void Caleidoscope2();
void Caleidoscope3();
void Caleidoscope4();
void Caleidoscope5();
void Caleidoscope6();
void SpiralStream(int x, int y, int r, byte dim);
void HorizontalStream(byte scale);
void VerticalStream(byte scale);
void VerticalMove();
void Copy(byte x0, byte y0, byte x1, byte y1, byte x2, byte y2);
void RotateTriangle();
void MirrorTriangle();
void RainbowTriangle();
void AutoRun();
void SlowMandala();
void Dots1();
void Dots2();
void SlowMandala2();
void SlowMandala3();
void Mandala8();
void MSGEQtest();
void MSGEQtest2();
void MSGEQtest3();
void MSGEQtest4();
void AudioSpiral();
void MSGEQtest5();
void MSGEQtest6();
void MSGEQtest7();
void MSGEQtest8();
void MSGEQtest9();
void CopyTest();
void CopyTest2();
void Scale(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3);
void Audio1();
void Audio2();
void Audio3();
void Audio4();
void CaleidoTest1();
void CaleidoTest2();
void Audio5();
void Audio6();
void RenderCustomMatrix();
void fillnoise8();
void fillnoise82();
void NoiseExample1();
void FillNoise(uint16_t x, uint16_t y, uint16_t z, uint16_t scale);
void NoiseExample2();
void NoiseExample3();
void SpeedTest();
void NoiseExample4();
void ReadAudio2();
void NoiseExample5();
void NoiseExample6();
void ChangePalettePeriodically();
void SetupPurpleAndGreenPalette();
void NoiseExample8();
void InitMSGEQ7();
void NoiseExample7();
void SetupTotallyRandomPalette();
void SetupBlackAndWhiteStripedPalette();

