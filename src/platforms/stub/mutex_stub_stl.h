// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/stub/mutex_stub_stl.h
/// @brief Stub platform mutex implementation using STL primitives
///
/// This header provides real mutex implementations for multithreaded stub platforms,
/// routing to std::mutex and std::recursive_mutex.

#include <mutex>  // ok include

namespace fl {
namespace platforms {

// Alias to std::mutex for full compatibility with standard library
using mutex = std::mutex;  // okay std namespace
using recursive_mutex = std::recursive_mutex;  // okay std namespace

// Use std::unique_lock for full compatibility with condition variables
template<typename Mutex>
using unique_lock = std::unique_lock<Mutex>;  // okay std namespace

// Lock constructor tag types (re-export from std)
using std::defer_lock_t;  // okay std namespace
using std::try_to_lock_t;  // okay std namespace
using std::adopt_lock_t;  // okay std namespace
using std::defer_lock;  // okay std namespace
using std::try_to_lock;  // okay std namespace
using std::adopt_lock;  // okay std namespace

// Define FASTLED_MULTITHREADED for multithreaded platforms
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 1
#endif

} // namespace platforms
} // namespace fl
