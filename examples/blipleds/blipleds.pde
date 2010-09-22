#include <FastSPI_LED.h>


//Example to control 10 RGB LED Modules.
//Bliptronics.com
//Ben Moyes 2009
//Use this as you wish, but please give credit, or at least buy some of my LEDs!
//Modified Sep 2010 by Daniel Garcia (dgarcia@dgarcia.net) to show off the FastSPI_LED Library
//Note, because this library uses the arduino's hardware SPI support, you -must- use the SPI 
//pins on your hardware - e.g. pins 11 and 13 on a Arduino pro mini or such

//Holds the 15 bit RGB values for each LED.
//You'll need one for each LED, we're using 10 LEDs here.
//Note you've only got limited memory on the Arduino, so you can only control 
//Several hundred LEDs on a normal arduino. Double that on a Duemilanove.

#define NUM_LEDS 19
unsigned int Display[NUM_LEDS];  

void setup() {
  byte Counter;

  // Turn all LEDs off.
  for(Counter=0;Counter < NUM_LEDS; Counter++)
    Display[Counter]=Color(Counter,0,31-Counter);
  
  // setup/run the fast spi library
  FastSPI_LED.setLeds(NUM_LEDS);
  FastSPI_LED.setChipset(CFastSPI_LED::SPI_LPD6803);
  //FastSPI_LED.setChipset(CFastSPI_LED::SPI_HL1606);
  //FastSPI_LED.setChipset(CFastSPI_LED::SPI_595);
  // this is the default, but included here to show where/how to change it
  FastSPI_LED.setCPUPercentage(50); 
  FastSPI_LED.init();
  FastSPI_LED.start();

  show();
}


void show()
{
  // copy data from display into the rgb library's output - we need to expand it back out since 
  // the rgb library expects values from 0-255 (because it's more generically focused).
  unsigned char *pData = FastSPI_LED.getRGBData();
  for(int i=0; i < NUM_LEDS; i++) { 
    int r = (Display[i] & 0x1F) * 8;
    int g = ((Display[i] >> 10) & 0x1F) * 8;
    int b = ((Display[i] >> 5) & 0x1F) * 8;
    
    *pData++ = r;
    *pData++ = g;
    *pData++ = b;
  }
  FastSPI_LED.show();
}

// Create a 15 bit color value from R,G,B
unsigned int Color(byte r, byte g, byte b)
{
  //Take the lowest 5 bits of each value and append them end to end
  return( ((unsigned int)g & 0x1F )<<10 | ((unsigned int)b & 0x1F)<<5 | (unsigned int)r & 0x1F);
}


// Show a colour bar going up from 0 to 9
void ColorUp( unsigned int ColourToUse)
{
  byte Counter;
  for(Counter=0;Counter < NUM_LEDS; Counter++)
  {
    Display[Counter]=ColourToUse;
    show();
    delay(25);
  }  
}

// Show a colour bar going down from 9 to 0
void ColorDown( unsigned int ColourToUse)
{
  byte Counter;

  for(Counter=NUM_LEDS;Counter > 0; Counter--)
  {
    Display[Counter-1]=ColourToUse;
    show();
    delay(25);
  }  
}


//Input a value 0 to 127 to get a color value.
//The colours are a transition r - g -b - back to r
unsigned int Wheel(byte WheelPos)
{
  byte r,g,b;
  switch(WheelPos >> 5)
  {
    case 0:
      r=31- WheelPos % 32;   //Red down
      g=WheelPos % 32;      // Green up
      b=0;                  //blue off
      break; 
    case 1:
      g=31- WheelPos % 32;  //green down
      b=WheelPos % 32;      //blue up
      r=0;                  //red off
      break; 
    case 2:
      b=31- WheelPos % 32;  //blue down 
      r=WheelPos % 32;      //red up
      g=0;                  //green off
      break; 
  }
  return(Color(r,g,b));
}

void loop() {

  unsigned int Counter, Counter2, Counter3;
  
  // Lets show some demo patterns.
  // Just change Display array, then set SendMode to 0
  
  //Spin LED with colour changing
  for(Counter=0;Counter < 15; Counter++)
  {
    for(Counter2=0; Counter2 < NUM_LEDS ; Counter2++)
    {
      Display[Counter2] = Wheel(Counter * NUM_LEDS + Counter2);
      show();

      delay(25);
      Display[Counter2] = Color(0,0,0) ;     
      show();

    }
  }


  //Scrolling Rainbow Effect
  for(Counter=0; Counter < 200 ; Counter++)
  {
    Counter3=Counter * 1;
    for(Counter2=0; Counter2 < NUM_LEDS; Counter2++)
    {
      Display[Counter2] = Wheel(Counter3%95);  //There's only 96 colors in this pallette.
      Counter3+=(96 / NUM_LEDS);
    }    
    show();
    delay(25);

  }


  //Color wipes.
  for(Counter=0;Counter < 2;Counter++)
  {
    ColorUp(Color(random(0,32),random(0,32),random(0,32)));
    delay(500);
    ColorDown(Color(random(0,32),random(0,32),random(0,32)));
    delay(500);
  }
}
    
    
