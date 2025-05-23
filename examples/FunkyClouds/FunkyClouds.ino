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

/*
-------------------------------------------------------------------
 Init Inputs and Outputs: LEDs and MSGEQ7
 -------------------------------------------------------------------
 */
void setup() {
    // use the following line only when working with a 16*16
    // and delete everything in the function RenderCustomMatrix()
    // at the end of the code; edit XY() to change your matrix layout
    // right now it is doing a serpentine mapping

    // just for debugging:
    // Serial.begin(9600);
    Serial.begin(38400);
    InitFunky();
}

/*
-------------------------------------------------------------------
 The main program
 -------------------------------------------------------------------
 */
void loop() {
    AutoRun();
    // Comment AutoRun out and test examples seperately here

    // Dots2();

    // For discovering parameters of examples I reccomend to
    // tinker with a renamed copy ...
}
