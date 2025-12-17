/*
TJpg_Decoder.cpp

Created by Bodmer 18/10/19

Latest version here:
https://github.com/Bodmer/TJpg_Decoder
*/

#include "TJpg_Decoder.h"
#include "fl/unused.h"
#include "fl/str.h"
#include "fl/stl/cstring.h"  // for fl::memset() and fl::memcpy()

namespace fl {
namespace third_party {

// Create a class instance to be used by the sketch (defined as extern in header)
TJpg_Decoder TJpgDec;

/***************************************************************************************
** Function name:           TJpg_Decoder
** Description:             Constructor
***************************************************************************************/
TJpg_Decoder::TJpg_Decoder(){
  // Setup a pointer to this class for static functions
  thisPtr = this;
}

/***************************************************************************************
** Function name:           ~TJpg_Decoder
** Description:             Destructor
***************************************************************************************/
TJpg_Decoder::~TJpg_Decoder(){
  // Bye
}

/***************************************************************************************
** Function name:           setSwapBytes
** Description:             Set byte swapping for output
***************************************************************************************/
void TJpg_Decoder::setSwapBytes(bool swapBytes){
  _swap = swapBytes;
}

/***************************************************************************************
** Function name:           setJpgScale
** Description:             Set the reduction scale factor (1, 2, 4 or 8)
***************************************************************************************/
void TJpg_Decoder::setJpgScale(uint8_t scaleFactor)
{
  switch (scaleFactor)
  {
    case 1:
      jpgScale = 0;
      break;
    case 2:
      jpgScale = 1;
      break;
    case 4:
      jpgScale = 2;
      break;
    case 8:
      jpgScale = 3;
      break;
    default:
      jpgScale = 0;
  }
}

/***************************************************************************************
** Function name:           setCallback
** Description:             Set the sketch callback function to render decoded blocks
***************************************************************************************/
void TJpg_Decoder::setCallback(SketchCallback sketchCallback)
{
  tft_output = sketchCallback;
}

/***************************************************************************************
** Function name:           jd_input (declared static)
** Description:             Called by tjpgd.c to get more data
***************************************************************************************/
size_t TJpg_Decoder::jd_input(JDEC* jdec, uint8_t* buf, size_t len)
{
  TJpg_Decoder *thisPtr = TJpgDec.thisPtr;
  FL_UNUSED(jdec);

  // Handle an array input
  if (thisPtr->jpg_source == TJPG_ARRAY) {
    // Avoid running off end of array
    if (thisPtr->array_index + len > thisPtr->array_size) {
      len = thisPtr->array_size - thisPtr->array_index;
    }

    // If buf is valid then copy len bytes to buffer
    if (buf) fl::memcpy(buf, (const uint8_t *)(thisPtr->array_data + thisPtr->array_index), len);

    // Move pointer
    thisPtr->array_index += len;
  }

  return len;
}

/***************************************************************************************
** Function name:           jd_output (declared static)
** Description:             Called by tjpgd.c with an image block for rendering
***************************************************************************************/
// Pass image block back to the sketch for rendering, may be a complete or partial MCU
int TJpg_Decoder::jd_output(JDEC* jdec, void* bitmap, JRECT* jrect)
{

  // This is a static function so create a pointer to access other members of the class
  TJpg_Decoder *thisPtr = TJpgDec.thisPtr;

  FL_UNUSED(jdec);

  // Check if we have a valid thisPtr
  if (!thisPtr) {
    return 0; // Indicate failure
  }

  // Check if we have a callback function
  if (!thisPtr->tft_output) {
    return 0; // Indicate failure
  }

  // Retrieve rendering parameters and add any offset
  int16_t  x = jrect->left + thisPtr->jpeg_x;
  int16_t  y = jrect->top  + thisPtr->jpeg_y;
  uint16_t w = jrect->right  + 1 - jrect->left;
  uint16_t h = jrect->bottom + 1 - jrect->top;

  // Pass the image block and rendering parameters in a callback to the sketch
  return thisPtr->tft_output(x, y, w, h, (uint16_t*)bitmap);
}

/***************************************************************************************
** Function name:           drawJpg
** Description:             Draw a jpg saved in a FLASH memory array
***************************************************************************************/
JRESULT TJpg_Decoder::drawJpg(int32_t x, int32_t y, const uint8_t jpeg_data[], size_t  data_size) {
  JDEC jdec;
  JRESULT jresult = JDR_OK;

  jpg_source = TJPG_ARRAY;
  array_index = 0;
  array_data  = jpeg_data;
  array_size  = data_size;

  jpeg_x = x;
  jpeg_y = y;

  // Set global pointer for static callback (missing in original - bug fix)
  TJpgDec.thisPtr = this;

  jdec.swap = _swap;

  // Analyse input data
  jresult = jd_prepare(&jdec, jd_input, workspace, TJPGD_WORKSPACE_SIZE, 0);

  // Extract image and render
  if (jresult == JDR_OK) {
    // Debug: Check image dimensions from jdec
    // jdec should now have width/height populated
    jresult = jd_decomp(&jdec, jd_output, jpgScale);
  }

  return jresult;
}

/***************************************************************************************
** Function name:           getJpgSize
** Description:             Get width and height of a jpg saved in a FLASH memory array
***************************************************************************************/
JRESULT TJpg_Decoder::getJpgSize(uint16_t *w, uint16_t *h, const uint8_t jpeg_data[], size_t  data_size) {
  JDEC jdec;
  JRESULT jresult = JDR_OK;

  *w = 0;
  *h = 0;

  jpg_source = TJPG_ARRAY;
  array_index = 0;
  array_data  = jpeg_data;
  array_size  = data_size;

  // Set global pointer for static callback
  TJpgDec.thisPtr = this;

  // Analyse input data
  jresult = jd_prepare(&jdec, jd_input, workspace, TJPGD_WORKSPACE_SIZE, 0);

  if (jresult == JDR_OK) {
    *w = jdec.width;
    *h = jdec.height;
  }

  return jresult;
}

} // namespace third_party
} // namespace fl