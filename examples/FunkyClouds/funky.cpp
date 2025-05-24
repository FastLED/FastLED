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

#include "FastLED.h"

#include "defs.h"
#include "funky.h"
#include "gfx.h"



// your display buffer for your not 16*16 setup
CRGB leds2[CUSTOM_HEIGHT * CUSTOM_WIDTH];

// the oscillators: linear ramps 0-255
// modified only by MoveOscillators()
byte osci[4];

// sin8(osci) swinging between 0 - 15
// modified only by MoveOscillators()
byte p[4];

// storage of the 7 10Bit (0-1023) audio band values
// modified only by AudioRead()
int left[7];
int right[7];

// noise stuff
uint16_t speed = 10;
uint16_t scale = 50;
uint16_t scale2 = 30;
const uint8_t kMatrixWidth = 16;
const uint8_t kMatrixHeight = 16;
#define MAX_DIMENSION                                                          \
    ((kMatrixWidth > kMatrixHeight) ? kMatrixWidth : kMatrixHeight)
uint8_t noise[MAX_DIMENSION][MAX_DIMENSION];
uint8_t noise2[MAX_DIMENSION][MAX_DIMENSION];
uint16_t x;
uint16_t y;
uint16_t z;
uint16_t x2;
uint16_t y2;
uint16_t z2;
// palette stuff
CRGBPalette16 currentPalette;
TBlendType currentBlending;
extern CRGBPalette16 myRedWhiteBluePalette;
extern const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM;

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

/*
-------------------------------------------------------------------
 Basic Helper functions:

 XY                  translate 2 dimensional coordinates into an index
 Line                draw a line
 Pixel               draw a pixel
 ClearAll            empty the screenbuffer
 MoveOscillators     increment osci[] and calculate p[]=sin8(osci)
 InitMSGEQ7          activate the MSGEQ7
 ReadAudio           get data from MSGEQ7 into left[7] and right[7]

 -------------------------------------------------------------------
 */

// translates from x, y into an index into the LED array and
// finds the right index for a S shaped matrix
int XY(int x, int y) {
    if (y > HEIGHT) {
        y = HEIGHT;
    }
    if (y < 0) {
        y = 0;
    }
    if (x > WIDTH) {
        x = WIDTH;
    }
    if (x < 0) {
        x = 0;
    }
    // for a serpentine layout reverse every 2nd row:
    if (x % 2 == 1) {
        return (x * (WIDTH) + (HEIGHT - y - 1));
    } else {
        // use that line only, if you have all rows beginning at the same side
        return (x * (WIDTH) + y);
    }
}

// Bresenham line algorythm based on 2 coordinates
void Line(int x0, int y0, int x1, int y1, byte color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        leds[XY(x0, y0)] = CHSV(color, 255, 255);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = 2 * err;
        if (e2 > dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// write one pixel with HSV color to coordinates
void Pixel(int x, int y, byte color) { leds[XY(x, y)] = CHSV(color, 255, 255); }

// delete the screenbuffer
void ClearAll() {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = 0;
    }
}

/*
Oscillators and Emitters
 */

// set the speeds (and by that ratios) of the oscillators here
void MoveOscillators() {
    osci[0] = osci[0] + 5;
    osci[1] = osci[1] + 2;
    osci[2] = osci[2] + 3;
    osci[3] = osci[3] + 4;
    for (int i = 0; i < 4; i++) {
        p[i] =
            sin8(osci[i]) /
            17; // why 17? to keep the result in the range of 0-15 (matrix size)
    }
}

// wake up the MSGEQ7
void InitMSGEQ7() {
    pinMode(MSGEQ7_RESET_PIN, OUTPUT);
    pinMode(MSGEQ7_STROBE_PIN, OUTPUT);
    digitalWrite(MSGEQ7_RESET_PIN, LOW);
    digitalWrite(MSGEQ7_STROBE_PIN, HIGH);
}

// get the data from the MSGEQ7
// (still fucking slow...)
void ReadAudio() {
    digitalWrite(MSGEQ7_RESET_PIN, HIGH);
    digitalWrite(MSGEQ7_RESET_PIN, LOW);
    for (byte band = 0; band < 7; band++) {
        digitalWrite(MSGEQ7_STROBE_PIN, LOW);
        delayMicroseconds(30);
        left[band] = analogRead(AUDIO_LEFT_PIN);
        right[band] = analogRead(AUDIO_RIGHT_PIN);
        digitalWrite(MSGEQ7_STROBE_PIN, HIGH);
    }
}

/*
-------------------------------------------------------------------
 Functions for manipulating existing data within the screenbuffer:

 DimAll           scales the brightness of the screenbuffer down
 Caleidoscope1    mirror one quarter to the other 3 (and overwrite them)
 Caleidoscope2    rotate one quarter to the other 3 (and overwrite them)
 Caleidoscope3    useless bullshit?!
 Caleidoscope4    rotate and add the complete screenbuffer 3 times
 Caleidoscope5    copy a triangle from the first quadrant to the other half
 Caleidoscope6
 SpiralStream     stream = give it a nice fading tail
 HorizontalStream
 VerticalStream
 VerticalMove     move = just move it as it is one line down
 Copy             copy a rectangle
 RotateTriangle   copy + rotate a triangle (in 8*8)
 MirrorTriangle   copy + mirror a triangle (in 8*8)
 RainbowTriangle  static draw for debugging

 -------------------------------------------------------------------
 */

// scale the brightness of the screenbuffer down
void DimAll(byte value) {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].nscale8(value);
    }
}

/*
Caleidoscope1 mirrors from source to A, B and C

 y

 |       |
 |   B   |   C
 |_______________
 |       |
 |source |   A
 |_______________ x

 */
void Caleidoscope1() {
    for (int x = 0; x < WIDTH / 2; x++) {
        for (int y = 0; y < HEIGHT / 2; y++) {
            leds[XY(WIDTH - 1 - x, y)] = leds[XY(x, y)];  // copy to A
            leds[XY(x, HEIGHT - 1 - y)] = leds[XY(x, y)]; // copy to B
            leds[XY(WIDTH - 1 - x, HEIGHT - 1 - y)] =
                leds[XY(x, y)]; // copy to C
        }
    }
}

/*
Caleidoscope2 rotates from source to A, B and C

 y

 |       |
 |   C   |   B
 |_______________
 |       |
 |source |   A
 |_______________ x

 */
void Caleidoscope2() {
    for (int x = 0; x < WIDTH / 2; x++) {
        for (int y = 0; y < HEIGHT / 2; y++) {
            leds[XY(WIDTH - 1 - x, y)] = leds[XY(y, x)]; // rotate to A
            leds[XY(WIDTH - 1 - x, HEIGHT - 1 - y)] =
                leds[XY(x, y)];                           // rotate to B
            leds[XY(x, HEIGHT - 1 - y)] = leds[XY(y, x)]; // rotate to C
        }
    }
}

// adds the color of one quarter to the other 3
void Caleidoscope3() {
    for (int x = 0; x < WIDTH / 2; x++) {
        for (int y = 0; y < HEIGHT / 2; y++) {
            leds[XY(WIDTH - 1 - x, y)] += leds[XY(y, x)]; // rotate to A
            leds[XY(WIDTH - 1 - x, HEIGHT - 1 - y)] +=
                leds[XY(x, y)];                            // rotate to B
            leds[XY(x, HEIGHT - 1 - y)] += leds[XY(y, x)]; // rotate to C
        }
    }
}

// add the complete screenbuffer 3 times while rotating
void Caleidoscope4() {
    for (int x = 0; x < WIDTH; x++) {
        for (int y = 0; y < HEIGHT; y++) {
            leds[XY(WIDTH - 1 - x, y)] += leds[XY(y, x)]; // rotate to A
            leds[XY(WIDTH - 1 - x, HEIGHT - 1 - y)] +=
                leds[XY(x, y)];                            // rotate to B
            leds[XY(x, HEIGHT - 1 - y)] += leds[XY(y, x)]; // rotate to C
        }
    }
}

// rotate, duplicate and copy over a triangle from first sector into the other
// half (crappy code)
void Caleidoscope5() {
    for (int x = 1; x < 8; x++) {
        leds[XY(7 - x, 7)] += leds[XY(x, 0)];
    } // a
    for (int x = 2; x < 8; x++) {
        leds[XY(7 - x, 6)] += leds[XY(x, 1)];
    } // b
    for (int x = 3; x < 8; x++) {
        leds[XY(7 - x, 5)] += leds[XY(x, 2)];
    } // c
    for (int x = 4; x < 8; x++) {
        leds[XY(7 - x, 4)] += leds[XY(x, 3)];
    } // d
    for (int x = 5; x < 8; x++) {
        leds[XY(7 - x, 3)] += leds[XY(x, 4)];
    } // e
    for (int x = 6; x < 8; x++) {
        leds[XY(7 - x, 2)] += leds[XY(x, 5)];
    } // f
    for (int x = 7; x < 8; x++) {
        leds[XY(7 - x, 1)] += leds[XY(x, 6)];
    } // g
}

void Caleidoscope6() {
    for (int x = 1; x < 8; x++) {
        leds[XY(7 - x, 7)] = leds[XY(x, 0)];
    } // a
    for (int x = 2; x < 8; x++) {
        leds[XY(7 - x, 6)] = leds[XY(x, 1)];
    } // b
    for (int x = 3; x < 8; x++) {
        leds[XY(7 - x, 5)] = leds[XY(x, 2)];
    } // c
    for (int x = 4; x < 8; x++) {
        leds[XY(7 - x, 4)] = leds[XY(x, 3)];
    } // d
    for (int x = 5; x < 8; x++) {
        leds[XY(7 - x, 3)] = leds[XY(x, 4)];
    } // e
    for (int x = 6; x < 8; x++) {
        leds[XY(7 - x, 2)] = leds[XY(x, 5)];
    } // f
    for (int x = 7; x < 8; x++) {
        leds[XY(7 - x, 1)] = leds[XY(x, 6)];
    } // g
}

// create a square twister
// x and y for center, r for radius
void SpiralStream(int x, int y, int r, byte dim) {
    for (int d = r; d >= 0; d--) { // from the outside to the inside
        for (int i = x - d; i <= x + d; i++) {
            leds[XY(i, y - d)] +=
                leds[XY(i + 1, y - d)]; // lowest row to the right
            leds[XY(i, y - d)].nscale8(dim);
        }
        for (int i = y - d; i <= y + d; i++) {
            leds[XY(x + d, i)] += leds[XY(x + d, i + 1)]; // right colum up
            leds[XY(x + d, i)].nscale8(dim);
        }
        for (int i = x + d; i >= x - d; i--) {
            leds[XY(i, y + d)] +=
                leds[XY(i - 1, y + d)]; // upper row to the left
            leds[XY(i, y + d)].nscale8(dim);
        }
        for (int i = y + d; i >= y - d; i--) {
            leds[XY(x - d, i)] += leds[XY(x - d, i - 1)]; // left colum down
            leds[XY(x - d, i)].nscale8(dim);
        }
    }
}

// give it a linear tail to the side
void HorizontalStream(byte scale) {
    for (int x = 1; x < WIDTH; x++) {
        for (int y = 0; y < HEIGHT; y++) {
            leds[XY(x, y)] += leds[XY(x - 1, y)];
            leds[XY(x, y)].nscale8(scale);
        }
    }
    for (int y = 0; y < HEIGHT; y++)
        leds[XY(0, y)].nscale8(scale);
}

// give it a linear tail downwards
void VerticalStream(byte scale) {
    for (int x = 0; x < WIDTH; x++) {
        for (int y = 1; y < HEIGHT; y++) {
            leds[XY(x, y)] += leds[XY(x, y - 1)];
            leds[XY(x, y)].nscale8(scale);
        }
    }
    for (int x = 0; x < WIDTH; x++)
        leds[XY(x, 0)].nscale8(scale);
}

// just move everything one line down
void VerticalMove() {
    for (int y = 15; y > 0; y--) {
        for (int x = 0; x < 16; x++) {
            leds[XY(x, y)] = leds[XY(x, y - 1)];
        }
    }
}

// copy the rectangle defined with 2 points x0, y0, x1, y1
// to the rectangle beginning at x2, x3
void Copy(byte x0, byte y0, byte x1, byte y1, byte x2, byte y2) {
    for (int y = y0; y < y1 + 1; y++) {
        for (int x = x0; x < x1 + 1; x++) {
            leds[XY(x + x2 - x0, y + y2 - y0)] = leds[XY(x, y)];
        }
    }
}

// rotate + copy triangle (8*8)
void RotateTriangle() {
    for (int x = 1; x < 8; x++) {
        for (int y = 0; y < x; y++) {
            leds[XY(x, 7 - y)] = leds[XY(7 - x, y)];
        }
    }
}

// mirror + copy triangle (8*8)
void MirrorTriangle() {
    for (int x = 1; x < 8; x++) {
        for (int y = 0; y < x; y++) {
            leds[XY(7 - y, x)] = leds[XY(7 - x, y)];
        }
    }
}

// draw static rainbow triangle pattern (8x8)
// (just for debugging)
void RainbowTriangle() {
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j <= i; j++) {
            Pixel(7 - i, j, i * j * 4);
        }
    }
}

/*
-------------------------------------------------------------------
 Examples how to combine functions in order to create an effect

 ...or: how to visualize some of the following data
 osci[0] ... osci[3]   (0-255) triangle
 p[0] ... p[3]         (0-15)  sinus
 left[0] ... left[6]   (0-1023) values of 63Hz, 160Hz, ...
 right[0] ... right[6] (0-1023)

 effects based only on oscillators (triangle/sine waves)

 AutoRun        shows everything that follows
 SlowMandala    red slow
 Dots1          2 arround one
 Dots2          stacking sines
 SlowMandala2   just nice and soft
 SlowMandala3   just nice and soft
 Mandala8       copy one triangle all over

 effects based on audio data (could be also linked to oscillators)

 MSGEQtest      colorfull 2 chanel 7 band analyzer
 MSGEQtest2     2 frequencies linked to dot emitters in a spiral mandala
 MSGEQtest3     analyzer 2 bars
 MSGEQtest4     analyzer x 4 (as showed on youtube)
 AudioSpiral    basedrum/snare linked to red/green emitters
 MSGEQtest5     one channel 7 band spectrum analyzer (spiral fadeout)
 MSGEQtest6     classic analyzer, slow falldown
 MSGEQtest7     spectrum mandala, color linked to low frequencies
 MSGEQtest8     spectrum mandala, color linked to osci
 MSGEQtest9     falling spectogram
 CopyTest
 Audio1
 Audio2
 Audio3
 Audio4
 CaleidoTest1
 Caleidotest2
 Audio5
 Audio6
 -------------------------------------------------------------------
 */

// all examples together
void AutoRun() {
    /*
    // all oscillator based:
     for(int i = 0; i < 300; i++) {Dots1();}
     for(int i = 0; i < 300; i++) {Dots2();}
     SlowMandala();
     SlowMandala2();
     SlowMandala3();
     for(int i = 0; i < 300; i++) {Mandala8();}

     // all MSGEQ7 based:
     for(int i = 0; i < 500; i++) {MSGEQtest();}
     for(int i = 0; i < 500; i++) {MSGEQtest2();}
     for(int i = 0; i < 500; i++) {MSGEQtest3();}
     for(int i = 0; i < 500; i++) {MSGEQtest4();}
     for(int i = 0; i < 500; i++) {AudioSpiral();}
     for(int i = 0; i < 500; i++) {MSGEQtest5();}
     for(int i = 0; i < 500; i++) {MSGEQtest6();}
     for(int i = 0; i < 500; i++) {MSGEQtest7();}
     for(int i = 0; i < 500; i++) {MSGEQtest8();}
     for(int i = 0; i < 500; i++) {MSGEQtest9();}
     for(int i = 0; i < 500; i++) {CopyTest();}
     for(int i = 0; i < 500; i++) {Audio1();}
     for(int i = 0; i < 500; i++) {Audio2();}
     for(int i = 0; i < 500; i++) {Audio3();}
     for(int i = 0; i < 500; i++) {Audio4();}
     for(int i = 0; i < 500; i++) {CaleidoTest1();}
     for(int i = 0; i < 500; i++) {CaleidoTest2();}
     for(int i = 0; i < 500; i++) {Audio5();}
     for(int i = 0; i < 500; i++) {Audio6();}

     for(int i = 0; i < 500; i++) {
     NoiseExample1();
     }
     for(int i = 0; i < 500; i++) {
     NoiseExample2();
     }
     for(int i = 0; i < 500; i++) {
     NoiseExample3();
     }
     //SpeedTest();
     for(int i = 0; i < 500; i++) {
     NoiseExample4();
     }
     //for(int i = 0; i < 500; i++) {NoiseExample5();}
     */
    // NoiseExample6();
    FASTLED_ASSERT(false, "breakpoint");
    NoiseExample7();
}

// red, 4 spirals, one dot emitter
// demonstrates SpiralStream and Caleidoscope
// (psychedelic)
void SlowMandala() {
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            Pixel(i, j, 1);
            SpiralStream(4, 4, 4, 127);
            Caleidoscope1();
            ShowFrame();
            //// FastLED.delay(50);
        }
    }
}

// 2 oscillators flying arround one ;)
void Dots1() {
    MoveOscillators();
    // 2 lissajous dots red
    leds[XY(p[0], p[1])] = CHSV(1, 255, 255);
    leds[XY(p[2], p[3])] = CHSV(1, 255, 150);
    // average of the coordinates in yellow
    Pixel((p[2] + p[0]) / 2, (p[1] + p[3]) / 2, 50);
    ShowFrame();
    //// FastLED.delay(20);
    HorizontalStream(125);
}

// x and y based on 3 sine waves
void Dots2() {
    MoveOscillators();
    Pixel((p[2] + p[0] + p[1]) / 3, (p[1] + p[3] + p[2]) / 3, osci[3]);
    ShowFrame();
    // FastLED.delay(20);
    HorizontalStream(125);
}

// beautifull but periodic
void SlowMandala2() {
    for (int i = 1; i < 8; i++) {
        for (int j = 0; j < 16; j++) {
            MoveOscillators();
            Pixel(j, i, (osci[0] + osci[1]) / 2);
            SpiralStream(4, 4, 4, 127);
            Caleidoscope2();
            ShowFrame();
            // FastLED.delay(20);
        }
    }
}

// same with a different timing
void SlowMandala3() {
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            MoveOscillators();
            Pixel(j, j, (osci[0] + osci[1]) / 2);
            SpiralStream(4, 4, 4, 127);
            Caleidoscope2();
            ShowFrame();
            // FastLED.delay(20);
        }
    }
}

// 2 lissajou dots *2 *4
void Mandala8() {
    MoveOscillators();
    Pixel(p[0] / 2, p[1] / 2, osci[2]);
    Pixel(p[2] / 2, p[3] / 2, osci[3]);
    Caleidoscope5();
    Caleidoscope2();
    HorizontalStream(110);
    ShowFrame();
}

// colorfull 2 chanel 7 band analyzer
void MSGEQtest() {
    ReadAudio();
    for (int i = 0; i < 7; i++) {
        Pixel(i, 16 - left[i] / 64, left[i] / 4);
    }
    for (int i = 0; i < 7; i++) {
        Pixel(8 + i, 16 - right[i] / 64, right[i] / 4);
    }
    ShowFrame();
    VerticalStream(120);
}

// 2 frequencies linked to dot emitters in a spiral mandala
void MSGEQtest2() {
    ReadAudio();
    if (left[0] > 500) {
        Pixel(0, 0, 1);
        Pixel(1, 1, 1);
    }
    if (left[2] > 200) {
        Pixel(2, 2, 100);
    }
    if (left[6] > 200) {
        Pixel(5, 0, 200);
    }
    SpiralStream(4, 4, 4, 127);
    Caleidoscope1();
    ShowFrame();
}

// analyzer 2 bars
void MSGEQtest3() {
    ReadAudio();
    for (int i = 0; i < 8; i++) {
        Pixel(i, 16 - left[0] / 64, 1);
    }
    for (int i = 8; i < 16; i++) {
        Pixel(i, 16 - left[4] / 64, 100);
    }
    ShowFrame();
    VerticalStream(120);
}

// analyzer x 4 (as showed on youtube)
void MSGEQtest4() {
    ReadAudio();
    for (int i = 0; i < 7; i++) {
        Pixel(7 - i, 8 - right[i] / 128, i * 10);
    }
    Caleidoscope2();
    ShowFrame();
    DimAll(240);
}

// basedrum/snare linked to red/green emitters
void AudioSpiral() {
    MoveOscillators();
    SpiralStream(7, 7, 7, 130);
    SpiralStream(4, 4, 4, 122);
    SpiralStream(11, 11, 3, 122);
    ReadAudio();
    if (left[1] > 500) {
        leds[2, 1] = CHSV(1, 255, 255);
    }
    if (left[4] > 500) {
        leds[XY(random(15), random(15))] = CHSV(100, 255, 255);
    }
    ShowFrame();
    DimAll(250);
}

// one channel 7 band spectrum analyzer (spiral fadeout)
void MSGEQtest5() {
    ReadAudio();
    for (int i = 0; i < 7; i++) {
        Line(2 * i, 16 - left[i] / 64, 2 * i, 15, i * 10);
        Line(1 + 2 * i, 16 - left[i] / 64, 1 + 2 * i, 15, i * 10);
    }
    ShowFrame();
    SpiralStream(7, 7, 7, 120);
}

// classic analyzer, slow falldown
void MSGEQtest6() {
    ReadAudio();
    for (int i = 0; i < 7; i++) {
        Line(2 * i, 16 - left[i] / 64, 2 * i, 15, i * 10);
        Line(1 + 2 * i, 16 - left[i] / 64, 1 + 2 * i, 15, i * 10);
    }
    ShowFrame();
    VerticalStream(170);
}

// geile Scheiße
// spectrum mandala, color linked to 160Hz band
void MSGEQtest7() {
    MoveOscillators();
    ReadAudio();
    for (int i = 0; i < 7; i++) {
        Pixel(7 - i, 8 - right[i] / 128, i * 10 + right[1] / 8);
    }
    Caleidoscope5();
    Caleidoscope1();
    ShowFrame();
    DimAll(240);
}

// spectrum mandala, color linked to osci
void MSGEQtest8() {
    MoveOscillators();
    ReadAudio();
    for (int i = 0; i < 7; i++) {
        Pixel(7 - i, 8 - right[i] / 128, i * 10 + osci[1]);
    }
    Caleidoscope5();
    Caleidoscope2();
    ShowFrame();
    DimAll(240);
}

// falling spectogram
void MSGEQtest9() {
    ReadAudio();
    for (int i = 0; i < 7; i++) {
        leds[XY(i * 2, 0)] = CHSV(
            i * 27, 255, right[i] / 3); // brightness should be divided by 4
        leds[XY(1 + i * 2, 0)] = CHSV(i * 27, 255, left[i] / 3);
    }
    leds[XY(14, 0)] = 0;
    leds[XY(15, 0)] = 0;
    ShowFrame();
    VerticalMove();
}

// 9 analyzers
void CopyTest() {
    ReadAudio();
    for (int i = 0; i < 5; i++) {
        Line(i, 4 - left[i] / 256, i, 4, i * 10);
    }
    Copy(0, 0, 4, 4, 5, 0);
    Copy(0, 0, 4, 4, 10, 0);
    Copy(0, 0, 14, 4, 0, 5);
    Copy(0, 0, 14, 4, 0, 10);
    ShowFrame();
    DimAll(200);
}

// test scale
// NOT WORKING as intended YET!
void CopyTest2() {
    ReadAudio();
    for (int i = 0; i < 5; i++) {
        Line(i * 2, 4 - left[i] / 128, i * 2, 4, i * 10);
    }
    Scale(0, 0, 4, 4, 7, 7, 15, 15);
    ShowFrame();
    DimAll(200);
}

// rechtangle 0-1 -> 2-3
// NOT WORKING as intended YET!
void Scale(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) {
    for (int y = y2; y < y3 + 1; y++) {
        for (int x = x2; x < x3 + 1; x++) {
            leds[XY(x, y)] = leds[XY(x0 + ((x * (x1 - x0)) / (x3 - x1)),
                                     y0 + ((y * (y1 - y0)) / (y3 - y1)))];
        }
    }
}

// line spectogram mandala
void Audio1() {
    ReadAudio();
    for (int i = 0; i < 5; i++) {
        Line(3 * i, 16 - left[i] / 64, 3 * (i + 1), 16 - left[i + 1] / 64,
             255 - i * 15);
    }
    Caleidoscope4();
    ShowFrame();
    DimAll(10);
}

// line analyzer with stream
void Audio2() {
    ReadAudio();
    for (int i = 0; i < 5; i++) {
        Line(3 * i, 16 - left[i] / 64, 3 * (i + 1), 16 - left[i + 1] / 64,
             255 - i * 15);
    }
    ShowFrame();
    HorizontalStream(120);
}

void Audio3() {
    ReadAudio();
    for (int i = 0; i < 7; i++) {
        leds[XY(6 - i, right[i] / 128)] = CHSV(i * 27, 255, right[i]);
    } // brightness should be divided by 4
    Caleidoscope6();
    Caleidoscope2();
    ShowFrame();
    DimAll(255);
}

void Audio4() {
    ReadAudio();
    for (int i = 0; i < 5; i++) {
        Line(3 * i, 8 - left[i] / 128, 3 * (i + 1), 8 - left[i + 1] / 128,
             i * left[i] / 32);
    }
    Caleidoscope4();
    ShowFrame();
    DimAll(12);
}

void CaleidoTest1() {
    ReadAudio();
    for (int i = 0; i < 7; i++) {
        Line(i, left[i] / 256, i, 0, left[i] / 32);
    }
    RotateTriangle();
    Caleidoscope2(); // copy + rotate
    ShowFrame();
    DimAll(240);
}

void CaleidoTest2() {
    MoveOscillators();
    ReadAudio();
    for (int i = 0; i < 7; i++) {
        Line(i, left[i] / 200, i, 0, (left[i] / 16) + 150);
    }
    MirrorTriangle();
    Caleidoscope1(); // mirror + rotate
    ShowFrame();
    DimAll(240);
}

void Audio5() {
    ReadAudio();
    for (int i = 0; i < 5; i++) {
        Line(3 * i, 8 - left[i] / 128,           // from
             3 * (i + 1), 8 - left[i + 1] / 128, // to
             i * 30);
    } // color
    Caleidoscope4();
    ShowFrame();
    DimAll(9);
}

void Audio6() {
    ReadAudio();
    for (int i = 0; i < 5; i++) {
        Line(3 * i, 8 - left[i] / 128,                  // from
             3 * (i + 1), 8 - left[i + 1] / 128,        // to
             i * 10);                                   // lolor
        Line(15 - (3 * i), 7 + left[i] / 128,           // from
             15 - (3 * (i + 1)), 7 + left[i + 1] / 128, // to
             i * 10);                                   // color
    }
    ShowFrame();
    DimAll(200);
    // ClearAll();
}

/*
-------------------------------------------------------------------
 Testcode for mapping the 16*16 calculation buffer to your
 custom matrix size
 -------------------------------------------------------------------

 */
// describe your matrix layout here:
// P.S. If you use a 8*8 just remove the */ and /*
void RenderCustomMatrix() {
    for (int x = 0; x < CUSTOM_WIDTH; x++) {
        for (int y = 0; y < CUSTOM_HEIGHT; y++) {
            // position in the custom array
            leds2[x + x * y] =
                // positions(s) in the source 16*16
                // in this example it interpolates between just 2 diagonal
                // touching pixels
                (leds[XY(x * 2, y * 2)] +            // first point added to
                 leds[XY(1 + (x * 2), 1 + (y * 2))]) // second point
                ; // divided by 2 to get the average color
        }
    }
}

void ShowFrame() {
    // when using a matrix different than 16*16 use
    // RenderCustomMatrix();
    GraphicsShow();
    LEDS.countFPS();
}

void fillnoise8() {
    for (int i = 0; i < MAX_DIMENSION; i++) {
        int ioffset = scale * i;
        for (int j = 0; j < MAX_DIMENSION; j++) {
            int joffset = scale * j;
            noise[i][j] = inoise8(x + ioffset, y + joffset, z);
        }
    }
    y += speed;
    // z += 2;
}

void fillnoise82() {
    for (int i = 0; i < MAX_DIMENSION; i++) {
        int ioffset = scale2 * i;
        for (int j = 0; j < MAX_DIMENSION; j++) {
            int joffset = scale2 * j;
            noise2[i][j] = inoise8(x2 + ioffset, y2 + joffset, z2);
        }
    }
    y2 += speed * 3;
    // z += 2;
}

void NoiseExample1() {
    MoveOscillators();
    scale2 = 30 + p[1] * 3;
    x = p[0] * 16;
    fillnoise8();
    fillnoise82();
    for (int i = 0; i < kMatrixWidth; i++) {
        for (int j = 0; j < kMatrixHeight; j++) {
            leds[XY(i, j)] =
                CHSV(noise[i][j] << 1, 255, (noise2[i][j] + noise[i][j]) / 2);
        }
    }

    ShowFrame();
}

void FillNoise(uint16_t x, uint16_t y, uint16_t z, uint16_t scale) {
    for (int i = 0; i < MAX_DIMENSION; i++) {
        int ioffset = scale * i;
        for (int j = 0; j < MAX_DIMENSION; j++) {
            int joffset = scale * j;
            noise[i][j] = inoise8(x + ioffset, y + joffset, z);
        }
    }
}

void NoiseExample2() {
    MoveOscillators();
    FillNoise(2000 - p[2] * 100, 100, 100, 100);
    for (int i = 0; i < p[2]; i++) {
        for (int j = 0; j < kMatrixHeight; j++) {
            leds[XY(i, j)] = CRGB(noise[i][j], 0, 0);
        }
    }
    for (int i = 0; i < p[1]; i++) {
        for (int j = 0; j < kMatrixHeight; j++) {
            leds[XY(j, i)] += CRGB(0, 0, noise[i][j]);
        }
    }
    ShowFrame();
    ClearAll();
}

void NoiseExample3() {
    MoveOscillators();
    FillNoise(p[1] * 100, p[2] * 100, 100, 100);
    for (int i = 0; i < p[1]; i++) {
        for (int j = 0; j < kMatrixHeight; j++) {
            leds[XY(i, j)] = CHSV(noise[i][j], 255, 200);
        }
    }

    for (int i = 0; i < p[3]; i++) {
        for (int j = 0; j < kMatrixHeight; j++) {
            leds[XY(j, i)] += CHSV(128 + noise[i][j], 255, 200);
        }
    }

    ShowFrame();
    ClearAll();
}

void SpeedTest() {
    ReadAudio();
    ShowFrame();
}

void NoiseExample4() {
    MoveOscillators();
    FillNoise(100, 100, 100, 100);
    for (int i = 0; i < p[0] + 1; i++) {
        for (int j = 0; j < kMatrixHeight; j++) {
            leds[XY(i, j)] += CHSV(noise[i][j + p[2]], 255, 255);
        }
    }
    ShowFrame();
    ClearAll();
}

void ReadAudio2() {
    digitalWrite(MSGEQ7_RESET_PIN, HIGH);
    digitalWrite(MSGEQ7_RESET_PIN, LOW);
    for (byte band = 0; band < 7; band++) {
        digitalWrite(MSGEQ7_STROBE_PIN, LOW);
        delayMicroseconds(30);
        left[band] = analogRead(AUDIO_LEFT_PIN) / 4;
        right[band] = analogRead(AUDIO_RIGHT_PIN) / 4;
        digitalWrite(MSGEQ7_STROBE_PIN, HIGH);
    }
}

void NoiseExample5() {
    MoveOscillators();
    ReadAudio();
    FillNoise(100, 100, 100, 300);

    for (int i = 0; i < kMatrixWidth; i++) {
        for (int j = 0; j < left[1] / 64; j++) {
            leds[XY(i, 15 - j)] = CRGB(0, noise[i][left[1] / 64 - j], 0);
        }
    }

    for (int i = 0; i < kMatrixWidth; i++) {
        for (int j = 0; j < left[5] / 64; j++) {
            leds[XY(j, i)] += CRGB(noise[i][left[5] / 64 - j], 0, 0);
        }
    }
    ShowFrame();
    ClearAll();
}

void NoiseExample6() {
    // MoveOscillators();
    for (int size = 1; size < 200; size++) {
        z++;
        FillNoise(size, size, z, size);
        for (int i = 0; i < kMatrixWidth; i++) {
            for (int j = 0; j < kMatrixHeight; j++) {
                leds[XY(i, j)] = CHSV(50 + noise[i][j], 255, 255);
            }
        }
        ShowFrame();
        // ClearAll();
    }
    for (int size = 200; size > 1; size--) {
        z++;
        FillNoise(size, size, z, size);
        for (int i = 0; i < kMatrixWidth; i++) {
            for (int j = 0; j < kMatrixHeight; j++) {
                leds[XY(i, j)] = CHSV(50 + noise[i][j], 255, 255);
            }
        }
        ShowFrame();
        // ClearAll();
    }
}

void ChangePalettePeriodically() {
    uint8_t secondHand = (millis() / 1000) % 60;
    static uint8_t lastSecond = 99;

    if (lastSecond != secondHand) {
        lastSecond = secondHand;
        if (secondHand == 0) {
            currentPalette = RainbowColors_p;
            currentBlending = LINEARBLEND;
        }
        if (secondHand == 10) {
            currentPalette = RainbowStripeColors_p;
            currentBlending = NOBLEND;
        }
        if (secondHand == 15) {
            currentPalette = RainbowStripeColors_p;
            currentBlending = LINEARBLEND;
        }
        if (secondHand == 20) {
            SetupPurpleAndGreenPalette();
            currentBlending = LINEARBLEND;
        }
        if (secondHand == 25) {
            SetupTotallyRandomPalette();
            currentBlending = LINEARBLEND;
        }
        if (secondHand == 30) {
            SetupBlackAndWhiteStripedPalette();
            currentBlending = NOBLEND;
        }
        if (secondHand == 35) {
            SetupBlackAndWhiteStripedPalette();
            currentBlending = LINEARBLEND;
        }
        if (secondHand == 40) {
            currentPalette = CloudColors_p;
            currentBlending = LINEARBLEND;
        }
        if (secondHand == 45) {
            currentPalette = PartyColors_p;
            currentBlending = LINEARBLEND;
        }
        if (secondHand == 50) {
            currentPalette = myRedWhiteBluePalette_p;
            currentBlending = NOBLEND;
        }
        if (secondHand == 55) {
            currentPalette = myRedWhiteBluePalette_p;
            currentBlending = LINEARBLEND;
        }
    }
}

void SetupTotallyRandomPalette() {
    for (int i = 0; i < 16; i++) {
        currentPalette[i] = CHSV(random8(), 255, random8());
    }
}

// This function sets up a palette of black and white stripes,
// using code.  Since the palette is effectively an array of
// sixteen CRGB colors, the various fill_* functions can be used
// to set them up.
void SetupBlackAndWhiteStripedPalette() {
    // 'black out' all 16 palette entries...
    fill_solid(currentPalette, 16, CRGB::Black);
    // and set every fourth one to white.
    currentPalette[0] = CRGB::White;
    currentPalette[4] = CRGB::White;
    currentPalette[8] = CRGB::White;
    currentPalette[12] = CRGB::White;
}

// This function sets up a palette of purple and green stripes.
void SetupPurpleAndGreenPalette() {
    CRGB purple = CHSV(HUE_PURPLE, 255, 255);
    CRGB green = CHSV(HUE_GREEN, 255, 255);
    CRGB black = CRGB::Black;

    currentPalette =
        CRGBPalette16(green, green, black, black, purple, purple, black, black,
                      green, green, black, black, purple, purple, black, black);
}

// This example shows how to set up a static color palette
// which is stored in PROGMEM (flash), which is almost always more
// plentiful than RAM.  A static PROGMEM palette like this
// takes up 64 bytes of flash.
const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM = {
    CRGB::Red,
    CRGB::Gray, // 'white' is too bright compared to red and blue
    CRGB::Blue, CRGB::Black,

    CRGB::Red,  CRGB::Gray,  CRGB::Blue,  CRGB::Black,

    CRGB::Red,  CRGB::Red,   CRGB::Gray,  CRGB::Gray,
    CRGB::Blue, CRGB::Blue,  CRGB::Black, CRGB::Black};
void NoiseExample7() {
    ChangePalettePeriodically();
    for (int size = 1; size < 100; size++) {
        z++;
        FillNoise(size * 3, size * 3, z, size);
        for (int i = 0; i < kMatrixWidth; i++) {
            for (int j = 0; j < kMatrixHeight; j++) {
                leds[XY(i, j)] = ColorFromPalette(currentPalette, noise[i][j],
                                                  255, currentBlending);
            }
        }
        ShowFrame();
    }
    for (int size = 100; size > 1; size--) {
        z++;
        FillNoise(size * 3, size * 3, z, size);
        for (int i = 0; i < kMatrixWidth; i++) {
            for (int j = 0; j < kMatrixHeight; j++) {
                leds[XY(i, j)] = ColorFromPalette(currentPalette, noise[i][j],
                                                  255, currentBlending);
            }
        }
        ShowFrame();
    }
}

void NoiseExample8() {
    ChangePalettePeriodically();
    x++;
    z++;
    FillNoise(x * 3, x * 3, z, sin8(x) >> 1);
    for (int i = 0; i < kMatrixWidth; i++) {
        for (int j = 0; j < kMatrixHeight; j++) {
            leds[XY(i, j)] = ColorFromPalette(currentPalette, noise[i][j], 255,
                                              currentBlending);
        }
    }
    ShowFrame();
}

void InitFunky() {
    InitMSGEQ7();

    x = random16();
    y = random16();
    z = random16();

    x2 = random16();
    y2 = random16();
    z2 = random16();
    InitGraphics();

}