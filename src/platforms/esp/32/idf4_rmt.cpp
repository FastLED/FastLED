
#ifdef ESP32
#ifndef FASTLED_ESP32_I2S

#define FASTLED_INTERNAL

#include "FastLED.h"
#include "idf4_rmt.h"
#include "idf4_rmt_impl.h"


void RmtController::init(gpio_num_t pin, bool built_in_driver)
{
    ESP32RMTController::init(pin, built_in_driver);
}

RmtController::RmtController(
    int DATA_PIN,
    int T1, int T2, int T3,
    int maxChannel, bool built_in_driver,
    bool is_rgbw, RGBW_MODE mode, uint16_t white_color_temp)
{
    mIsRgbw = is_rgbw;
    mRgbwMode = mode;
    mColorTemp = white_color_temp;
    pImpl = new ESP32RMTController(DATA_PIN, T1, T2, T3, maxChannel, built_in_driver);
}
RmtController::~RmtController()
{
    delete pImpl;
}

void RmtController::showPixels()
{
    pImpl->showPixels();
}

void RmtController::ingest(uint8_t val)
{
    pImpl->ingest(val);
}

uint8_t *RmtController::getPixelBuffer(int size_in_bytes)
{
    return pImpl->getPixelBuffer(size_in_bytes);
}

bool RmtController::built_in_driver()
{
    return pImpl->mBuiltInDriver;
}

void RmtController::initPulseBuffer(int size_in_bytes)
{
    pImpl->initPulseBuffer(size_in_bytes);
}


#endif // ! FASTLED_ESP32_I2S

#endif // ESP32
