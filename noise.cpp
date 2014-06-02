#include <FastLED.h>

static uint8_t p[] = { 151,160,137,91,90,15,
   131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
   190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
   88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
   77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
   102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
   135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
   5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
   223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
   129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
   251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
   49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
   138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,151
   };
#define P(x) p[(x)]

//
// #define FADE_12
#define FADE_16

#ifdef FADE_12
#define FADE logfade12
#define LERP(a,b,u) lerp15by12(a,b,u)
#else
#define FADE(x) scale16(x,x)
#define LERP(a,b,u) lerp15by16(a,b,u)
#endif

static int16_t __attribute__((always_inline))  grad16(uint8_t hash, int16_t x, int16_t y, int16_t z) {
#if 0
  switch(hash & 0xF) {
    case  0: return (( x) + ( y))>>1;
    case  1: return ((-x) + ( y))>>1;
    case  2: return (( x) + (-y))>>1;
    case  3: return ((-x) + (-y))>>1;
    case  4: return (( x) + ( z))>>1;
    case  5: return ((-x) + ( z))>>1;
    case  6: return (( x) + (-z))>>1;
    case  7: return ((-x) + (-z))>>1;
    case  8: return (( y) + ( z))>>1;
    case  9: return ((-y) + ( z))>>1;
    case 10: return (( y) + (-z))>>1;
    case 11: return ((-y) + (-z))>>1;
    case 12: return (( y) + ( x))>>1;
    case 13: return ((-y) + ( z))>>1;
    case 14: return (( y) + (-x))>>1;
    case 15: return ((-y) + (-z))>>1;
  }
#else
  hash = hash&15;
  int16_t u = hash<8?x:y;
  int16_t v = hash<4?y:hash==12||hash==14?x:z;
  if(hash&1) { u = -u; }
  if(hash&2) { v = -v; }

  return (u+v)>>1;
#endif
}

static int8_t  __attribute__((always_inline)) grad8(uint8_t hash, int8_t x, int8_t y, int8_t z) {
#if 0
  switch(hash & 0xF) {
    case  0: return (( x) + ( y))>>1;
    case  1: return ((-x) + ( y))>>1;
    case  2: return (( x) + (-y))>>1;
    case  3: return ((-x) + (-y))>>1;
    case  4: return (( x) + ( z))>>1;
    case  5: return ((-x) + ( z))>>1;
    case  6: return (( x) + (-z))>>1;
    case  7: return ((-x) + (-z))>>1;
    case  8: return (( y) + ( z))>>1;
    case  9: return ((-y) + ( z))>>1;
    case 10: return (( y) + (-z))>>1;
    case 11: return ((-y) + (-z))>>1;
    case 12: return (( y) + ( x))>>1;
    case 13: return ((-y) + ( z))>>1;
    case 14: return (( y) + (-x))>>1;
    case 15: return ((-y) + (-z))>>1;
  }
#else
  hash &= 0xF;
  int8_t u = (hash&8)?y:x;
  int8_t v = hash<4?y:hash==12||hash==14?x:z;
  if(hash&1) { u = -u; }
  if(hash&2) { v = -v; }

  return (u+v)>>1;
#endif
}

#ifdef FADE_12
uint16_t logfade12(uint16_t val) {
  return scale16(val,val)>>4;
}

static int16_t __attribute__((always_inline)) lerp15by12( int16_t a, int16_t b, fract16 frac)
{
   //if(1) return (lerp(frac,a,b));
    int16_t result;
    if( b > a) {
        uint16_t delta = b - a;
        uint16_t scaled = scale16(delta,frac<<4);
        result = a + scaled;
     } else {
        uint16_t delta = a - b;
        uint16_t scaled = scale16(delta,frac<<4);
      result = a - scaled;
     }
    return result;
}
#endif

static int8_t __attribute__((always_inline)) lerp7by8( int8_t a, int8_t b, fract8 frac)
{
    // int8_t delta = b - a;
    // int16_t prod = (uint16_t)delta * (uint16_t)frac;
    // int8_t scaled = prod >> 8;
    // int8_t result = a + scaled;
    // return result;
    int8_t result;
    if( b > a) {
        uint8_t delta = b - a;
        uint8_t scaled = scale8( delta, frac);
        result = a + scaled;
    } else {
        uint8_t delta = a - b;
        uint8_t scaled = scale8( delta, frac);
        result = a - scaled;
    }
    return result;
}

uint16_t inoise16(uint32_t x, uint32_t y, uint32_t z)
{
  // Find the unit cube containing the point
  uint8_t X = x>>16;
  uint8_t Y = y>>16;
  uint8_t Z = z>>16;

  // Hash cube corner coordinates
  uint8_t A = P(X)+Y;
  uint8_t AA = P(A)+Z;
  uint8_t AB = P(A+1)+Z;
  uint8_t B = P(X+1)+Y;
  uint8_t BA = P(B) + Z;
  uint8_t BB = P(B+1)+Z;

  // Get the relative position of the point in the cube
  uint16_t u = x & 0xFFFF;
  uint16_t v = y & 0xFFFF;
  uint16_t w = z & 0xFFFF;

  // Get a signed version of the above for the grad function
  int16_t xx = (u >> 1) & 0x7FFF;
  int16_t yy = (v >> 1) & 0x7FFF;
  int16_t zz = (w >> 1) & 0x7FFF;
  uint16_t N = 0x8000L;

  u = FADE(u); v = FADE(v); w = FADE(w);


  // skip the log fade adjustment for the moment, otherwise here we would
  // adjust fade values for u,v,w
  int16_t X1 = LERP(grad16(P(AA), xx, yy, zz), grad16(P(BA), xx - N, yy, zz), u);
  int16_t X2 = LERP(grad16(P(AB), xx, yy-N, zz), grad16(P(BB), xx - N, yy - N, zz), u);
  int16_t X3 = LERP(grad16(P(AA+1), xx, yy, zz-N), grad16(P(BA+1), xx - N, yy, zz-N), u);
  int16_t X4 = LERP(grad16(P(AB+1), xx, yy-N, zz-N), grad16(P(BB+1), xx - N, yy - N, zz - N), u);

  int16_t Y1 = LERP(X1,X2,v);
  int16_t Y2 = LERP(X3,X4,v);

  int16_t ans = LERP(Y1,Y2,w);

  return scale16by8(ans+15900,250)<<1;
  // return N+ans;
}

uint16_t inoise16(uint32_t x, uint32_t y)
{
  // Find the unit cube containing the point
  uint8_t X = x>>16;
  uint8_t Y = y>>16;

  // Hash cube corner coordinates
  uint8_t A = P(X)+Y;
  uint8_t AA = P(A);
  uint8_t AB = P(A+1);
  uint8_t B = P(X+1)+Y;
  uint8_t BA = P(B);
  uint8_t BB = P(B+1);

  // Get the relative position of the point in the cube
  uint16_t u = x & 0xFFFF;
  uint16_t v = y & 0xFFFF;

  // Get a signed version of the above for the grad function
  int16_t xx = (u >> 1) & 0x7FFF;
  int16_t yy = (v >> 1) & 0x7FFF;
  uint16_t N = 0x8000L;

  u = FADE(u); v = FADE(v);

  int16_t X1 = LERP(grad16(P(AA), xx, yy, 0), grad16(P(BA), xx - N, yy, 0), u);
  int16_t X2 = LERP(grad16(P(AB), xx, yy-N, 0), grad16(P(BB), xx - N, yy - N, 0), u);

  int16_t ans = LERP(X1,X2,v);

  return scale16by8(ans+15900,250)<<1;
  // return N+ans;
}

uint16_t inoise16(uint32_t x)
{
  // Find the unit cube containing the point
  uint8_t X = x>>16;

  // Hash cube corner coordinates
  uint8_t A = P(X);
  uint8_t AA = P(A);
  uint8_t B = P(X+1);
  uint8_t BA = P(B);

  // Get the relative position of the point in the cube
  uint16_t u = x & 0xFFFF;

  // Get a signed version of the above for the grad function
  int16_t xx = (u >> 1) & 0x7FFF;
  uint16_t N = 0x8000L;

  u = FADE(u);

  int16_t ans = LERP(grad16(P(AA), xx, 0, 0), grad16(P(BA), xx - N, 0, 0), u);

  return scale16by8(ans+15900,250)<<1;
  // return N+ans;
}

uint8_t inoise8(uint16_t x, uint16_t y, uint16_t z)
{
  // Find the unit cube containing the point
  uint8_t X = x>>8;
  uint8_t Y = y>>8;
  uint8_t Z = z>>8;

  // Hash cube corner coordinates
  uint8_t A = P(X)+Y;
  uint8_t AA = P(A)+Z;
  uint8_t AB = P(A+1)+Z;
  uint8_t B = P(X+1)+Y;
  uint8_t BA = P(B) + Z;
  uint8_t BB = P(B+1)+Z;

  // Get the relative position of the point in the cube
  uint8_t u = x;
  uint8_t v = y;
  uint8_t w = z;

  // Get a signed version of the above for the grad function
  int8_t xx = (x>>1) & 0x7F;
  int8_t yy = (y>>1) & 0x7F;
  int8_t zz = (z>>1) & 0x7F;
  uint8_t N = 0x80;

  // u = FADE(u); v = FADE(v); w = FADE(w);
  u = scale8_LEAVING_R1_DIRTY(u,u); v = scale8_LEAVING_R1_DIRTY(v,v); w = scale8(w,w);

  int8_t X1 = lerp7by8(grad8(P(AA), xx, yy, zz), grad8(P(BA), xx - N, yy, zz), u);
  int8_t X2 = lerp7by8(grad8(P(AB), xx, yy-N, zz), grad8(P(BB), xx - N, yy - N, zz), u);
  int8_t X3 = lerp7by8(grad8(P(AA+1), xx, yy, zz-N), grad8(P(BA+1), xx - N, yy, zz-N), u);
  int8_t X4 = lerp7by8(grad8(P(AB+1), xx, yy-N, zz-N), grad8(P(BB+1), xx - N, yy - N, zz - N), u);

  int8_t Y1 = lerp7by8(X1,X2,v);
  int8_t Y2 = lerp7by8(X3,X4,v);

  int8_t ans = lerp7by8(Y1,Y2,w);

  return scale8((70+(ans)),234)<<1;
}

uint8_t inoise8(uint16_t x, uint16_t y)
{
  // Find the unit cube containing the point
  uint8_t X = x>>8;
  uint8_t Y = y>>8;

  // Hash cube corner coordinates
  uint8_t A = P(X)+Y;
  uint8_t AA = P(A);
  uint8_t AB = P(A+1);
  uint8_t B = P(X+1)+Y;
  uint8_t BA = P(B);
  uint8_t BB = P(B+1);

  // Get the relative position of the point in the cube
  uint8_t u = x;
  uint8_t v = y;

  // Get a signed version of the above for the grad function
  int8_t xx = (x>>1) & 0x7F;
  int8_t yy = (y>>1) & 0x7F;
  uint8_t N = 0x80;

  // u = FADE(u); v = FADE(v); w = FADE(w);
  u = scale8_LEAVING_R1_DIRTY(u,u); v = scale8(v,v);

  int8_t X1 = lerp7by8(grad8(P(AA), xx, yy, 0), grad8(P(BA), xx - N, yy, 0), u);
  int8_t X2 = lerp7by8(grad8(P(AB), xx, yy-N, 0), grad8(P(BB), xx - N, yy - N, 0), u);

  int8_t ans = lerp7by8(X1,X2,v);

  return scale8((70+(ans)),234)<<1;
}

uint8_t inoise8(uint16_t x)
{
  // Find the unit cube containing the point
  uint8_t X = x>>8;

  // Hash cube corner coordinates
  uint8_t A = P(X);
  uint8_t AA = P(A);
  uint8_t B = P(X+1);
  uint8_t BA = P(B);

  // Get the relative position of the point in the cube
  uint8_t u = x;

  // Get a signed version of the above for the grad function
  int8_t xx = (x>>1) & 0x7F;
  uint8_t N = 0x80;

  u = scale8(u,u);

  int8_t ans = lerp7by8(grad8(P(AA), xx, 0, 0), grad8(P(BA), xx - N, 0, 0), u);

  return scale8((70+(ans)),234)<<1;
}

void fill_raw_noise8(uint8_t *pData, uint8_t num_points, uint8_t octaves, uint16_t x, int scale, uint16_t time) {
  uint32_t _xx = x;
  uint32_t scx = scale;
  for(int o = 0; o < octaves; o++) {
    for(int i = 0,xx=_xx; i < num_points; i++, xx+=scx) {
          pData[i] = qadd8(pData[i],inoise8(xx,time)>>o);
    }

    _xx <<= 1;
    scx <<= 1;
  }
}

void fill_raw_noise16into8(uint8_t *pData, uint8_t num_points, uint8_t octaves, uint32_t x, int scale, uint32_t time) {
  uint32_t _xx = x;
  uint32_t scx = scale;
  for(int o = 0; o < octaves; o++) {
    for(int i = 0,xx=_xx; i < num_points; i++, xx+=scx) {
      uint32_t accum = (inoise16(xx,time))>>o;
      accum += (pData[i]<<8);
      if(accum > 65535) { accum = 65535; }
      pData[i] = accum>>8;
    }

    _xx <<= 1;
    scx <<= 1;
  }
}

void fill_raw_2dnoise8(uint8_t *pData, int width, int height, uint8_t octaves, uint16_t x, int scalex, uint16_t y, int scaley, uint16_t time) {
  uint32_t _xx = x;
  uint32_t _yy = y;
  uint32_t scx = scalex;
  uint32_t scy = scaley;
  for(int o = 0; o < octaves; o++) {
    for(int i = 0,yy=_yy; i < height; i++,yy+=scy) {
      uint8_t *pRow = pData + (i * width);
      for(int j = 0,xx=_xx; j < width; j++,xx+=scx) {
        pRow[j] = qadd8(pRow[j],inoise8(xx,yy,time)>>o);
      }
    }
    _xx <<= 1;
    scx <<= 1;
    _yy <<= 1;
    scy <<= 1;
  }
}

inline void fill_raw_2dnoise16into8(uint8_t *pData, int width, int height, uint8_t octaves, uint32_t x, int scalex, uint32_t y, int scaley, uint32_t time) {
  for(int o = 0; o < octaves; o++) {
    for(int i = 0,yy=y; i < height; i++,yy+=scaley) {
      uint8_t *pRow = pData + (i * width);
      for(int j = 0,xx=x; j < width; j++,xx+=scalex) {
        uint32_t accum = (inoise16(xx,yy,time))>>o;
        accum += (pRow[j]<<8);
        if(accum > 65535) { accum = 65535; }
        pRow[j] = accum>>8;
      }
    }
    x <<= 1;
    scalex <<= 1;
    y <<= 1;
    scaley <<= 1;
  }
}

void fill_noise8(CRGB *leds, int num_leds,
            uint8_t octaves, uint16_t x, int scale,
            uint8_t hue_octaves, uint16_t hue_x, int hue_scale,
            uint16_t time) {
  uint8_t V[num_leds];
  uint8_t H[num_leds];

  memset(V,0,num_leds);
  memset(H,0,num_leds);

  fill_raw_noise8(V,num_leds,octaves,x,scale,time);
  fill_raw_noise8(H,num_leds,hue_octaves,hue_x,hue_scale,time);

  for(int i = 0; i < num_leds; i++) {
    leds[i] = CHSV(H[i],255,V[i]);
  }
}

void fill_noise16(CRGB *leds, int num_leds,
            uint8_t octaves, uint16_t x, int scale,
            uint8_t hue_octaves, uint16_t hue_x, int hue_scale,
            uint16_t time) {
  uint8_t V[num_leds];
  uint8_t H[num_leds];

  memset(V,0,num_leds);
  memset(H,0,num_leds);

  fill_raw_noise16into8(V,num_leds,octaves,x,scale,time);
  fill_raw_noise8(H,num_leds,hue_octaves,hue_x,hue_scale,time);

  for(int i = 0; i < num_leds; i++) {
    leds[i] = CHSV(H[i],255,V[i]);
  }
}

void fill_2dnoise8(CRGB *leds, int width, int height, bool serpentine,
            uint8_t octaves, uint16_t x, int xscale, uint16_t y, int yscale, uint16_t time,
            uint8_t hue_octaves, uint16_t hue_x, int hue_xscale, uint16_t hue_y, uint16_t hue_yscale,uint16_t hue_time,bool blend) {
  uint8_t V[height][width];
  uint8_t H[height][width];

  memset(V,0,height*width);
  memset(H,0,height*width);

  fill_raw_2dnoise8((uint8_t*)V,width,height,octaves,x,xscale,y,yscale,time);
  fill_raw_2dnoise8((uint8_t*)H,width,height,hue_octaves,hue_x,hue_xscale,hue_y,hue_yscale,hue_time);

  int w1 = width-1;
  int h1 = height-1;
  for(int i = 0; i < height; i++) {
    int wb = i*width;
    for(int j = 0; j < width; j++) {
      CRGB led(CHSV(H[h1-i][w1-j],255,V[i][j]));

      int pos = j;
      if(serpentine && (i&0x1)) {
        pos = w1-j;
      }

      if(blend) {
        leds[wb+pos] >>= 1; leds[wb+pos] += (led>>=1);
      } else {
        leds[wb+pos] = led;
      }
    }
  }
}

void fill_2dnoise16(CRGB *leds, int width, int height, bool serpentine,
            uint8_t octaves, uint32_t x, int xscale, uint32_t y, int yscale, uint32_t time,
            uint8_t hue_octaves, uint16_t hue_x, int hue_xscale, uint16_t hue_y, uint16_t hue_yscale,uint16_t hue_time, bool blend) {
  uint8_t V[height][width];
  uint8_t H[height][width];

  memset(V,0,height*width);
  memset(H,0,height*width);

  fill_raw_2dnoise16into8((uint8_t*)V,width,height,octaves,x,xscale,y,yscale,time);
  fill_raw_2dnoise8((uint8_t*)H,width,height,hue_octaves,hue_x,hue_xscale,hue_y,hue_yscale,hue_time);

  int w1 = width-1;
  int h1 = height-1;
  for(int i = 0; i < height; i++) {
    int wb = i*width;
    for(int j = 0; j < width; j++) {
      CRGB led(CHSV(H[h1-i][w1-j],255,V[i][j]));

      int pos = j;
      if(serpentine && (i&0x1)) {
        pos = w1-j;
      }

      if(blend) {
        leds[wb+pos] >>= 1; leds[wb+pos] += (led>>=1);
      } else {
        leds[wb+pos] = led;
      }
    }
  }
}
