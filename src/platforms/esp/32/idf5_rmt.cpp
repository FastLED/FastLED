#include "led_strip/enabled.h"

#if FASTLED_ESP32_COMPONENT_LED_STRIP_BUILT_IN

#include "idf5_rmt.h"
#include "led_strip/led_strip.h"
#include "esp_log.h"
#include "led_strip/demo.h"
#include "led_strip/configure_led.h"

USING_NAMESPACE_LED_STRIP

#define TAG "idf5_rmt.cpp"

void to_esp_modes(LedStripMode mode, led_model_t* out_chipset, led_pixel_format_t* out_rgbw) {
    switch (mode) {
        case WS2812:
            *out_rgbw = LED_PIXEL_FORMAT_GRB;
            *out_chipset = LED_MODEL_WS2812;
            break;
        case kSK6812:
            *out_rgbw = LED_PIXEL_FORMAT_GRB;
            *out_chipset = LED_MODEL_SK6812;
            break;
        case WS2812_RGBW:
            *out_rgbw = LED_PIXEL_FORMAT_GRBW;
            *out_chipset = LED_MODEL_WS2812;
            break;
        case kSK6812_RGBW:
            *out_rgbw = LED_PIXEL_FORMAT_GRBW;
            *out_chipset = LED_MODEL_SK6812;
            break;
        default:
            ESP_LOGE(TAG, "Invalid LedStripMode");
            break;
    }
}

RmtController5::RmtController5(int DATA_PIN, int T1, int T2, int T3) {
    // Stub implementation
    ESP_LOGI(TAG, "RmtController5 constructor called");
}

RmtController5::~RmtController5() {
    // Stub implementation
    ESP_LOGI(TAG, "RmtController5 destructor called");
}

void RmtController5::showPixels(PixelIterator &pixels) {
    // Stub implementation
    ESP_LOGI(TAG, "showPixels called");
}

void RmtController5::ingest(uint8_t val) {
    // Stub implementation
    ESP_LOGI(TAG, "ingest called with value: %d", val);
}

void RmtController5::showPixels() {
    // Stub implementation
    ESP_LOGI(TAG, "showPixels called");
}

bool RmtController5::built_in_driver() {
    // Stub implementation
    ESP_LOGI(TAG, "built_in_driver called");
    return true;
}

uint8_t* RmtController5::getPixelBuffer(int size_in_bytes) {
    // Stub implementation
    ESP_LOGI(TAG, "getPixelBuffer called with size: %d", size_in_bytes);
    return nullptr;
}

void RmtController5::loadPixelDataForStreamEncoding(PixelIterator& pixels) {
    // Stub implementation
    ESP_LOGI(TAG, "loadPixelDataForStreamEncoding called");
}



#endif  // FASTLED_ESP32_COMPONENT_LED_STRIP_BUILT_IN

