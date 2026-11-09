#pragma once

// IWYU pragma: private

// ok no namespace fl

// Static allocation is the opt-in contract for one fixed FastLED TX strip.
// Keep this header independent of ESP-IDF headers and do not size the static
// ledger to every physical TX/RX channel; dynamic mode retains the full ledger.
#define FL_RMT_ALLOCATION_LEDGER_CAPACITY 1

// Preferred spelling for new code. Keep the old public setting as a
// compatibility alias so existing sketches and build flags continue to work.
#if defined(FASTLED_RMT_STATIC_ALLOCATION) && defined(FL_RMT_STATIC_ALLOCATION)
#if FASTLED_RMT_STATIC_ALLOCATION != FL_RMT_STATIC_ALLOCATION
#error "FASTLED_RMT_STATIC_ALLOCATION and FL_RMT_STATIC_ALLOCATION must match"
#endif
#endif

#ifndef FL_RMT_STATIC_ALLOCATION
#ifdef FASTLED_RMT_STATIC_ALLOCATION
#define FL_RMT_STATIC_ALLOCATION FASTLED_RMT_STATIC_ALLOCATION
#else
#define FL_RMT_STATIC_ALLOCATION 0
#endif
#endif

#ifndef FASTLED_RMT_STATIC_ALLOCATION
#define FASTLED_RMT_STATIC_ALLOCATION FL_RMT_STATIC_ALLOCATION
#endif

// RMT TX/RX DMA opt-out (#5161). On chips with RMT DMA (ESP32-S3) the first
// channel takes the single DMA slot, which allocates a 4 KB DMA-capable
// internal-RAM block regardless of strip length. Build with
// -DFL_RMT5_USE_DMA=0 to keep every channel on on-chip RMT memory (double
// buffered, refilled by interrupt) and save that heap on boards without PSRAM.
#ifndef FL_RMT5_USE_DMA
#define FL_RMT5_USE_DMA 1
#endif

// RMT TX DMA sizing policy (#5161 follow-up). Even when DMA is enabled, the
// driver only takes the DMA channel when the encoded frame is long enough to
// benefit from it. A DMA channel costs a fixed 4 KB DMA-capable internal-RAM
// block (ESP-IDF's recommended mem_block_symbols = 1024); a short strip fits in
// a fraction of that block and gains nothing from streaming, so it stays on
// on-chip RMT memory instead. Set FL_RMT5_DMA_AUTO=0 to restore "always DMA
// when the hardware supports it". FL_RMT5_USE_DMA=0 still forces non-DMA.
#ifndef FL_RMT5_DMA_AUTO
#define FL_RMT5_DMA_AUTO 1
#endif

// Encoded RMT symbols at or below which a strip counts as "short" and skips the
// DMA channel. One RMT symbol is one bit, so 1024 symbols is 128 bytes of
// pixel data -- about 42 WS2812 RGB LEDs, i.e. the size of the DMA block
// itself. Override with -DFL_RMT5_DMA_MIN_SYMBOLS=<n>.
#ifndef FL_RMT5_DMA_MIN_SYMBOLS
#define FL_RMT5_DMA_MIN_SYMBOLS 1024
#endif
