// Static constexpr member definitions for C++11 compatibility
// In C++11, static constexpr members that are ODR-used need out-of-class definitions
// in a single translation unit to avoid duplicate symbol errors.
//
// NOTE: After converting most traits to use enum instead of static constexpr,
// only a few members still need definitions here (those that use template expressions
// or other complex initializers that can't be put in enums).

#include "fl/stl/type_traits.h"
#include "fl/stl/limits.h"
#include "platforms/shared/spi_bitbang/spi_block_8.h"
#include "platforms/shared/spi_bitbang/spi_block_16.h"

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

// type_rank - static constexpr members
template <typename T> constexpr int type_rank<T>::value;
constexpr int type_rank<bool>::value;
constexpr int type_rank<signed char>::value;
constexpr int type_rank<unsigned char>::value;
constexpr int type_rank<char>::value;
constexpr int type_rank<short>::value;
constexpr int type_rank<unsigned short>::value;
constexpr int type_rank<int>::value;
constexpr int type_rank<unsigned int>::value;
constexpr int type_rank<long>::value;
constexpr int type_rank<unsigned long>::value;
constexpr int type_rank<long long>::value;
constexpr int type_rank<unsigned long long>::value;
constexpr int type_rank<float>::value;
constexpr int type_rank<double>::value;
constexpr int type_rank<long double>::value;

// SpiBlock8
constexpr int SpiBlock8::NUM_DATA_PINS;
constexpr uint16_t SpiBlock8::MAX_BUFFER_SIZE;

// SpiBlock16
constexpr int SpiBlock16::NUM_DATA_PINS;
constexpr uint16_t SpiBlock16::MAX_BUFFER_SIZE;

} // namespace fl
