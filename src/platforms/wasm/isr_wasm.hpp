/*
  FastLED — WASM ISR Implementation
  ----------------------------------
  WASM implementation of the cross-platform ISR API.
  Reuses the stub platform's POSIX thread-based implementation.

  License: MIT (FastLED)
*/

#pragma once

// ok no namespace fl

// WASM uses the same host-based POSIX thread implementation as stub
#include "platforms/stub/isr_stub.hpp"
