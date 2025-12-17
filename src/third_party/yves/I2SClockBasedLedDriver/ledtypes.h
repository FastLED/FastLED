
#pragma once
#ifdef USE_FASTLED
    #include "fl/fastled.h" //to have the type CRGB
#endif

#include "fl/stl/math.h"
#include "fl/math_macros.h"

// NAN is not available in all platforms, define it if not present
#ifndef FL_NAN
    #define FL_NAN (0.0f / 0.0f)
#endif

/*Definition des pixels*/
#define APA102 0
#define HD107 0
#define HD108 2
#define WS2811 4 

#ifndef PIXEL_TYPE
#define PIXEL_TYPE APA102
#endif


typedef struct {
    uint16_t r;       // a fraction between 0 and 1
    uint16_t g;       // a fraction between 0 and 1
    uint16_t b;       // a fraction between 0 and 1
} rgb16; 

typedef struct {
    float r;       // a fraction between 0 and 1
    float g;       // a fraction between 0 and 1
    float b;       // a fraction between 0 and 1
} rgb;

typedef struct {
    float h;       // angle in degrees
    float s;       // a fraction between 0 and 1
    float v;       // a fraction between 0 and 1
} hsv;

inline rgb16 HSVtoRGB(hsv color){
    
    float H=color.h;
    rgb16 result;
    float S=color.s;
    float V=color.v;
    if(H>360 || H<0 || S>100 || S<0 || V>100 || V<0){
       // cout<<"The givem HSV values are not in valid range"<<endl;
        return result;
    }
    float s = color.s;
    float v = color.v;
   // float H=color.h;
    float C = s*v;
    float X = C*(1-FL_ABS(fl::fmod(H/60.0f, 2.0f)-1.0f));
    float m = v-C;
    float r,g,b;
    if(H >= 0 && H < 60){
        r = C,g = X,b = 0;
    }
    else if(H >= 60 && H < 120){
        r = X,g = C,b = 0;
    }
    else if(H >= 120 && H < 180){
        r = 0,g = C,b = X;
    }
    else if(H >= 180 && H < 240){
        r = 0,g = X,b = C;
    }
    else if(H >= 240 && H < 300){
        r = X,g = 0,b = C;
    }
    else{
        r = C,g = 0,b = X;
    }
   // rgb result;
    result.r= (r+m)*65535;
   result.g = (g+m)*65535;
   result.b = (b+m)*65535;
    return result;
}


inline hsv rgb2hsv(rgb in)
{
    hsv         out;
    float      min, max, delta;

    min = in.r < in.g ? in.r : in.g;
    min = min  < in.b ? min  : in.b;

    max = in.r > in.g ? in.r : in.g;
    max = max  > in.b ? max  : in.b;

    out.v = max;                                // v
    delta = max - min;
    if (delta < 0.00001)
    {
        out.s = 0;
        out.h = 0; // undefined, maybe nan?
        return out;
    }
    if( max > 0.0 ) { // NOTE: if Max is == 0, this divide would cause a crash
        out.s = (delta / max);                  // s
    } else {
        // if max is 0, then r = g = b = 0              
        // s = 0, h is undefined
        out.s = 0.0;
        out.h = FL_NAN;                            // its now undefined
        return out;
    }
    if( in.r >= max )                           // > is bogus, just keeps compilor happy
        out.h = ( in.g - in.b ) / delta;        // between yellow & magenta
    else
    if( in.g >= max )
        out.h = 2.0 + ( in.b - in.r ) / delta;  // between cyan & yellow
    else
        out.h = 4.0 + ( in.r - in.g ) / delta;  // between magenta & cyan

    out.h *= 60.0;                              // degrees

    if( out.h < 0.0 )
        out.h += 360.0;
 //Serial.printf(" %f %f %f \n",out.h,out.s,out.v);
    return out;
}



struct IndvBrightness{
  uint8_t bred;
  uint8_t bgreen;
  uint8_t bblue;
inline IndvBrightness() __attribute__((always_inline))
    {
    }
inline IndvBrightness( uint8_t ir, uint8_t ig, uint8_t ib)  __attribute__((always_inline))
        : bred(ir), bgreen(ig), bblue(ib)
    {
    }
    inline IndvBrightness( uint8_t ir)  __attribute__((always_inline))
        : bred(ir), bgreen(ir), bblue(ir)
    {
    }
     
};


struct PixelStruct{
    uint8_t block_size;
    uint8_t number_block;
    uint8_t block_order[20];
    uint8_t start_frame_size; //in block_size
    bool end_frame;
    
    uint8_t end_block_value;
    uint8_t dma_size;

inline PixelStruct(uint8_t blocksize,uint8_t numberblock,uint8_t *blockorder,uint8_t startframesize,bool endframe) __attribute__((always_inline)) 
    :block_size(blocksize),number_block(numberblock),start_frame_size(startframesize),end_frame(endframe)
{
    for(int i=0;i<numberblock;i++)
    {
        block_order[i]=blockorder[i];
    }
    dma_size=blocksize*numberblock*8*2; //each block is 8 bit *2 beause we're doing 16 // outputs
}

	inline PixelStruct() __attribute__((always_inline))
    {
    }

};


#if PIXEL_TYPE == WS2811

struct Pixel {
    union {
        uint8_t raw[3];
        struct 
        {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            
        };
        
    };

    inline Pixel(uint8_t r, uint8_t g,uint8_t b) __attribute__((always_inline))
    :red(r),green(g),blue(b)
{
    //brigthness =0xE0 |(br&31);
}

	inline Pixel() __attribute__((always_inline))
    {

    }

       inline Pixel &operator= (const IndvBrightness& rhs) __attribute__((always_inline))
    {
         //brigthness =0xE0 |(rhs.bred&31);      
        return *this;
    }
#ifdef USE_FASTLED
inline Pixel &operator= (const CRGB& rhs) __attribute__((always_inline))
    {
        red = rhs.r;
        green = rhs.g;
        blue = rhs.b;
        return *this;
    }
   #endif

inline Pixel (const Pixel& rhs) __attribute__((always_inline))
     {
         //brigthness=rhs.brigthness;
         red=rhs.red;
         green=rhs.green;
         blue=rhs.blue;
     }
     inline Pixel& operator= (const uint32_t colorcode) __attribute__((always_inline))
    {
       // rgb colorg; 
        red = (colorcode >> 16) & 0xFF;
        green = (colorcode >>  8) & 0xFF;
        blue = (colorcode >>  0) & 0xFF;
        return *this;
    }
        

};


#define BA0 1
#define BA1 0
#define BA2 2
#define BA3 0
#define BA4 0
#define BA5 0
#define BA6 0
#define BA7 0
#define BA8 0
#define BA9 0
#define BA10 0
#define BA11 0
#define BA12 0
#define BA13 0
#define BA14 0
#define BA15 0
#define BA16 0
#define BA17 0
#define BA18 0
#define BA19 0

#define DATA_SIZE 1
#define BRIGHTNESS 0
#define NUMBER_OF_BLOCK 3
#define START_FRAME_SIZE 3
#define END_FRAME 0
#define OFFSET_LED 3

#endif

#if PIXEL_TYPE == APA102

struct Pixel {
    union {
        uint8_t raw[4];
        struct 
        {
            uint8_t brigthness;
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            
        };
        
    };

    inline Pixel(uint8_t br,uint8_t r, uint8_t g,uint8_t b) __attribute__((always_inline))
    :red(r),green(g),blue(b)
{
    brigthness =0xE0 |(br&31);
}

	inline Pixel() __attribute__((always_inline)):brigthness(0xff)
    {

    }

       inline Pixel &operator= (const IndvBrightness& rhs) __attribute__((always_inline))
    {
         brigthness =0xE0 |(rhs.bred&31);      
        return *this;
    }
#ifdef USE_FASTLED
inline Pixel &operator= (const CRGB& rhs) __attribute__((always_inline))
    {
        red = rhs.r;
        green = rhs.g;
        blue = rhs.b;
        return *this;
    }
   #endif

inline Pixel (const Pixel& rhs) __attribute__((always_inline))
     {
         brigthness=rhs.brigthness;
         red=rhs.red;
         green=rhs.green;
         blue=rhs.blue;
     }
     inline Pixel& operator= (const uint32_t colorcode) __attribute__((always_inline))
    {
        red = (colorcode >> 16) & 0xFF;
        green = (colorcode >>  8) & 0xFF;
        blue = (colorcode >>  0) & 0xFF;
        return *this;
    }
        

};


#define BA0 0
#define BA1 3
#define BA2 2
#define BA3 1
#define BA4 0
#define BA5 0
#define BA6 0
#define BA7 0
#define BA8 0
#define BA9 0
#define BA10 0
#define BA11 0
#define BA12 0
#define BA13 0
#define BA14 0
#define BA15 0
#define BA16 0
#define BA17 0
#define BA18 0
#define BA19 0

#define DATA_SIZE 1
#define BRIGHTNESS 1
#define NUMBER_OF_BLOCK 4
#define START_FRAME_SIZE 4
#define END_FRAME 1
#define OFFSET_LED 4
//static uint8_t f7[4]={0,3,2,1}; //britghness blue green red
//static PixelStruct HD107=PixelStruct(1,4,f7,4,true); //4 blocks of 1*8bits
#endif
#if PIXEL_TYPE == HD108

struct Pixel {
    union {
        uint16_t raw[4];
        struct 
        {
            uint16_t brigthness;
            uint16_t red;
            uint16_t green;
            uint16_t blue;
            
        };
        
    };

inline Pixel(uint8_t br,uint16_t r, uint8_t bg,uint16_t g,uint8_t bb,uint16_t b) __attribute__((always_inline))
    :red(r),green(g),blue(b)
{
    brigthness =0x8000 | ((br&31)<<10)|((bg&31)<<5) |(bb&31);
}

inline Pixel(uint16_t r, uint16_t g, uint16_t b) __attribute__((always_inline))
:red(r),green(g),blue(b),brigthness(0xffff) 
{
    //brigthness =0x8000 ;
}

	inline Pixel() __attribute__((always_inline)):brigthness(0xffff) 
    {
    }
     
     inline Pixel (const Pixel& rhs) __attribute__((always_inline))
     {
         brigthness=rhs.brigthness;
         red=rhs.red;
         green=rhs.green;
         blue=rhs.blue;
     }
#ifdef USE_FASTLED
	inline Pixel &operator= (const CRGB& rhs) __attribute__((always_inline))
    {
        red = rhs.r;
        green = rhs.g;
        blue = rhs.b;
        return *this;
    }
#endif


    inline Pixel &operator= (const IndvBrightness& rhs) __attribute__((always_inline))
    {
        brigthness =0x8000 | ((rhs.bred&31)<<10)|((rhs.bgreen&31)<<5) |(rhs.bblue&31);      
        return *this;
    }
     inline Pixel& operator= (const uint32_t colorcode) __attribute__((always_inline))
    {
         
        rgb16 color;
        color.r= (colorcode >> 16) & 0xFF;
        color.g = (colorcode >>  8) & 0xFF;
        color.b = (colorcode >>  0) & 0xFF;
       // Serial.printf(" %f %f %f \n",colorg.r,colorg.g,colorg.b);
        
         red=color.r<<8 +color.r;
         green=color.g<<8 +color.g;
         blue=color.b<<8 +color.b;
         
        brigthness=0xffff;
        return *this;
        
    }

    
};

#define BA0 1
#define BA1 0
#define BA2 3
#define BA3 2
#define BA4 5
#define BA5 4
#define BA6 7
#define BA7 6
#define BA8 0
#define BA9 0
#define BA10 0
#define BA11 0
#define BA12 0
#define BA13 0
#define BA14 0
#define BA15 0
#define BA16 0
#define BA17 0
#define BA18 0
#define BA19 0

#define DATA_SIZE 2
#define BRIGHTNESS 1
#define NUMBER_OF_BLOCK 8
#define START_FRAME_SIZE 8
#define END_FRAME 0
#define OFFSET_LED 3
//static uint8_t f8[8]={1,0,3,2,5,4,7,6}; //brightness,red,green,blue
//static PixelStruct HD108=PixelStruct(2,4,f8,8,false); //4 blocks of 2*8bits
#endif
