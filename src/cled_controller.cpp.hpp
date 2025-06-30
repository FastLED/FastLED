/// @file cled_controller.cpp
/// base definitions used by led controllers for writing out led data

#define FASTLED_INTERNAL
#include "FastLED.h"

#include "cled_controller.h"

FASTLED_NAMESPACE_BEGIN

CLEDController::~CLEDController() = default;

/// Create an led controller object, add it to the chain of controllers
CLEDController::CLEDController() : m_Data(NULL), m_ColorCorrection(UncorrectedColor), m_ColorTemperature(UncorrectedTemperature), m_DitherMode(BINARY_DITHER), m_nLeds(0) {
    m_pNext = NULL;
    if(m_pHead==NULL) { m_pHead = this; }
    if(m_pTail != NULL) { m_pTail->m_pNext = this; }
    m_pTail = this;
}



void CLEDController::clearLedDataInternal(int nLeds) {
    if(m_Data) {
        nLeds = (nLeds < 0) ? m_nLeds : nLeds;
        nLeds = (nLeds > m_nLeds) ? m_nLeds : nLeds;
        memset((void*)m_Data, 0, sizeof(struct CRGB) * nLeds);
    }

}

ColorAdjustment CLEDController::getAdjustmentData(uint8_t brightness) {
    // *premixed = getAdjustment(brightness);
    // if (color_correction) {
    //     *color_correction = getAdjustment(255);
    // }
    #if FASTLED_HD_COLOR_MIXING
    ColorAdjustment out = {this->getAdjustment(brightness), this->getAdjustment(255), brightness};
    #else
    ColorAdjustment out = {getAdjustment(brightness)};
    #endif
    return out;
}


FASTLED_NAMESPACE_END


