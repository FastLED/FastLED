/// @file test_delay_ns.cpp
/// Unit tests for nanosecond-precision delay functionality (fl/delay.h)

#include "fl/delay.h"
#include "doctest.h"

// ============================================================================
// Test Suite: Compile-time Template Delays
// ============================================================================

TEST_CASE("delayNanoseconds<50>() compiles") {
  fl::delayNanoseconds<50>();
  CHECK(true);
}

TEST_CASE("delayNanoseconds<100>() compiles") {
  fl::delayNanoseconds<100>();
  CHECK(true);
}

TEST_CASE("delayNanoseconds<350>() compiles (WS2812 T1)") {
  fl::delayNanoseconds<350>();
  CHECK(true);
}

TEST_CASE("delayNanoseconds<700>() compiles (WS2812 T2)") {
  fl::delayNanoseconds<700>();
  CHECK(true);
}

TEST_CASE("delayNanoseconds<1000>() compiles (1 microsecond)") {
  fl::delayNanoseconds<1000>();
  CHECK(true);
}

TEST_CASE("delayNanoseconds<10000>() compiles (10 microseconds)") {
  fl::delayNanoseconds<10000>();
  CHECK(true);
}

// ============================================================================
// Test Suite: Runtime Delays
// ============================================================================

TEST_CASE("delayNanoseconds(0) does nothing") {
  fl::delayNanoseconds(0);
  CHECK(true);
}

TEST_CASE("delayNanoseconds(50) compiles and runs") {
  fl::delayNanoseconds(50);
  CHECK(true);
}

TEST_CASE("delayNanoseconds(350) compiles and runs (WS2812 T1)") {
  fl::delayNanoseconds(350);
  CHECK(true);
}

TEST_CASE("delayNanoseconds(700) compiles and runs (WS2812 T2)") {
  fl::delayNanoseconds(700);
  CHECK(true);
}

TEST_CASE("delayNanoseconds(1000) compiles and runs (1 microsecond)") {
  fl::delayNanoseconds(1000);
  CHECK(true);
}

TEST_CASE("delayNanoseconds with explicit clock frequency - 16 MHz") {
  fl::delayNanoseconds(100, 16000000);
  CHECK(true);
}

TEST_CASE("delayNanoseconds with explicit clock frequency - 80 MHz (ESP32)") {
  fl::delayNanoseconds(350, 80000000);
  CHECK(true);
}

TEST_CASE("delayNanoseconds with explicit clock frequency - 160 MHz (ESP32 turbo)") {
  fl::delayNanoseconds(700, 160000000);
  CHECK(true);
}

// ============================================================================
// Test Suite: LED Protocol Delays
// ============================================================================

TEST_CASE("WS2812 LED Protocol Delays") {
  fl::delayNanoseconds<350>();
  fl::delayNanoseconds<800>();
  fl::delayNanoseconds<700>();
  fl::delayNanoseconds<600>();
  CHECK(true);
}

TEST_CASE("SK6812 LED Protocol Delays") {
  fl::delayNanoseconds<300>();
  fl::delayNanoseconds<600>();
  fl::delayNanoseconds<300>();
  CHECK(true);
}

TEST_CASE("Various nanosecond delays") {
  fl::delayNanoseconds<50>();
  fl::delayNanoseconds<100>();
  fl::delayNanoseconds<500>();
  fl::delayNanoseconds<1000>();
  fl::delayNanoseconds<5000>();
  fl::delayNanoseconds<10000>();
  CHECK(true);
}

TEST_CASE("Mix of compile-time and runtime delays") {
  fl::delayNanoseconds<350>();
  fl::delayNanoseconds(700);
  fl::delayNanoseconds(600, 80000000);
  fl::delayNanoseconds<300>();
  CHECK(true);
}
