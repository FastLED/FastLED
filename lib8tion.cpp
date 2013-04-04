#include <stdint.h>

#define RAND16_SEED  1337
uint16_t rand16seed = RAND16_SEED;



#if 0
// TEST / VERIFICATION CODE ONLY BELOW THIS POINT
#include <Arduino.h>
#include "lib8tion.h"

void test1abs( int8_t i)
{
    Serial.print("abs("); Serial.print(i); Serial.print(") = ");
    int8_t j = abs8(i);
    Serial.print(j); Serial.println(" ");
}

void testabs()
{
    delay(5000);
    for( int8_t q = -128; q != 127; q++) {
        test1abs(q);
    }
    for(;;){};
}


void testmul8()
{
    delay(5000);
    byte r, c;
    
    Serial.println("mul8:");
    for( r = 0; r <= 20; r += 1) {
        Serial.print(r); Serial.print(" : ");
        for( c = 0; c <= 20; c += 1) {
            byte t;
            t = mul8( r, c);
            Serial.print(t); Serial.print(' ');
        }
        Serial.println(' ');
    }
    Serial.println("done.");
    for(;;){};
}


void testscale8()
{
    delay(5000);
    byte r, c;

    Serial.println("scale8:");
    for( r = 0; r <= 240; r += 10) {
        Serial.print(r); Serial.print(" : ");
        for( c = 0; c <= 240; c += 10) {
            byte t;
            t = scale8( r, c);
            Serial.print(t); Serial.print(' ');
        }
        Serial.println(' ');
    }

    Serial.println(' ');
    Serial.println("scale8_video:");

    for( r = 0; r <= 100; r += 4) {
        Serial.print(r); Serial.print(" : ");
        for( c = 0; c <= 100; c += 4) {
            byte t;
            t = scale8_video( r, c);
            Serial.print(t); Serial.print(' ');
        }
        Serial.println(' ');
    }

    Serial.println("done.");
    for(;;){};
}



void testqadd8()
{
    delay(5000);
    byte r, c;
    for( r = 0; r <= 240; r += 10) {
        Serial.print(r); Serial.print(" : ");
        for( c = 0; c <= 240; c += 10) {
            byte t;
            t = qadd8( r, c);
            Serial.print(t); Serial.print(' ');
        }
        Serial.println(' ');
    }
    Serial.println("done.");
    for(;;){};
}

void testnscale8x3()
{
    delay(5000);
    byte r, g, b, sc;
    for( byte z = 0; z < 10; z++) {
        r = random8(); g = random8(); b = random8(); sc = random8();
        
        Serial.print("nscale8x3_video( ");
        Serial.print(r); Serial.print(", ");
        Serial.print(g); Serial.print(", ");
        Serial.print(b); Serial.print(", ");
        Serial.print(sc); Serial.print(") = [ ");
        
        nscale8x3_video( r, g, b, sc);
        
        Serial.print(r); Serial.print(", ");
        Serial.print(g); Serial.print(", ");
        Serial.print(b); Serial.print("]");
        
        Serial.println(' ');
    }
    Serial.println("done.");
    for(;;){};
}

#endif
