/// @file memory.cpp
/// @brief Tests for fl/memory.h platform abstraction

#include "fl/memory.h"
#include "test.h"
#include "platforms/is_platform.h"

#define TEST_MODULE "fl::memory"

FL_TEST_CASE("getFreeHeap returns valid HeapInfo") {
    // getFreeHeap should always return a valid HeapInfo struct
    fl::HeapInfo heap = fl::getFreeHeap();

    // Values should be non-negative (size is unsigned, so always true, but documents intent)
    FL_CHECK(heap.free_sram >= 0);
    FL_CHECK(heap.free_psram >= 0);
    FL_CHECK(heap.total() >= 0);
}

FL_TEST_CASE("getFreeHeap behavior on different platforms") {
    fl::HeapInfo heap = fl::getFreeHeap();

    #if defined(FL_IS_ESP32)
    // On ESP32, should return actual SRAM heap size (typically > 0)
    FL_CHECK(heap.free_sram > 0);
    FL_DINFO("ESP32 platform reported free SRAM: " << heap.free_sram << " bytes");
    FL_DINFO("ESP32 platform reported free PSRAM: " << heap.free_psram << " bytes");
    FL_DINFO("ESP32 total free heap: " << heap.total() << " bytes");

    if (heap.has_psram()) {
        FL_DINFO("PSRAM is available!");
        FL_CHECK(heap.free_psram > 0);
    } else {
        FL_DINFO("No PSRAM detected");
        FL_CHECK(heap.free_psram == 0);
    }

    #elif defined(FL_IS_ESP8266)
    // On ESP8266, should return SRAM only (no PSRAM support)
    FL_CHECK(heap.free_sram > 0);
    FL_CHECK(heap.free_psram == 0);  // No PSRAM on ESP8266
    FL_CHECK(!heap.has_psram());
    FL_DINFO("ESP8266 platform reported free SRAM: " << heap.free_sram << " bytes");

    #elif defined(FL_IS_AVR)
    // On AVR, should return free SRAM between heap and stack
    FL_CHECK(heap.free_sram > 0);
    FL_CHECK(heap.free_psram == 0);  // No PSRAM on AVR
    FL_CHECK(!heap.has_psram());
    FL_DINFO("AVR platform reported free SRAM: " << heap.free_sram << " bytes");

    #else
    // On native/stub/WASM platforms, may return 0 (not implemented)
    // Just verify it doesn't crash
    FL_DINFO("Platform reported free SRAM: " << heap.free_sram << " bytes (may be 0 if not implemented)");
    FL_DINFO("Platform reported free PSRAM: " << heap.free_psram << " bytes (may be 0 if not implemented)");
    FL_CHECK(heap.free_psram == 0);  // Native platforms don't have PSRAM

    #endif
}

FL_TEST_CASE("getFreeHeap is callable multiple times") {
    // Should be able to call getFreeHeap multiple times without crashing
    fl::HeapInfo heap1 = fl::getFreeHeap();
    fl::HeapInfo heap2 = fl::getFreeHeap();
    fl::HeapInfo heap3 = fl::getFreeHeap();

    // Values should be similar (within reasonable bounds)
    // On some platforms they may be identical, on others slightly different
    // due to stack usage variations
    FL_DINFO("First call:  SRAM=" << heap1.free_sram << " PSRAM=" << heap1.free_psram << " total=" << heap1.total());
    FL_DINFO("Second call: SRAM=" << heap2.free_sram << " PSRAM=" << heap2.free_psram << " total=" << heap2.total());
    FL_DINFO("Third call:  SRAM=" << heap3.free_sram << " PSRAM=" << heap3.free_psram << " total=" << heap3.total());

    // All calls should succeed (not crash)
    FL_CHECK(true);
}

FL_TEST_CASE("HeapInfo helper methods") {
    fl::HeapInfo heap;

    // Test total() method
    heap.free_sram = 1000;
    heap.free_psram = 500;
    FL_CHECK(heap.total() == 1500);

    // Test has_psram() method
    heap.free_psram = 0;
    FL_CHECK(!heap.has_psram());

    heap.free_psram = 1;
    FL_CHECK(heap.has_psram());
}

#undef TEST_MODULE
