#include "FastLED.h"

#ifndef NBSPRITE
#define NBSPRITE 8
#endif
#ifndef SPRITE_WIDTH
#define SPRITE_WIDTH 20
#endif
#ifndef SPRITE_HEIGHT
#define SPRITE_HEIGHT 20
#endif
#ifndef nb_components
#define nb_components 3
#endif

struct res {
    bool result;
    CRGB color;
};
static int _spritenumber;
uint16_t * target; //to be sized in the main
uint8_t _spritesleds[NBSPRITE*SPRITE_HEIGHT*SPRITE_WIDTH*nb_components];
class hardwareSprite
{
public:
  hardwareSprite()
  {
      displaySprite=false;
      leds=(CRGB*)&_spritesleds[_spritenumber*SPRITE_WIDTH*SPRITE_HEIGHT*nb_components];
      spritenumber=_spritenumber;
      _spritenumber++;

  };
  bool displaySprite;
  int spritenumber;
  CRGB transparentColor=CRGB(0,0,0);
  int posX = 0;
  int posY = 0;

  int offset(int x, int y, int width, int height)
  {
      
    if ((posX + x) >= width or (posX + x) < 0 or (posY + y) >= height or (posY + y) < 0)
    {
        //Serial.printf("%d %d,%d %d ",x,y,posX+x,posY+y);
        //Serial.println("out");
      return -1;
    }
    //Serial.println("ok");
#if SNAKEPATTERN == 1
    if ((posY+y) % 2 == 0)
    {
      return (posY + y) * width + x + posX;
    }
    else
    {
      return width * (y + posY + 1) - 1 - (x + posX);
    }
#else
    return (y + posY) * width + x + posX;

#endif
  }
  void setTransparentColor(CRGB color)
  {
      for(int i=0;i<SPRITE_WIDTH*SPRITE_HEIGHT;i++)
      {
          leds[i]=color;
          transparentColor=color;
      }
  }
  void reorder(int width, int height)
  {
      if(displaySprite)
      {
    for (int i = 0; i < SPRITE_WIDTH; i++)
    {
      for (int j = 0; j<  SPRITE_HEIGHT; j++)
      {
          if(leds[j * SPRITE_WIDTH + i]!=transparentColor)
          {
              int _offset=offset(i, j, width, height);
              if(_offset>=0 and _offset<width*height)
                target[_offset]=(uint16_t)(((j * SPRITE_WIDTH + i)+spritenumber*SPRITE_WIDTH*SPRITE_HEIGHT)*nb_components+1); //if 0 then no print
            //else    
              //  Serial.printf("%d %d out\n",i,j);
        //lednumber[j * WIDTH + i] = offset(i, j, width, height);
        //Serial.printf("%d %d %d\n",i,j,lednumber[j * WIDTH + i]);
         // _led[j * WIDTH + i] = leds[j * WIDTH + i];
          }
        
      }
    }
      }
  }

  CRGB *leds;


  
};

hardwareSprite sprites[NBSPRITE];
