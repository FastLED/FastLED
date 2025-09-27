/// @file Esp32S3LcdParallel.ino
/// ESP32-S3 LCD/I80 Parallel LED Driver Demo
/// 
/// This example demonstrates the ESP32-S3 LCD/I80 parallel LED driver
/// with mixed chipset support. It drives up to 16 WS28xx LED strips
/// in parallel with precise 50ns timing resolution.
/// 
/// Features demonstrated:
/// - Mixed chipset operation (WS2812 + WS2816 in same frame)
/// - 16 parallel lanes with independent timing
/// - Double-buffered PSRAM operation
/// - High-performance bit-sliced encoding
/// - Real-time performance monitoring

#ifndef ESP32
#define IS_ESP32_S3 0
#else
#include "sdkconfig.h"

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define IS_ESP32_S3 1
#else
#define IS_ESP32_S3 0
#endif
#endif

#if IS_ESP32_S3
#include "Esp32S3LcdParallel.h"
#else
#include "platforms/sketch_fake.hpp"
#endif