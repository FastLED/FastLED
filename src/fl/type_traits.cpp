
#include "fl/type_traits.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

// Type traits are tricky and can be compiler dependent. We can't rely on
// unit testing since that runs on the host machine. So to any type trait
// that can statically be tested, add below.

// typetrait test
namespace {
void __compile_test() {
    static_assert(fl::is_integral<int>::value, "int should be integral");
    static_assert(fl::is_integral<float>::value == false,
                  "float should not be integral");
    static_assert(fl::is_integral<bool>::value, "bool should be integral");
    static_assert(fl::is_integral<char>::value, "char should be integral");
    static_assert(fl::is_integral<unsigned char>::value,
                  "unsigned char should be integral");
    static_assert(fl::is_integral<signed char>::value,
                  "signed char should be integral");
    static_assert(fl::is_integral<short>::value, "short should be integral");
    static_assert(fl::is_integral<unsigned short>::value,
                  "unsigned short should be integral");
    static_assert(fl::is_integral<long>::value, "long should be integral");
    static_assert(fl::is_integral<unsigned long>::value,
                  "unsigned long should be integral");
    static_assert(fl::is_integral<long long>::value,
                  "long long should be integral");
    static_assert(fl::is_integral<unsigned long long>::value,
                  "unsigned long long should be integral");
    static_assert(fl::is_integral<int *>::value == false,
                  "int* should not be integral");
    static_assert(fl::is_integral<float *>::value == false,
                  "float* should not be integral");
    static_assert(fl::is_integral<void>::value == false,
                  "void should not be integral");
    static_assert(fl::is_integral<void *>::value == false,
                  "void* should not be integral");
    static_assert(fl::is_integral<const int>::value,
                  "const int should be integral");
    static_assert(fl::is_integral<const float>::value == false,
                  "const float should not be integral");
    static_assert(fl::is_integral<const char>::value,
                  "const char should be integral");
    static_assert(fl::is_integral<const unsigned char>::value,
                  "const unsigned char should be integral");
    static_assert(fl::is_integral<const signed char>::value,
                  "const signed char should be integral");
    static_assert(fl::is_integral<const short>::value,
                  "const short should be integral");
    static_assert(fl::is_integral<const unsigned short>::value,
                  "const unsigned short should be integral");
    static_assert(fl::is_integral<const long>::value,
                  "const long should be integral");
    static_assert(fl::is_integral<const unsigned long>::value,
                  "const unsigned long should be integral");
    static_assert(fl::is_integral<const long long>::value,
                  "const long long should be integral");
    static_assert(fl::is_integral<const unsigned long long>::value,
                  "const unsigned long long should be integral");

    // volatile
    static_assert(fl::is_integral<volatile int>::value,
                  "volatile int should be integral");
    static_assert(fl::is_integral<volatile float>::value == false,
                  "volatile float should not be integral");
    static_assert(fl::is_integral<volatile char>::value,
                  "volatile char should be integral");

    // ref
    static_assert(fl::is_integral<unsigned char &>::value,
                  "unsigned char& should be integral");
    static_assert(fl::is_integral<const unsigned char &>::value,
                  "const unsigned char& should be integral");

    // fixed width int types
    static_assert(fl::is_integral<int8_t>::value, "int8_t should be integral");
    static_assert(fl::is_integral<uint8_t>::value,
                  "uint8_t should be integral");
    static_assert(fl::is_integral<int16_t>::value,
                  "int16_t should be integral");
    static_assert(fl::is_integral<uint16_t>::value,
                  "uint16_t should be integral");
    static_assert(fl::is_integral<int32_t>::value,
                  "int32_t should be integral");
    static_assert(fl::is_integral<uint32_t>::value,
                  "uint32_t should be integral");
    static_assert(fl::is_integral<int64_t>::value,
                  "int64_t should be integral");
    static_assert(fl::is_integral<uint64_t>::value,
                  "uint64_t should be integral");
    static_assert(fl::is_integral<int8_t *>::value == false,
                  "int8_t* should not be integral");
    static_assert(fl::is_integral<uint8_t *>::value == false,
                  "uint8_t* should not be integral");
}
} // namespace

#pragma GCC diagnostic pop
