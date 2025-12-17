/*
TJpg_Decoder.h

JPEG Decoder for Arduino using TJpgDec:
http://elm-chan.org/fsw/tjpgd/00index.html

Incorporated into an Arduino library by Bodmer 18/10/19

Latest version here:
https://github.com/Bodmer/TJpg_Decoder
*/

#ifndef TJpg_Decoder_H
  #define TJpg_Decoder_H

  #include "User_Config.h"
  #include "tjpgd.h"
    #include "fl/stl/stdint.h"
  #include "fl/stl/string.h"

namespace fl {
namespace third_party {

enum {
	TJPG_ARRAY = 0
};

//------------------------------------------------------------------------------

typedef bool (*SketchCallback)(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *data);

class TJpg_Decoder {

public:

  TJpg_Decoder();
  ~TJpg_Decoder();

  static int jd_output(JDEC* jdec, void* bitmap, JRECT* jrect);
  static size_t jd_input(JDEC* jdec, uint8_t* buf, size_t len);

  void setJpgScale(uint8_t scale);
  void setCallback(SketchCallback sketchCallback);

  JRESULT drawJpg(int32_t x, int32_t y, const uint8_t array[], size_t array_size);
  JRESULT getJpgSize(uint16_t *w, uint16_t *h, const uint8_t array[], size_t array_size);

  void setSwapBytes(bool swap);

  bool _swap = false;

  const uint8_t* array_data  = nullptr;
  size_t array_index = 0;
  size_t array_size  = 0;

  // Must align workspace to a 32 bit boundary
  uint8_t workspace[TJPGD_WORKSPACE_SIZE] __attribute__((aligned(4)));

  uint8_t jpg_source = 0;

  int16_t jpeg_x = 0;
  int16_t jpeg_y = 0;

  uint8_t jpgScale = 0;

  SketchCallback tft_output = nullptr;

  TJpg_Decoder *thisPtr = nullptr;
};

extern TJpg_Decoder TJpgDec;

} // namespace third_party
} // namespace fl

#endif // TJpg_Decoder_H
