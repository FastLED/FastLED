// Static constexpr member definitions for C++11 compatibility
// In C++11, static constexpr members that are ODR-used need out-of-class definitions
// in a single translation unit to avoid duplicate symbol errors.
//
// NOTE: After converting most traits to use enum instead of static constexpr,
// only a few members still need definitions here (those that use template expressions
// or other complex initializers that can't be put in enums).

#include "fl/type_traits.h"
#include "fl/limits.h"
#include "platforms/shared/spi_bitbang/spi_block_8.h"

namespace fl {

// numeric_limits - Only define static constexpr members that use template expressions
// (enum members don't need definitions)

// char - is_signed uses expression, digits/digits10 use template expressions
constexpr bool numeric_limits<char>::is_signed;
constexpr int numeric_limits<char>::digits;
constexpr int numeric_limits<char>::digits10;

// signed char
constexpr int numeric_limits<signed char>::digits;
constexpr int numeric_limits<signed char>::digits10;

// unsigned char
constexpr int numeric_limits<unsigned char>::digits;
constexpr int numeric_limits<unsigned char>::digits10;

// short
constexpr int numeric_limits<short>::digits;
constexpr int numeric_limits<short>::digits10;

// unsigned short
constexpr int numeric_limits<unsigned short>::digits;
constexpr int numeric_limits<unsigned short>::digits10;

// int
constexpr int numeric_limits<int>::digits;
constexpr int numeric_limits<int>::digits10;

// unsigned int
constexpr int numeric_limits<unsigned int>::digits;
constexpr int numeric_limits<unsigned int>::digits10;

// long
constexpr int numeric_limits<long>::digits;
constexpr int numeric_limits<long>::digits10;

// unsigned long
constexpr int numeric_limits<unsigned long>::digits;
constexpr int numeric_limits<unsigned long>::digits10;

// long long
constexpr int numeric_limits<long long>::digits;
constexpr int numeric_limits<long long>::digits10;

// unsigned long long
constexpr int numeric_limits<unsigned long long>::digits;
constexpr int numeric_limits<unsigned long long>::digits10;

// float - static constexpr members
constexpr int numeric_limits<float>::max_digits10;
constexpr int numeric_limits<float>::max_exponent;
constexpr int numeric_limits<float>::max_exponent10;
constexpr int numeric_limits<float>::min_exponent;
constexpr int numeric_limits<float>::min_exponent10;

// double - static constexpr members
constexpr int numeric_limits<double>::max_digits10;
constexpr int numeric_limits<double>::max_exponent;
constexpr int numeric_limits<double>::max_exponent10;
constexpr int numeric_limits<double>::min_exponent;
constexpr int numeric_limits<double>::min_exponent10;

// long double - static constexpr members (use template expressions)
constexpr int numeric_limits<long double>::digits;
constexpr int numeric_limits<long double>::digits10;
constexpr int numeric_limits<long double>::max_digits10;

// SpiBlock8
constexpr int SpiBlock8::NUM_DATA_PINS;
constexpr uint16_t SpiBlock8::MAX_BUFFER_SIZE;

} // namespace fl
