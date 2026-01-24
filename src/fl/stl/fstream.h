#pragma once

#include <fstream>  // ok include
#include <ios>

namespace fl {

// Bring std file stream types into fl namespace
using std::ifstream;  // okay std namespace
using std::ofstream;  // okay std namespace
using std::fstream;  // okay std namespace

// Create constexpr references to std::ios flags in fl::ios namespace
// Note: These are static const members of std::ios_base, accessible via std::ios
namespace ios {
    // Open mode flags (std::ios_base::openmode)
    static constexpr std::ios_base::openmode binary = std::ios_base::binary;  // okay std namespace
    static constexpr std::ios_base::openmode ate = std::ios_base::ate;  // okay std namespace
    static constexpr std::ios_base::openmode in = std::ios_base::in;  // okay std namespace
    static constexpr std::ios_base::openmode out = std::ios_base::out;  // okay std namespace
    static constexpr std::ios_base::openmode trunc = std::ios_base::trunc;  // okay std namespace
    static constexpr std::ios_base::openmode app = std::ios_base::app;  // okay std namespace

    // Seek direction flags (std::ios_base::seekdir)
    static constexpr std::ios_base::seekdir beg = std::ios_base::beg;  // okay std namespace
    static constexpr std::ios_base::seekdir end = std::ios_base::end;  // okay std namespace
    static constexpr std::ios_base::seekdir cur = std::ios_base::cur;  // okay std namespace
}

} // namespace fl
