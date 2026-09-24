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
