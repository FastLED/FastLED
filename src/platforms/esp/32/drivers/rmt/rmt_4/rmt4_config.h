#pragma once

// IWYU pragma: private
// ok no namespace fl - preprocessor-only backend configuration.

// Set to zero to use blocking RMT4 output without a background TX worker.
#ifndef FL_RMT4_ASYNC
#define FL_RMT4_ASYNC 1
#endif

// Optional translator timing counters for hardware diagnosis.
#ifndef FL_RMT4_DIAGNOSTICS
#define FL_RMT4_DIAGNOSTICS 0
#endif
