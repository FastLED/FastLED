/// @file BulkArchitectureTest.ino
/// @brief Test the ESP32-P4 PARLIO bulk architecture
///
/// This example validates the bulk architecture for the PARLIO driver:
/// - Single strip transmission
/// - Multi-strip batching (same chipset)
/// - Chipset changes (WS2812 → SK6812)
///
/// Expected Behavior:
/// - All strips should display correct colors without flickering
/// - Multi-strip batching should transmit sequentially via hub coordination
/// - No crashes or memory leaks during operation
///
/// Architecture Under Test:
/// ClocklessController_Parlio_Esp32P4 (user API)
///   ↓
/// ParlioChannel (data holder)
///   ↓
/// ParlioGroup (data packing, waveform generation)
///   ↓
/// ParlioHub (scheduling, gatekeeper)
///   ↓
/// ParlioEngine (DMA hardware controller)

// @filter: (target is -DESP32P4)

#include "BulkArchitectureTest.h"
