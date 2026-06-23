#pragma once
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declarations
class ostream;
class sstream;

//-----------------------------------------------------------------------------
// I/O Formatting Flags
//-----------------------------------------------------------------------------
enum class fmtflags {
    dec = 0,   // decimal base
    hex = 1,   // hexadecimal base
    oct = 2    // octal base
};

//-----------------------------------------------------------------------------
// I/O Manipulators for Stream Formatting
//-----------------------------------------------------------------------------

// Hexadecimal manipulator
struct hex_t {};
extern const hex_t hex;

ostream& operator<<(ostream& os, const hex_t&) FL_NOEXCEPT;
sstream& operator<<(sstream& ss, const hex_t&) FL_NOEXCEPT;

// Decimal manipulator
struct dec_t {};
extern const dec_t dec;

ostream& operator<<(ostream& os, const dec_t&) FL_NOEXCEPT;
sstream& operator<<(sstream& ss, const dec_t&) FL_NOEXCEPT;

// Octal manipulator (for completeness)
struct oct_t {};
extern const oct_t oct;

ostream& operator<<(ostream& os, const oct_t&) FL_NOEXCEPT;
sstream& operator<<(sstream& ss, const oct_t&) FL_NOEXCEPT;

} // namespace fl
