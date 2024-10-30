// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2024, Benoit BLANCHON
// MIT License

#pragma once


#ifndef FASTLED_JSON_GUARD
#error "You must include json.h instead of json.hpp"
#endif

    

#ifdef __cplusplus

#if __cplusplus < 201103L && (!defined(_MSC_VER) || _MSC_VER < 1910)
#  error ArduinoJson requires C++11 or newer. Configure your compiler for C++11 or downgrade ArduinoJson to 6.20.
#endif
#ifndef ARDUINOJSON_ENABLE_STD_STREAM
#  ifdef __has_include
#    if __has_include(<istream>) && \
    __has_include(<ostream>) && \
    !defined(min) && \
    !defined(max)
#      define ARDUINOJSON_ENABLE_STD_STREAM 1
#    else
#      define ARDUINOJSON_ENABLE_STD_STREAM 0
#    endif
#  else
#    ifdef ARDUINO
#      define ARDUINOJSON_ENABLE_STD_STREAM 0
#    else
#      define ARDUINOJSON_ENABLE_STD_STREAM 1
#    endif
#  endif
#endif
#ifndef ARDUINOJSON_ENABLE_STD_STRING
#  ifdef __has_include
#    if __has_include(<string>) && !defined(min) && !defined(max)
#      define ARDUINOJSON_ENABLE_STD_STRING 1
#    else
#      define ARDUINOJSON_ENABLE_STD_STRING 0
#    endif
#  else
#    ifdef ARDUINO
#      define ARDUINOJSON_ENABLE_STD_STRING 0
#    else
#      define ARDUINOJSON_ENABLE_STD_STRING 1
#    endif
#  endif
#endif
#ifndef ARDUINOJSON_ENABLE_STRING_VIEW
#  ifdef __has_include
#    if __has_include(<string_view>) && __cplusplus >= 201703L
#      define ARDUINOJSON_ENABLE_STRING_VIEW 1
#    else
#      define ARDUINOJSON_ENABLE_STRING_VIEW 0
#    endif
#  else
#    define ARDUINOJSON_ENABLE_STRING_VIEW 0
#  endif
#endif
#ifndef ARDUINOJSON_SIZEOF_POINTER
#  if defined(__SIZEOF_POINTER__)
#    define ARDUINOJSON_SIZEOF_POINTER __SIZEOF_POINTER__
#  elif defined(_WIN64) && _WIN64
#    define ARDUINOJSON_SIZEOF_POINTER 8  // 64 bits
#  else
#    define ARDUINOJSON_SIZEOF_POINTER 4  // assume 32 bits otherwise
#  endif
#endif
#ifndef ARDUINOJSON_USE_DOUBLE
#  if ARDUINOJSON_SIZEOF_POINTER >= 4  // 32 & 64 bits systems
#    define ARDUINOJSON_USE_DOUBLE 1
#  else
#    define ARDUINOJSON_USE_DOUBLE 0
#  endif
#endif
#ifndef ARDUINOJSON_USE_LONG_LONG
#  if ARDUINOJSON_SIZEOF_POINTER >= 4  // 32 & 64 bits systems
#    define ARDUINOJSON_USE_LONG_LONG 1
#  else
#    define ARDUINOJSON_USE_LONG_LONG 0
#  endif
#endif
#ifndef ARDUINOJSON_DEFAULT_NESTING_LIMIT
#  define ARDUINOJSON_DEFAULT_NESTING_LIMIT 10
#endif
#ifndef ARDUINOJSON_SLOT_ID_SIZE
#  if ARDUINOJSON_SIZEOF_POINTER <= 2
#    define ARDUINOJSON_SLOT_ID_SIZE 1
#  elif ARDUINOJSON_SIZEOF_POINTER == 4
#    define ARDUINOJSON_SLOT_ID_SIZE 2
#  else
#    define ARDUINOJSON_SLOT_ID_SIZE 4
#  endif
#endif
#ifndef ARDUINOJSON_POOL_CAPACITY
#  if ARDUINOJSON_SLOT_ID_SIZE == 1
#    define ARDUINOJSON_POOL_CAPACITY 16  // 96 bytes
#  elif ARDUINOJSON_SLOT_ID_SIZE == 2
#    define ARDUINOJSON_POOL_CAPACITY 128  // 1024 bytes
#  else
#    define ARDUINOJSON_POOL_CAPACITY 256  // 4096 bytes
#  endif
#endif
#ifndef ARDUINOJSON_INITIAL_POOL_COUNT
#  define ARDUINOJSON_INITIAL_POOL_COUNT 4
#endif
#ifndef ARDUINOJSON_AUTO_SHRINK
#  if ARDUINOJSON_SIZEOF_POINTER <= 2
#    define ARDUINOJSON_AUTO_SHRINK 0
#  else
#    define ARDUINOJSON_AUTO_SHRINK 1
#  endif
#endif
#ifndef ARDUINOJSON_STRING_LENGTH_SIZE
#  if ARDUINOJSON_SIZEOF_POINTER <= 2
#    define ARDUINOJSON_STRING_LENGTH_SIZE 1  // up to 255 characters
#  else
#    define ARDUINOJSON_STRING_LENGTH_SIZE 2  // up to 65535 characters
#  endif
#endif
#ifdef ARDUINO
#  ifndef ARDUINOJSON_ENABLE_ARDUINO_STRING
#    define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#  endif
#  ifndef ARDUINOJSON_ENABLE_ARDUINO_STREAM
#    define ARDUINOJSON_ENABLE_ARDUINO_STREAM 1
#  endif
#  ifndef ARDUINOJSON_ENABLE_ARDUINO_PRINT
#    define ARDUINOJSON_ENABLE_ARDUINO_PRINT 1
#  endif
#  ifndef ARDUINOJSON_ENABLE_PROGMEM
#    define ARDUINOJSON_ENABLE_PROGMEM 1
#  endif
#else  // ARDUINO
#  ifndef ARDUINOJSON_ENABLE_ARDUINO_STRING
#    define ARDUINOJSON_ENABLE_ARDUINO_STRING 0
#  endif
#  ifndef ARDUINOJSON_ENABLE_ARDUINO_STREAM
#    define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#  endif
#  ifndef ARDUINOJSON_ENABLE_ARDUINO_PRINT
#    define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#  endif
#  ifndef ARDUINOJSON_ENABLE_PROGMEM
#    ifdef __AVR__
#      define ARDUINOJSON_ENABLE_PROGMEM 1
#    else
#      define ARDUINOJSON_ENABLE_PROGMEM 0
#    endif
#  endif
#endif  // ARDUINO
#ifndef ARDUINOJSON_DECODE_UNICODE
#  define ARDUINOJSON_DECODE_UNICODE 1
#endif
#ifndef ARDUINOJSON_ENABLE_COMMENTS
#  define ARDUINOJSON_ENABLE_COMMENTS 0
#endif
#ifndef ARDUINOJSON_ENABLE_NAN
#  define ARDUINOJSON_ENABLE_NAN 0
#endif
#ifndef ARDUINOJSON_ENABLE_INFINITY
#  define ARDUINOJSON_ENABLE_INFINITY 0
#endif
#ifndef ARDUINOJSON_POSITIVE_EXPONENTIATION_THRESHOLD
#  define ARDUINOJSON_POSITIVE_EXPONENTIATION_THRESHOLD 1e7
#endif
#ifndef ARDUINOJSON_NEGATIVE_EXPONENTIATION_THRESHOLD
#  define ARDUINOJSON_NEGATIVE_EXPONENTIATION_THRESHOLD 1e-5
#endif
#ifndef ARDUINOJSON_LITTLE_ENDIAN
#  if defined(_MSC_VER) ||                           \
      (defined(__BYTE_ORDER__) &&                    \
       __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || \
      defined(__LITTLE_ENDIAN__) || defined(__i386) || defined(__x86_64)
#    define ARDUINOJSON_LITTLE_ENDIAN 1
#  else
#    define ARDUINOJSON_LITTLE_ENDIAN 0
#  endif
#endif
#ifndef ARDUINOJSON_ENABLE_ALIGNMENT
#  if defined(__AVR)
#    define ARDUINOJSON_ENABLE_ALIGNMENT 0
#  else
#    define ARDUINOJSON_ENABLE_ALIGNMENT 1
#  endif
#endif
#ifndef ARDUINOJSON_TAB
#  define ARDUINOJSON_TAB "  "
#endif
#ifndef ARDUINOJSON_STRING_BUFFER_SIZE
#  define ARDUINOJSON_STRING_BUFFER_SIZE 32
#endif
#ifndef ARDUINOJSON_DEBUG
#  ifdef __PLATFORMIO_BUILD_DEBUG__
#    define ARDUINOJSON_DEBUG 1
#  else
#    define ARDUINOJSON_DEBUG 0
#  endif
#endif
#if ARDUINOJSON_USE_LONG_LONG || ARDUINOJSON_USE_DOUBLE
#  define ARDUINOJSON_USE_EXTENSIONS 1
#else
#  define ARDUINOJSON_USE_EXTENSIONS 0
#endif
#if defined(nullptr)
#  error nullptr is defined as a macro. Remove the faulty #define or #undef nullptr
#endif
#if ARDUINOJSON_ENABLE_ARDUINO_STRING || ARDUINOJSON_ENABLE_ARDUINO_STREAM || \
    ARDUINOJSON_ENABLE_ARDUINO_PRINT ||                                       \
    (ARDUINOJSON_ENABLE_PROGMEM && defined(ARDUINO))
#include <Arduino.h>  // ok include
#endif
#if !ARDUINOJSON_DEBUG
#  ifdef __clang__
#    pragma clang system_header
#  elif defined __GNUC__
#    pragma GCC system_header
#  endif
#endif
#define ARDUINOJSON_CONCAT_(A, B) A##B
#define ARDUINOJSON_CONCAT2(A, B) ARDUINOJSON_CONCAT_(A, B)
#define ARDUINOJSON_CONCAT3(A, B, C) \
  ARDUINOJSON_CONCAT2(ARDUINOJSON_CONCAT2(A, B), C)
#define ARDUINOJSON_CONCAT4(A, B, C, D) \
  ARDUINOJSON_CONCAT2(ARDUINOJSON_CONCAT3(A, B, C), D)
#define ARDUINOJSON_CONCAT5(A, B, C, D, E) \
  ARDUINOJSON_CONCAT2(ARDUINOJSON_CONCAT4(A, B, C, D), E)
#define ARDUINOJSON_BIN2ALPHA_0000() A
#define ARDUINOJSON_BIN2ALPHA_0001() B
#define ARDUINOJSON_BIN2ALPHA_0010() C
#define ARDUINOJSON_BIN2ALPHA_0011() D
#define ARDUINOJSON_BIN2ALPHA_0100() E
#define ARDUINOJSON_BIN2ALPHA_0101() F
#define ARDUINOJSON_BIN2ALPHA_0110() G
#define ARDUINOJSON_BIN2ALPHA_0111() H
#define ARDUINOJSON_BIN2ALPHA_1000() I
#define ARDUINOJSON_BIN2ALPHA_1001() J
#define ARDUINOJSON_BIN2ALPHA_1010() K
#define ARDUINOJSON_BIN2ALPHA_1011() L
#define ARDUINOJSON_BIN2ALPHA_1100() M
#define ARDUINOJSON_BIN2ALPHA_1101() N
#define ARDUINOJSON_BIN2ALPHA_1110() O
#define ARDUINOJSON_BIN2ALPHA_1111() P
#define ARDUINOJSON_BIN2ALPHA_(A, B, C, D) ARDUINOJSON_BIN2ALPHA_##A##B##C##D()
#define ARDUINOJSON_BIN2ALPHA(A, B, C, D) ARDUINOJSON_BIN2ALPHA_(A, B, C, D)
#define ARDUINOJSON_VERSION "7.2.0"
#define ARDUINOJSON_VERSION_MAJOR 7
#define ARDUINOJSON_VERSION_MINOR 2
#define ARDUINOJSON_VERSION_REVISION 0
#define ARDUINOJSON_VERSION_MACRO V720
#ifndef ARDUINOJSON_VERSION_NAMESPACE
#  define ARDUINOJSON_VERSION_NAMESPACE                               \
    ARDUINOJSON_CONCAT5(                                              \
        ARDUINOJSON_VERSION_MACRO,                                    \
        ARDUINOJSON_BIN2ALPHA(ARDUINOJSON_ENABLE_PROGMEM,             \
                              ARDUINOJSON_USE_LONG_LONG,              \
                              ARDUINOJSON_USE_DOUBLE, 1),             \
        ARDUINOJSON_BIN2ALPHA(                                        \
            ARDUINOJSON_ENABLE_NAN, ARDUINOJSON_ENABLE_INFINITY,      \
            ARDUINOJSON_ENABLE_COMMENTS, ARDUINOJSON_DECODE_UNICODE), \
        ARDUINOJSON_SLOT_ID_SIZE, ARDUINOJSON_STRING_LENGTH_SIZE)
#endif
#define ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE \
  namespace FLArduinoJson {                  \
  inline namespace ARDUINOJSON_VERSION_NAMESPACE {
#define ARDUINOJSON_END_PUBLIC_NAMESPACE \
  }                                      \
  }
#define ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE        \
  namespace FLArduinoJson {                          \
  inline namespace ARDUINOJSON_VERSION_NAMESPACE { \
  namespace detail {
#define ARDUINOJSON_END_PRIVATE_NAMESPACE \
  }                                       \
  }                                       \
  }
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
template <typename T, typename Enable = void>
struct Converter;
ARDUINOJSON_END_PUBLIC_NAMESPACE
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename T1, typename T2>
class InvalidConversion;  // Error here? See https://arduinojson.org/v7/invalid-conversion/
ARDUINOJSON_END_PRIVATE_NAMESPACE
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
class Allocator {
 public:
  virtual void* allocate(size_t size) = 0;
  virtual void deallocate(void* ptr) = 0;
  virtual void* reallocate(void* ptr, size_t new_size) = 0;
 protected:
  ~Allocator() = default;
};
namespace detail {
class DefaultAllocator : public Allocator {
 public:
  void* allocate(size_t size) override {
    return malloc(size);
  }
  void deallocate(void* ptr) override {
    free(ptr);
  }
  void* reallocate(void* ptr, size_t new_size) override {
    return realloc(ptr, new_size);
  }
  static Allocator* instance() {
    static DefaultAllocator allocator;
    return &allocator;
  }
 private:
  DefaultAllocator() = default;
  ~DefaultAllocator() = default;
};
}  // namespace detail
ARDUINOJSON_END_PUBLIC_NAMESPACE
#if ARDUINOJSON_DEBUG
#include <assert.h>  // ok include
#  define ARDUINOJSON_ASSERT(X) assert(X)
#else
#  define ARDUINOJSON_ASSERT(X) ((void)0)
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <int Bits>
struct uint_;
template <>
struct uint_<8> {
  typedef uint8_t type;
};
template <>
struct uint_<16> {
  typedef uint16_t type;
};
template <>
struct uint_<32> {
  typedef uint32_t type;
};
template <int Bits>
using uint_t = typename uint_<Bits>::type;
using SlotId = uint_t<ARDUINOJSON_SLOT_ID_SIZE * 8>;
using SlotCount = SlotId;
const SlotId NULL_SLOT = SlotId(-1);
template <typename T>
class Slot {
 public:
  Slot() : ptr_(nullptr), id_(NULL_SLOT) {}
  Slot(T* p, SlotId id) : ptr_(p), id_(id) {
    ARDUINOJSON_ASSERT((p == nullptr) == (id == NULL_SLOT));
  }
  explicit operator bool() const {
    return ptr_ != nullptr;
  }
  SlotId id() const {
    return id_;
  }
  T* ptr() const {
    return ptr_;
  }
  T* operator->() const {
    ARDUINOJSON_ASSERT(ptr_ != nullptr);
    return ptr_;
  }
 private:
  T* ptr_;
  SlotId id_;
};
template <typename T>
class MemoryPool {
 public:
  void create(SlotCount cap, Allocator* allocator) {
    ARDUINOJSON_ASSERT(cap > 0);
    slots_ = reinterpret_cast<T*>(allocator->allocate(slotsToBytes(cap)));
    capacity_ = slots_ ? cap : 0;
    usage_ = 0;
  }
  void destroy(Allocator* allocator) {
    if (slots_)
      allocator->deallocate(slots_);
    slots_ = nullptr;
    capacity_ = 0;
    usage_ = 0;
  }
  Slot<T> allocSlot() {
    if (!slots_)
      return {};
    if (usage_ >= capacity_)
      return {};
    auto index = usage_++;
    return {slots_ + index, SlotId(index)};
  }
  T* getSlot(SlotId id) const {
    ARDUINOJSON_ASSERT(id < usage_);
    return slots_ + id;
  }
  void clear() {
    usage_ = 0;
  }
  void shrinkToFit(Allocator* allocator) {
    auto newSlots = reinterpret_cast<T*>(
        allocator->reallocate(slots_, slotsToBytes(usage_)));
    if (newSlots) {
      slots_ = newSlots;
      capacity_ = usage_;
    }
  }
  SlotCount usage() const {
    return usage_;
  }
  static SlotCount bytesToSlots(size_t n) {
    return static_cast<SlotCount>(n / sizeof(T));
  }
  static size_t slotsToBytes(SlotCount n) {
    return n * sizeof(T);
  }
 private:
  SlotCount capacity_;
  SlotCount usage_;
  T* slots_;
};
template <bool Condition, class TrueType, class FalseType>
struct conditional {
  typedef TrueType type;
};
template <class TrueType, class FalseType>
struct conditional<false, TrueType, FalseType> {
  typedef FalseType type;
};
template <bool Condition, class TrueType, class FalseType>
using conditional_t =
    typename conditional<Condition, TrueType, FalseType>::type;
template <bool Condition, typename T = void>
struct enable_if {};
template <typename T>
struct enable_if<true, T> {
  typedef T type;
};
template <bool Condition, typename T = void>
using enable_if_t = typename enable_if<Condition, T>::type;
template <typename Sig>
struct function_traits;
template <typename ReturnType, typename Arg1>
struct function_traits<ReturnType (*)(Arg1)> {
  using return_type = ReturnType;
  using arg1_type = Arg1;
};
template <typename ReturnType, typename Arg1, typename Arg2>
struct function_traits<ReturnType (*)(Arg1, Arg2)> {
  using return_type = ReturnType;
  using arg1_type = Arg1;
  using arg2_type = Arg2;
};
template <typename T, T v>
struct integral_constant {
  static const T value = v;
};
typedef integral_constant<bool, true> true_type;
typedef integral_constant<bool, false> false_type;
template <typename T>
struct is_array : false_type {};
template <typename T>
struct is_array<T[]> : true_type {};
template <typename T, size_t N>
struct is_array<T[N]> : true_type {};
template <typename T>
struct remove_reference {
  typedef T type;
};
template <typename T>
struct remove_reference<T&> {
  typedef T type;
};
template <typename T>
using remove_reference_t = typename remove_reference<T>::type;
template <typename TBase, typename TDerived>
class is_base_of {
 protected:  // <- to avoid GCC's "all member functions in class are private"
  static int probe(const TBase*);
  static char probe(...);
 public:
  static const bool value =
      sizeof(probe(reinterpret_cast<remove_reference_t<TDerived>*>(0))) ==
      sizeof(int);
};
template <typename T>
T&& declval();
template <typename T>
struct is_class {
 protected:  // <- to avoid GCC's "all member functions in class are private"
  template <typename U>
  static int probe(void (U::*)(void));
  template <typename>
  static char probe(...);
 public:
  static const bool value = sizeof(probe<T>(0)) == sizeof(int);
};
template <typename T>
struct is_const : false_type {};
template <typename T>
struct is_const<const T> : true_type {};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4244)
#endif
#ifdef __ICCARM__
#pragma diag_suppress=Pa093
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename From, typename To>
struct is_convertible {
 protected:  // <- to avoid GCC's "all member functions in class are private"
  static int probe(To);
  static char probe(...);
  static From& from_;
 public:
  static const bool value = sizeof(probe(from_)) == sizeof(int);
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#ifdef _MSC_VER
#  pragma warning(pop)
#endif
#ifdef __ICCARM__
#pragma diag_default=Pa093
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename T, typename U>
struct is_same : false_type {};
template <typename T>
struct is_same<T, T> : true_type {};
template <typename T>
struct remove_cv {
  typedef T type;
};
template <typename T>
struct remove_cv<const T> {
  typedef T type;
};
template <typename T>
struct remove_cv<volatile T> {
  typedef T type;
};
template <typename T>
struct remove_cv<const volatile T> {
  typedef T type;
};
template <typename T>
using remove_cv_t = typename remove_cv<T>::type;
template <class T>
struct is_floating_point
    : integral_constant<bool,  //
                        is_same<float, remove_cv_t<T>>::value ||
                            is_same<double, remove_cv_t<T>>::value> {};
template <typename T>
struct is_integral : integral_constant<bool,
    is_same<remove_cv_t<T>, signed char>::value ||
    is_same<remove_cv_t<T>, unsigned char>::value ||
    is_same<remove_cv_t<T>, signed short>::value ||
    is_same<remove_cv_t<T>, unsigned short>::value ||
    is_same<remove_cv_t<T>, signed int>::value ||
    is_same<remove_cv_t<T>, unsigned int>::value ||
    is_same<remove_cv_t<T>, signed long>::value ||
    is_same<remove_cv_t<T>, unsigned long>::value ||
    is_same<remove_cv_t<T>, signed long long>::value ||
    is_same<remove_cv_t<T>, unsigned long long>::value ||
    is_same<remove_cv_t<T>, char>::value ||
    is_same<remove_cv_t<T>, bool>::value> {};
template <typename T>
struct is_enum {
  static const bool value = is_convertible<T, long long>::value &&
                            !is_class<T>::value && !is_integral<T>::value &&
                            !is_floating_point<T>::value;
};
template <typename T>
struct is_pointer : false_type {};
template <typename T>
struct is_pointer<T*> : true_type {};
template <typename T>
struct is_signed : integral_constant<bool,
    is_same<remove_cv_t<T>, char>::value ||
    is_same<remove_cv_t<T>, signed char>::value ||
    is_same<remove_cv_t<T>, signed short>::value ||
    is_same<remove_cv_t<T>, signed int>::value ||
    is_same<remove_cv_t<T>, signed long>::value ||
    is_same<remove_cv_t<T>, signed long long>::value ||
    is_same<remove_cv_t<T>, float>::value ||
    is_same<remove_cv_t<T>, double>::value> {};
template <typename T>
struct is_unsigned : integral_constant<bool,
    is_same<remove_cv_t<T>, unsigned char>::value ||
    is_same<remove_cv_t<T>, unsigned short>::value ||
    is_same<remove_cv_t<T>, unsigned int>::value ||
    is_same<remove_cv_t<T>, unsigned long>::value ||
    is_same<remove_cv_t<T>, unsigned long long>::value ||
    is_same<remove_cv_t<T>, bool>::value> {};
template <typename T>
struct type_identity {
  typedef T type;
};
template <typename T>
struct make_unsigned;
template <>
struct make_unsigned<char> : type_identity<unsigned char> {};
template <>
struct make_unsigned<signed char> : type_identity<unsigned char> {};
template <>
struct make_unsigned<unsigned char> : type_identity<unsigned char> {};
template <>
struct make_unsigned<signed short> : type_identity<unsigned short> {};
template <>
struct make_unsigned<unsigned short> : type_identity<unsigned short> {};
template <>
struct make_unsigned<signed int> : type_identity<unsigned int> {};
template <>
struct make_unsigned<unsigned int> : type_identity<unsigned int> {};
template <>
struct make_unsigned<signed long> : type_identity<unsigned long> {};
template <>
struct make_unsigned<unsigned long> : type_identity<unsigned long> {};
template <>
struct make_unsigned<signed long long> : type_identity<unsigned long long> {};
template <>
struct make_unsigned<unsigned long long> : type_identity<unsigned long long> {};
template <typename T>
using make_unsigned_t = typename make_unsigned<T>::type;
template <typename T>
struct remove_const {
  typedef T type;
};
template <typename T>
struct remove_const<const T> {
  typedef T type;
};
template <typename T>
using remove_const_t = typename remove_const<T>::type;
template <typename...>
struct make_void {
  using type = void;
};
template <typename... Args>
using void_t = typename make_void<Args...>::type;
using nullptr_t = decltype(nullptr);
template <class T>
T&& forward(remove_reference_t<T>& t) noexcept {
  return static_cast<T&&>(t);
}
template <class T>
remove_reference_t<T>&& move(T&& t) {
  return static_cast<remove_reference_t<T>&&>(t);
}
template <class T>
void swap_(T& a, T& b) {
  T tmp = move(a);
  a = move(b);
  b = move(tmp);
}
ARDUINOJSON_END_PRIVATE_NAMESPACE
#include <string.h>
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
using PoolCount = SlotId;
template <typename T>
class MemoryPoolList {
  struct FreeSlot {
    SlotId next;
  };
  static_assert(sizeof(FreeSlot) <= sizeof(T), "T is too small");
 public:
  using Pool = MemoryPool<T>;
  MemoryPoolList() = default;
  ~MemoryPoolList() {
    ARDUINOJSON_ASSERT(count_ == 0);
  }
  friend void swap(MemoryPoolList& a, MemoryPoolList& b) {
    bool aUsedPreallocated = a.pools_ == a.preallocatedPools_;
    bool bUsedPreallocated = b.pools_ == b.preallocatedPools_;
    if (aUsedPreallocated && bUsedPreallocated) {
      for (PoolCount i = 0; i < ARDUINOJSON_INITIAL_POOL_COUNT; i++)
        swap_(a.preallocatedPools_[i], b.preallocatedPools_[i]);
    } else if (bUsedPreallocated) {
      for (PoolCount i = 0; i < b.count_; i++)
        a.preallocatedPools_[i] = b.preallocatedPools_[i];
      b.pools_ = a.pools_;
      a.pools_ = a.preallocatedPools_;
    } else if (aUsedPreallocated) {
      for (PoolCount i = 0; i < a.count_; i++)
        b.preallocatedPools_[i] = a.preallocatedPools_[i];
      a.pools_ = b.pools_;
      b.pools_ = b.preallocatedPools_;
    } else {
      swap_(a.pools_, b.pools_);
    }
    swap_(a.count_, b.count_);
    swap_(a.capacity_, b.capacity_);
    swap_(a.freeList_, b.freeList_);
  }
  MemoryPoolList& operator=(MemoryPoolList&& src) {
    ARDUINOJSON_ASSERT(count_ == 0);
    if (src.pools_ == src.preallocatedPools_) {
      memcpy(preallocatedPools_, src.preallocatedPools_,
             sizeof(preallocatedPools_));
      pools_ = preallocatedPools_;
    } else {
      pools_ = src.pools_;
      src.pools_ = nullptr;
    }
    count_ = src.count_;
    capacity_ = src.capacity_;
    src.count_ = 0;
    src.capacity_ = 0;
    return *this;
  }
  Slot<T> allocSlot(Allocator* allocator) {
    if (freeList_ != NULL_SLOT) {
      return allocFromFreeList();
    }
    if (count_) {
      auto slot = allocFromLastPool();
      if (slot)
        return slot;
    }
    auto pool = addPool(allocator);
    if (!pool)
      return {};
    return allocFromLastPool();
  }
  void freeSlot(Slot<T> slot) {
    reinterpret_cast<FreeSlot*>(slot.ptr())->next = freeList_;
    freeList_ = slot.id();
  }
  T* getSlot(SlotId id) const {
    if (id == NULL_SLOT)
      return nullptr;
    auto poolIndex = SlotId(id / ARDUINOJSON_POOL_CAPACITY);
    auto indexInPool = SlotId(id % ARDUINOJSON_POOL_CAPACITY);
    ARDUINOJSON_ASSERT(poolIndex < count_);
    return pools_[poolIndex].getSlot(indexInPool);
  }
  void clear(Allocator* allocator) {
    for (PoolCount i = 0; i < count_; i++)
      pools_[i].destroy(allocator);
    count_ = 0;
    freeList_ = NULL_SLOT;
    if (pools_ != preallocatedPools_) {
      allocator->deallocate(pools_);
      pools_ = preallocatedPools_;
      capacity_ = ARDUINOJSON_INITIAL_POOL_COUNT;
    }
  }
  SlotCount usage() const {
    SlotCount total = 0;
    for (PoolCount i = 0; i < count_; i++)
      total = SlotCount(total + pools_[i].usage());
    return total;
  }
  size_t size() const {
    return Pool::slotsToBytes(usage());
  }
  void shrinkToFit(Allocator* allocator) {
    if (count_ > 0)
      pools_[count_ - 1].shrinkToFit(allocator);
    if (pools_ != preallocatedPools_ && count_ != capacity_) {
      pools_ = static_cast<Pool*>(
          allocator->reallocate(pools_, count_ * sizeof(Pool)));
      ARDUINOJSON_ASSERT(pools_ != nullptr);  // realloc to smaller can't fail
      capacity_ = count_;
    }
  }
 private:
  Slot<T> allocFromFreeList() {
    ARDUINOJSON_ASSERT(freeList_ != NULL_SLOT);
    auto id = freeList_;
    auto slot = getSlot(freeList_);
    freeList_ = reinterpret_cast<FreeSlot*>(slot)->next;
    return {slot, id};
  }
  Slot<T> allocFromLastPool() {
    ARDUINOJSON_ASSERT(count_ > 0);
    auto poolIndex = SlotId(count_ - 1);
    auto slot = pools_[poolIndex].allocSlot();
    if (!slot)
      return {};
    return {slot.ptr(),
            SlotId(poolIndex * ARDUINOJSON_POOL_CAPACITY + slot.id())};
  }
  Pool* addPool(Allocator* allocator) {
    if (count_ == capacity_ && !increaseCapacity(allocator))
      return nullptr;
    auto pool = &pools_[count_++];
    SlotCount poolCapacity = ARDUINOJSON_POOL_CAPACITY;
    if (count_ == maxPools)  // last pool is smaller because of NULL_SLOT
      poolCapacity--;
    pool->create(poolCapacity, allocator);
    return pool;
  }
  bool increaseCapacity(Allocator* allocator) {
    if (capacity_ == maxPools)
      return false;
    void* newPools;
    auto newCapacity = PoolCount(capacity_ * 2);
    if (pools_ == preallocatedPools_) {
      newPools = allocator->allocate(newCapacity * sizeof(Pool));
      if (!newPools)
        return false;
      memcpy(newPools, preallocatedPools_, sizeof(preallocatedPools_));
    } else {
      newPools = allocator->reallocate(pools_, newCapacity * sizeof(Pool));
      if (!newPools)
        return false;
    }
    pools_ = static_cast<Pool*>(newPools);
    capacity_ = newCapacity;
    return true;
  }
  Pool preallocatedPools_[ARDUINOJSON_INITIAL_POOL_COUNT];
  Pool* pools_ = preallocatedPools_;
  PoolCount count_ = 0;
  PoolCount capacity_ = ARDUINOJSON_INITIAL_POOL_COUNT;
  SlotId freeList_ = NULL_SLOT;
 public:
  static const PoolCount maxPools =
      PoolCount(NULL_SLOT / ARDUINOJSON_POOL_CAPACITY + 1);
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4310)
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename T, typename Enable = void>
struct numeric_limits;
template <typename T>
struct numeric_limits<T, enable_if_t<is_unsigned<T>::value>> {
  static constexpr T lowest() {
    return 0;
  }
  static constexpr T highest() {
    return T(-1);
  }
};
template <typename T>
struct numeric_limits<
    T, enable_if_t<is_integral<T>::value && is_signed<T>::value>> {
  static constexpr T lowest() {
    return T(T(1) << (sizeof(T) * 8 - 1));
  }
  static constexpr T highest() {
    return T(~lowest());
  }
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#ifdef _MSC_VER
#  pragma warning(pop)
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
struct StringNode {
  using references_type = uint_t<ARDUINOJSON_SLOT_ID_SIZE * 8>;
  using length_type = uint_t<ARDUINOJSON_STRING_LENGTH_SIZE * 8>;
  struct StringNode* next;
  references_type references;
  length_type length;
  char data[1];
  static constexpr size_t maxLength = numeric_limits<length_type>::highest();
  static constexpr size_t sizeForLength(size_t n) {
    return n + 1 + offsetof(StringNode, data);
  }
  static StringNode* create(size_t length, Allocator* allocator) {
    if (length > maxLength)
      return nullptr;
    auto size = sizeForLength(length);
    if (size < length)  // integer overflow
      return nullptr;   // (not testable on 64-bit)
    auto node = reinterpret_cast<StringNode*>(allocator->allocate(size));
    if (node) {
      node->length = length_type(length);
      node->references = 1;
    }
    return node;
  }
  static StringNode* resize(StringNode* node, size_t length,
                            Allocator* allocator) {
    ARDUINOJSON_ASSERT(node != nullptr);
    StringNode* newNode;
    if (length <= maxLength)
      newNode = reinterpret_cast<StringNode*>(
          allocator->reallocate(node, sizeForLength(length)));
    else
      newNode = nullptr;
    if (newNode)
      newNode->length = length_type(length);
    else
      allocator->deallocate(node);
    return newNode;
  }
  static void destroy(StringNode* node, Allocator* allocator) {
    allocator->deallocate(node);
  }
};
constexpr size_t sizeofString(size_t n) {
  return StringNode::sizeForLength(n);
}
ARDUINOJSON_END_PRIVATE_NAMESPACE
#ifdef _MSC_VER  // Visual Studio
#  define FORCE_INLINE  // __forceinline causes C4714 when returning std::string
#  ifndef ARDUINOJSON_DEPRECATED
#    define ARDUINOJSON_DEPRECATED(msg) __declspec(deprecated(msg))
#  endif
#elif defined(__GNUC__)  // GCC or Clang
#  define FORCE_INLINE __attribute__((always_inline))
#  ifndef ARDUINOJSON_DEPRECATED
#    if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#      define ARDUINOJSON_DEPRECATED(msg) __attribute__((deprecated(msg)))
#    else
#      define ARDUINOJSON_DEPRECATED(msg) __attribute__((deprecated))
#    endif
#  endif
#else  // Other compilers
#  define FORCE_INLINE
#  ifndef ARDUINOJSON_DEPRECATED
#    define ARDUINOJSON_DEPRECATED(msg)
#  endif
#endif
#if defined(__has_attribute)
#  if __has_attribute(no_sanitize)
#    define ARDUINOJSON_NO_SANITIZE(check) __attribute__((no_sanitize(check)))
#  else
#    define ARDUINOJSON_NO_SANITIZE(check)
#  endif
#else
#  define ARDUINOJSON_NO_SANITIZE(check)
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename TString, typename Enable = void>
struct StringAdapter;
template <typename TString, typename Enable = void>
struct SizedStringAdapter;
template <typename TString>
typename StringAdapter<TString>::AdaptedString adaptString(const TString& s) {
  return StringAdapter<TString>::adapt(s);
}
template <typename TChar>
typename StringAdapter<TChar*>::AdaptedString adaptString(TChar* p) {
  return StringAdapter<TChar*>::adapt(p);
}
template <typename TChar>
typename SizedStringAdapter<TChar*>::AdaptedString adaptString(TChar* p,
                                                               size_t n) {
  return SizedStringAdapter<TChar*>::adapt(p, n);
}
template <typename T>
struct IsChar
    : integral_constant<bool, is_integral<T>::value && sizeof(T) == 1> {};
class ZeroTerminatedRamString {
 public:
  static const size_t typeSortKey = 3;
  ZeroTerminatedRamString(const char* str) : str_(str) {}
  bool isNull() const {
    return !str_;
  }
  FORCE_INLINE size_t size() const {
    return str_ ? ::strlen(str_) : 0;
  }
  char operator[](size_t i) const {
    ARDUINOJSON_ASSERT(str_ != 0);
    ARDUINOJSON_ASSERT(i <= size());
    return str_[i];
  }
  const char* data() const {
    return str_;
  }
  friend int stringCompare(ZeroTerminatedRamString a,
                           ZeroTerminatedRamString b) {
    ARDUINOJSON_ASSERT(!a.isNull());
    ARDUINOJSON_ASSERT(!b.isNull());
    return ::strcmp(a.str_, b.str_);
  }
  friend bool stringEquals(ZeroTerminatedRamString a,
                           ZeroTerminatedRamString b) {
    return stringCompare(a, b) == 0;
  }
  bool isLinked() const {
    return false;
  }
 protected:
  const char* str_;
};
template <typename TChar>
struct StringAdapter<TChar*, enable_if_t<IsChar<TChar>::value>> {
  typedef ZeroTerminatedRamString AdaptedString;
  static AdaptedString adapt(const TChar* p) {
    return AdaptedString(reinterpret_cast<const char*>(p));
  }
};
template <typename TChar, size_t N>
struct StringAdapter<TChar[N], enable_if_t<IsChar<TChar>::value>> {
  typedef ZeroTerminatedRamString AdaptedString;
  static AdaptedString adapt(const TChar* p) {
    return AdaptedString(reinterpret_cast<const char*>(p));
  }
};
class StaticStringAdapter : public ZeroTerminatedRamString {
 public:
  StaticStringAdapter(const char* str) : ZeroTerminatedRamString(str) {}
  bool isLinked() const {
    return true;
  }
};
template <>
struct StringAdapter<const char*, void> {
  typedef StaticStringAdapter AdaptedString;
  static AdaptedString adapt(const char* p) {
    return AdaptedString(p);
  }
};
class SizedRamString {
 public:
  static const size_t typeSortKey = 2;
  SizedRamString(const char* str, size_t sz) : str_(str), size_(sz) {}
  bool isNull() const {
    return !str_;
  }
  size_t size() const {
    return size_;
  }
  char operator[](size_t i) const {
    ARDUINOJSON_ASSERT(str_ != 0);
    ARDUINOJSON_ASSERT(i <= size());
    return str_[i];
  }
  const char* data() const {
    return str_;
  }
  bool isLinked() const {
    return false;
  }
 protected:
  const char* str_;
  size_t size_;
};
template <typename TChar>
struct SizedStringAdapter<TChar*, enable_if_t<IsChar<TChar>::value>> {
  typedef SizedRamString AdaptedString;
  static AdaptedString adapt(const TChar* p, size_t n) {
    return AdaptedString(reinterpret_cast<const char*>(p), n);
  }
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#if ARDUINOJSON_ENABLE_STD_STREAM
#include <ostream>
#endif
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
class JsonString {
 public:
  enum Ownership { Copied, Linked };
  JsonString() : data_(0), size_(0), ownership_(Linked) {}
  JsonString(const char* data, Ownership ownership = Linked)
      : data_(data), size_(data ? ::strlen(data) : 0), ownership_(ownership) {}
  JsonString(const char* data, size_t sz, Ownership ownership = Linked)
      : data_(data), size_(sz), ownership_(ownership) {}
  const char* c_str() const {
    return data_;
  }
  bool isNull() const {
    return !data_;
  }
  bool isLinked() const {
    return ownership_ == Linked;
  }
  size_t size() const {
    return size_;
  }
  explicit operator bool() const {
    return data_ != 0;
  }
  friend bool operator==(JsonString lhs, JsonString rhs) {
    if (lhs.size_ != rhs.size_)
      return false;
    if (lhs.data_ == rhs.data_)
      return true;
    if (!lhs.data_)
      return false;
    if (!rhs.data_)
      return false;
    return memcmp(lhs.data_, rhs.data_, lhs.size_) == 0;
  }
  friend bool operator!=(JsonString lhs, JsonString rhs) {
    return !(lhs == rhs);
  }
#if ARDUINOJSON_ENABLE_STD_STREAM
  friend std::ostream& operator<<(std::ostream& lhs, const JsonString& rhs) {
    lhs.write(rhs.c_str(), static_cast<std::streamsize>(rhs.size()));
    return lhs;
  }
#endif
 private:
  const char* data_;
  size_t size_;
  Ownership ownership_;
};
ARDUINOJSON_END_PUBLIC_NAMESPACE
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
class JsonStringAdapter : public SizedRamString {
 public:
  JsonStringAdapter(const JsonString& s)
      : SizedRamString(s.c_str(), s.size()), linked_(s.isLinked()) {}
  bool isLinked() const {
    return linked_;
  }
 private:
  bool linked_;
};
template <>
struct StringAdapter<JsonString> {
  typedef JsonStringAdapter AdaptedString;
  static AdaptedString adapt(const JsonString& s) {
    return AdaptedString(s);
  }
};
namespace string_traits_impl {
template <class T, class = void>
struct has_cstr : false_type {};
template <class T>
struct has_cstr<T, enable_if_t<is_same<decltype(declval<const T>().c_str()),
                                       const char*>::value>> : true_type {};
template <class T, class = void>
struct has_data : false_type {};
template <class T>
struct has_data<T, enable_if_t<is_same<decltype(declval<const T>().data()),
                                       const char*>::value>> : true_type {};
template <class T, class = void>
struct has_length : false_type {};
template <class T>
struct has_length<
    T, enable_if_t<is_unsigned<decltype(declval<const T>().length())>::value>>
    : true_type {};
template <class T, class = void>
struct has_size : false_type {};
template <class T>
struct has_size<
    T, enable_if_t<is_same<decltype(declval<const T>().size()), size_t>::value>>
    : true_type {};
}  // namespace string_traits_impl
template <typename T>
struct string_traits {
  enum {
    has_cstr = string_traits_impl::has_cstr<T>::value,
    has_length = string_traits_impl::has_length<T>::value,
    has_data = string_traits_impl::has_data<T>::value,
    has_size = string_traits_impl::has_size<T>::value
  };
};
template <typename T>
struct StringAdapter<
    T,
    enable_if_t<(string_traits<T>::has_cstr || string_traits<T>::has_data) &&
                (string_traits<T>::has_length || string_traits<T>::has_size)>> {
  typedef SizedRamString AdaptedString;
  static AdaptedString adapt(const T& s) {
    return AdaptedString(get_data(s), get_size(s));
  }
 private:
  template <typename U>
  static enable_if_t<string_traits<U>::has_size, size_t> get_size(const U& s) {
    return s.size();
  }
  template <typename U>
  static enable_if_t<!string_traits<U>::has_size, size_t> get_size(const U& s) {
    return s.length();
  }
  template <typename U>
  static enable_if_t<string_traits<U>::has_data, const char*> get_data(
      const U& s) {
    return s.data();
  }
  template <typename U>
  static enable_if_t<!string_traits<U>::has_data, const char*> get_data(
      const U& s) {
    return s.c_str();
  }
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#if ARDUINOJSON_ENABLE_PROGMEM
#ifdef ARDUINO
#else
class __FlashStringHelper;
#include <avr/pgmspace.h>
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
struct pgm_p {
  pgm_p(const void* p) : address(reinterpret_cast<const char*>(p)) {}
  const char* address;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#ifndef strlen_P
inline size_t strlen_P(FLArduinoJson::detail::pgm_p s) {
  const char* p = s.address;
  ARDUINOJSON_ASSERT(p != NULL);
  while (pgm_read_byte(p))
    p++;
  return size_t(p - s.address);
}
#endif
#ifndef strncmp_P
inline int strncmp_P(const char* a, FLArduinoJson::detail::pgm_p b, size_t n) {
  const char* s1 = a;
  const char* s2 = b.address;
  ARDUINOJSON_ASSERT(s1 != NULL);
  ARDUINOJSON_ASSERT(s2 != NULL);
  while (n-- > 0) {
    char c1 = *s1++;
    char c2 = static_cast<char>(pgm_read_byte(s2++));
    if (c1 < c2)
      return -1;
    if (c1 > c2)
      return 1;
    if (c1 == 0 /* and c2 as well */)
      return 0;
  }
  return 0;
}
#endif
#ifndef strcmp_P
inline int strcmp_P(const char* a, FLArduinoJson::detail::pgm_p b) {
  const char* s1 = a;
  const char* s2 = b.address;
  ARDUINOJSON_ASSERT(s1 != NULL);
  ARDUINOJSON_ASSERT(s2 != NULL);
  for (;;) {
    char c1 = *s1++;
    char c2 = static_cast<char>(pgm_read_byte(s2++));
    if (c1 < c2)
      return -1;
    if (c1 > c2)
      return 1;
    if (c1 == 0 /* and c2 as well */)
      return 0;
  }
}
#endif
#ifndef memcmp_P
inline int memcmp_P(const void* a, FLArduinoJson::detail::pgm_p b, size_t n) {
  const uint8_t* p1 = reinterpret_cast<const uint8_t*>(a);
  const char* p2 = b.address;
  ARDUINOJSON_ASSERT(p1 != NULL);
  ARDUINOJSON_ASSERT(p2 != NULL);
  while (n-- > 0) {
    uint8_t v1 = *p1++;
    uint8_t v2 = pgm_read_byte(p2++);
    if (v1 != v2)
      return v1 - v2;
  }
  return 0;
}
#endif
#ifndef memcpy_P
inline void* memcpy_P(void* dst, FLArduinoJson::detail::pgm_p src, size_t n) {
  uint8_t* d = reinterpret_cast<uint8_t*>(dst);
  const char* s = src.address;
  ARDUINOJSON_ASSERT(d != NULL);
  ARDUINOJSON_ASSERT(s != NULL);
  while (n-- > 0) {
    *d++ = pgm_read_byte(s++);
  }
  return dst;
}
#endif
#ifndef pgm_read_dword
inline uint32_t pgm_read_dword(FLArduinoJson::detail::pgm_p p) {
  uint32_t result;
  memcpy_P(&result, p.address, 4);
  return result;
}
#endif
#ifndef pgm_read_float
inline float pgm_read_float(FLArduinoJson::detail::pgm_p p) {
  float result;
  memcpy_P(&result, p.address, sizeof(float));
  return result;
}
#endif
#ifndef pgm_read_double
#  if defined(__SIZEOF_DOUBLE__) && defined(__SIZEOF_FLOAT__) && \
      __SIZEOF_DOUBLE__ == __SIZEOF_FLOAT__
inline double pgm_read_double(FLArduinoJson::detail::pgm_p p) {
  return pgm_read_float(p.address);
}
#  else
inline double pgm_read_double(FLArduinoJson::detail::pgm_p p) {
  double result;
  memcpy_P(&result, p.address, sizeof(double));
  return result;
}
#  endif
#endif
#ifndef pgm_read_ptr
inline void* pgm_read_ptr(FLArduinoJson::detail::pgm_p p) {
  void* result;
  memcpy_P(&result, p.address, sizeof(result));
  return result;
}
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
class FlashString {
 public:
  static const size_t typeSortKey = 1;
  FlashString(const __FlashStringHelper* str, size_t sz)
      : str_(reinterpret_cast<const char*>(str)), size_(sz) {}
  bool isNull() const {
    return !str_;
  }
  char operator[](size_t i) const {
    ARDUINOJSON_ASSERT(str_ != 0);
    ARDUINOJSON_ASSERT(i <= size_);
    return static_cast<char>(pgm_read_byte(str_ + i));
  }
  const char* data() const {
    return nullptr;
  }
  size_t size() const {
    return size_;
  }
  friend bool stringEquals(FlashString a, SizedRamString b) {
    ARDUINOJSON_ASSERT(a.typeSortKey < b.typeSortKey);
    ARDUINOJSON_ASSERT(!a.isNull());
    ARDUINOJSON_ASSERT(!b.isNull());
    if (a.size() != b.size())
      return false;
    return ::memcmp_P(b.data(), a.str_, a.size_) == 0;
  }
  friend int stringCompare(FlashString a, SizedRamString b) {
    ARDUINOJSON_ASSERT(a.typeSortKey < b.typeSortKey);
    ARDUINOJSON_ASSERT(!a.isNull());
    ARDUINOJSON_ASSERT(!b.isNull());
    size_t minsize = a.size() < b.size() ? a.size() : b.size();
    int res = ::memcmp_P(b.data(), a.str_, minsize);
    if (res)
      return -res;
    if (a.size() < b.size())
      return -1;
    if (a.size() > b.size())
      return 1;
    return 0;
  }
  friend void stringGetChars(FlashString s, char* p, size_t n) {
    ARDUINOJSON_ASSERT(s.size() <= n);
    ::memcpy_P(p, s.str_, n);
  }
  bool isLinked() const {
    return false;
  }
 private:
  const char* str_;
  size_t size_;
};
template <>
struct StringAdapter<const __FlashStringHelper*, void> {
  typedef FlashString AdaptedString;
  static AdaptedString adapt(const __FlashStringHelper* s) {
    return AdaptedString(s, s ? strlen_P(reinterpret_cast<const char*>(s)) : 0);
  }
};
template <>
struct SizedStringAdapter<const __FlashStringHelper*, void> {
  typedef FlashString AdaptedString;
  static AdaptedString adapt(const __FlashStringHelper* s, size_t n) {
    return AdaptedString(s, n);
  }
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename TAdaptedString1, typename TAdaptedString2>
enable_if_t<TAdaptedString1::typeSortKey <= TAdaptedString2::typeSortKey, int>
stringCompare(TAdaptedString1 s1, TAdaptedString2 s2) {
  ARDUINOJSON_ASSERT(!s1.isNull());
  ARDUINOJSON_ASSERT(!s2.isNull());
  size_t size1 = s1.size();
  size_t size2 = s2.size();
  size_t n = size1 < size2 ? size1 : size2;
  for (size_t i = 0; i < n; i++) {
    if (s1[i] != s2[i])
      return s1[i] - s2[i];
  }
  if (size1 < size2)
    return -1;
  if (size1 > size2)
    return 1;
  return 0;
}
template <typename TAdaptedString1, typename TAdaptedString2>
enable_if_t<(TAdaptedString1::typeSortKey > TAdaptedString2::typeSortKey), int>
stringCompare(TAdaptedString1 s1, TAdaptedString2 s2) {
  return -stringCompare(s2, s1);
}
template <typename TAdaptedString1, typename TAdaptedString2>
enable_if_t<TAdaptedString1::typeSortKey <= TAdaptedString2::typeSortKey, bool>
stringEquals(TAdaptedString1 s1, TAdaptedString2 s2) {
  ARDUINOJSON_ASSERT(!s1.isNull());
  ARDUINOJSON_ASSERT(!s2.isNull());
  size_t size1 = s1.size();
  size_t size2 = s2.size();
  if (size1 != size2)
    return false;
  for (size_t i = 0; i < size1; i++) {
    if (s1[i] != s2[i])
      return false;
  }
  return true;
}
template <typename TAdaptedString1, typename TAdaptedString2>
enable_if_t<(TAdaptedString1::typeSortKey > TAdaptedString2::typeSortKey), bool>
stringEquals(TAdaptedString1 s1, TAdaptedString2 s2) {
  return stringEquals(s2, s1);
}
template <typename TAdaptedString>
static void stringGetChars(TAdaptedString s, char* p, size_t n) {
  ARDUINOJSON_ASSERT(s.size() <= n);
  for (size_t i = 0; i < n; i++) {
    p[i] = s[i];
  }
}
class StringPool {
 public:
  StringPool() = default;
  StringPool(const StringPool&) = delete;
  void operator=(StringPool&& src) = delete;
  ~StringPool() {
    ARDUINOJSON_ASSERT(strings_ == nullptr);
  }
  friend void swap(StringPool& a, StringPool& b) {
    swap_(a.strings_, b.strings_);
  }
  void clear(Allocator* allocator) {
    while (strings_) {
      auto node = strings_;
      strings_ = node->next;
      StringNode::destroy(node, allocator);
    }
  }
  size_t size() const {
    size_t total = 0;
    for (auto node = strings_; node; node = node->next)
      total += sizeofString(node->length);
    return total;
  }
  template <typename TAdaptedString>
  StringNode* add(TAdaptedString str, Allocator* allocator) {
    ARDUINOJSON_ASSERT(str.isNull() == false);
    auto node = get(str);
    if (node) {
      node->references++;
      return node;
    }
    size_t n = str.size();
    node = StringNode::create(n, allocator);
    if (!node)
      return nullptr;
    stringGetChars(str, node->data, n);
    node->data[n] = 0;  // force NUL terminator
    add(node);
    return node;
  }
  void add(StringNode* node) {
    ARDUINOJSON_ASSERT(node != nullptr);
    node->next = strings_;
    strings_ = node;
  }
  template <typename TAdaptedString>
  StringNode* get(const TAdaptedString& str) const {
    for (auto node = strings_; node; node = node->next) {
      if (stringEquals(str, adaptString(node->data, node->length)))
        return node;
    }
    return nullptr;
  }
  void dereference(const char* s, Allocator* allocator) {
    StringNode* prev = nullptr;
    for (auto node = strings_; node; node = node->next) {
      if (node->data == s) {
        if (--node->references == 0) {
          if (prev)
            prev->next = node->next;
          else
            strings_ = node->next;
          StringNode::destroy(node, allocator);
        }
        return;
      }
      prev = node;
    }
  }
 private:
  StringNode* strings_ = nullptr;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
template <typename T>
class SerializedValue {
 public:
  explicit SerializedValue(T str) : str_(str) {}
  operator T() const {
    return str_;
  }
  const char* data() const {
    return str_.c_str();
  }
  size_t size() const {
    return str_.length();
  }
 private:
  T str_;
};
template <typename TChar>
class SerializedValue<TChar*> {
 public:
  explicit SerializedValue(TChar* p, size_t n) : data_(p), size_(n) {}
  operator TChar*() const {
    return data_;
  }
  TChar* data() const {
    return data_;
  }
  size_t size() const {
    return size_;
  }
 private:
  TChar* data_;
  size_t size_;
};
using RawString = SerializedValue<const char*>;
template <typename T>
inline SerializedValue<T> serialized(T str) {
  return SerializedValue<T>(str);
}
template <typename TChar>
inline SerializedValue<TChar*> serialized(TChar* p) {
  return SerializedValue<TChar*>(p, detail::adaptString(p).size());
}
template <typename TChar>
inline SerializedValue<TChar*> serialized(TChar* p, size_t n) {
  return SerializedValue<TChar*>(p, n);
}
ARDUINOJSON_END_PUBLIC_NAMESPACE
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wconversion"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
#ifndef isnan
template <typename T>
bool isnan(T x) {
  return x != x;
}
#endif
#ifndef isinf
template <typename T>
bool isinf(T x) {
  return x != 0.0 && x * 2 == x;
}
#endif
template <typename T, typename F>
struct alias_cast_t {
  union {
    F raw;
    T data;
  };
};
template <typename T, typename F>
T alias_cast(F raw_data) {
  alias_cast_t<T, F> ac;
  ac.raw = raw_data;
  return ac.data;
}
ARDUINOJSON_END_PRIVATE_NAMESPACE
#if ARDUINOJSON_ENABLE_PROGMEM
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
#if ARDUINOJSON_ENABLE_PROGMEM
#  ifndef ARDUINOJSON_DEFINE_PROGMEM_ARRAY
#    define ARDUINOJSON_DEFINE_PROGMEM_ARRAY(type, name, ...) \
      static type const name[] PROGMEM = __VA_ARGS__;
#  endif
template <typename T>
inline const T* pgm_read(const T* const* p) {
  return reinterpret_cast<const T*>(pgm_read_ptr(p));
}
inline uint32_t pgm_read(const uint32_t* p) {
  return pgm_read_dword(p);
}
inline double pgm_read(const double* p) {
  return pgm_read_double(p);
}
inline float pgm_read(const float* p) {
  return pgm_read_float(p);
}
#else
#  ifndef ARDUINOJSON_DEFINE_PROGMEM_ARRAY
#    define ARDUINOJSON_DEFINE_PROGMEM_ARRAY(type, name, ...) \
      static type const name[] = __VA_ARGS__;
#  endif
template <typename T>
inline T pgm_read(const T* p) {
  return *p;
}
#endif
template <typename T>
class pgm_ptr {
 public:
  explicit pgm_ptr(const T* ptr) : ptr_(ptr) {}
  T operator[](intptr_t index) const {
    return pgm_read(ptr_ + index);
  }
 private:
  const T* ptr_;
};
template <typename T, size_t = sizeof(T)>
struct FloatTraits {};
template <typename T>
struct FloatTraits<T, 8 /*64bits*/> {
  typedef uint64_t mantissa_type;
  static const short mantissa_bits = 52;
  static const mantissa_type mantissa_max =
      (mantissa_type(1) << mantissa_bits) - 1;
  typedef int16_t exponent_type;
  static const exponent_type exponent_max = 308;
  static pgm_ptr<T> positiveBinaryPowersOfTen() {
    ARDUINOJSON_DEFINE_PROGMEM_ARRAY(  //
        uint64_t, factors,
        {
            0x4024000000000000,  // 1e1
            0x4059000000000000,  // 1e2
            0x40C3880000000000,  // 1e4
            0x4197D78400000000,  // 1e8
            0x4341C37937E08000,  // 1e16
            0x4693B8B5B5056E17,  // 1e32
            0x4D384F03E93FF9F5,  // 1e64
            0x5A827748F9301D32,  // 1e128
            0x75154FDD7F73BF3C,  // 1e256
        });
    return pgm_ptr<T>(reinterpret_cast<const T*>(factors));
  }
  static pgm_ptr<T> negativeBinaryPowersOfTen() {
    ARDUINOJSON_DEFINE_PROGMEM_ARRAY(  //
        uint64_t, factors,
        {
            0x3FB999999999999A,  // 1e-1
            0x3F847AE147AE147B,  // 1e-2
            0x3F1A36E2EB1C432D,  // 1e-4
            0x3E45798EE2308C3A,  // 1e-8
            0x3C9CD2B297D889BC,  // 1e-16
            0x3949F623D5A8A733,  // 1e-32
            0x32A50FFD44F4A73D,  // 1e-64
            0x255BBA08CF8C979D,  // 1e-128
            0x0AC8062864AC6F43   // 1e-256
        });
    return pgm_ptr<T>(reinterpret_cast<const T*>(factors));
  }
  static T nan() {
    return forge(0x7ff8000000000000);
  }
  static T inf() {
    return forge(0x7ff0000000000000);
  }
  static T highest() {
    return forge(0x7FEFFFFFFFFFFFFF);
  }
  template <typename TOut>  // int64_t
  static T highest_for(
      enable_if_t<is_integral<TOut>::value && is_signed<TOut>::value &&
                      sizeof(TOut) == 8,
                  signed>* = 0) {
    return forge(0x43DFFFFFFFFFFFFF);  //  9.2233720368547748e+18
  }
  template <typename TOut>  // uint64_t
  static T highest_for(
      enable_if_t<is_integral<TOut>::value && is_unsigned<TOut>::value &&
                      sizeof(TOut) == 8,
                  unsigned>* = 0) {
    return forge(0x43EFFFFFFFFFFFFF);  //  1.8446744073709549568e+19
  }
  static T lowest() {
    return forge(0xFFEFFFFFFFFFFFFF);
  }
  static T forge(uint64_t bits) {
    return alias_cast<T>(bits);
  }
};
template <typename T>
struct FloatTraits<T, 4 /*32bits*/> {
  typedef uint32_t mantissa_type;
  static const short mantissa_bits = 23;
  static const mantissa_type mantissa_max =
      (mantissa_type(1) << mantissa_bits) - 1;
  typedef int8_t exponent_type;
  static const exponent_type exponent_max = 38;
  static pgm_ptr<T> positiveBinaryPowersOfTen() {
    ARDUINOJSON_DEFINE_PROGMEM_ARRAY(uint32_t, factors,
                                     {
                                         0x41200000,  // 1e1f
                                         0x42c80000,  // 1e2f
                                         0x461c4000,  // 1e4f
                                         0x4cbebc20,  // 1e8f
                                         0x5a0e1bca,  // 1e16f
                                         0x749dc5ae   // 1e32f
                                     });
    return pgm_ptr<T>(reinterpret_cast<const T*>(factors));
  }
  static pgm_ptr<T> negativeBinaryPowersOfTen() {
    ARDUINOJSON_DEFINE_PROGMEM_ARRAY(uint32_t, factors,
                                     {
                                         0x3dcccccd,  // 1e-1f
                                         0x3c23d70a,  // 1e-2f
                                         0x38d1b717,  // 1e-4f
                                         0x322bcc77,  // 1e-8f
                                         0x24e69595,  // 1e-16f
                                         0x0a4fb11f   // 1e-32f
                                     });
    return pgm_ptr<T>(reinterpret_cast<const T*>(factors));
  }
  static T forge(uint32_t bits) {
    return alias_cast<T>(bits);
  }
  static T nan() {
    return forge(0x7fc00000);
  }
  static T inf() {
    return forge(0x7f800000);
  }
  static T highest() {
    return forge(0x7f7fffff);
  }
  template <typename TOut>  // int32_t
  static T highest_for(
      enable_if_t<is_integral<TOut>::value && is_signed<TOut>::value &&
                      sizeof(TOut) == 4,
                  signed>* = 0) {
    return forge(0x4EFFFFFF);  // 2.14748352E9
  }
  template <typename TOut>  // uint32_t
  static T highest_for(
      enable_if_t<is_integral<TOut>::value && is_unsigned<TOut>::value &&
                      sizeof(TOut) == 4,
                  unsigned>* = 0) {
    return forge(0x4F7FFFFF);  // 4.29496704E9
  }
  template <typename TOut>  // int64_t
  static T highest_for(
      enable_if_t<is_integral<TOut>::value && is_signed<TOut>::value &&
                      sizeof(TOut) == 8,
                  signed>* = 0) {
    return forge(0x5EFFFFFF);  // 9.22337148709896192E18
  }
  template <typename TOut>  // uint64_t
  static T highest_for(
      enable_if_t<is_integral<TOut>::value && is_unsigned<TOut>::value &&
                      sizeof(TOut) == 8,
                  unsigned>* = 0) {
    return forge(0x5F7FFFFF);  // 1.844674297419792384E19
  }
  static T lowest() {
    return forge(0xFf7fffff);
  }
};
template <typename TFloat, typename TExponent>
inline TFloat make_float(TFloat m, TExponent e) {
  using traits = FloatTraits<TFloat>;
  auto powersOfTen = e > 0 ? traits::positiveBinaryPowersOfTen()
                           : traits::negativeBinaryPowersOfTen();
  if (e <= 0)
    e = TExponent(-e);
  for (uint8_t index = 0; e != 0; index++) {
    if (e & 1)
      m *= powersOfTen[index];
    e >>= 1;
  }
  return m;
}
ARDUINOJSON_END_PRIVATE_NAMESPACE
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
#if ARDUINOJSON_USE_DOUBLE
typedef double JsonFloat;
#else
typedef float JsonFloat;
#endif
ARDUINOJSON_END_PUBLIC_NAMESPACE
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename TOut, typename TIn>
enable_if_t<is_integral<TIn>::value && is_unsigned<TIn>::value &&
                is_integral<TOut>::value && sizeof(TOut) <= sizeof(TIn),
            bool>
canConvertNumber(TIn value) {
  return value <= TIn(numeric_limits<TOut>::highest());
}
template <typename TOut, typename TIn>
enable_if_t<is_integral<TIn>::value && is_unsigned<TIn>::value &&
                is_integral<TOut>::value && sizeof(TIn) < sizeof(TOut),
            bool>
canConvertNumber(TIn) {
  return true;
}
template <typename TOut, typename TIn>
enable_if_t<is_integral<TIn>::value && is_floating_point<TOut>::value, bool>
canConvertNumber(TIn) {
  return true;
}
template <typename TOut, typename TIn>
enable_if_t<is_integral<TIn>::value && is_signed<TIn>::value &&
                is_integral<TOut>::value && is_signed<TOut>::value &&
                sizeof(TOut) < sizeof(TIn),
            bool>
canConvertNumber(TIn value) {
  return value >= TIn(numeric_limits<TOut>::lowest()) &&
         value <= TIn(numeric_limits<TOut>::highest());
}
template <typename TOut, typename TIn>
enable_if_t<is_integral<TIn>::value && is_signed<TIn>::value &&
                is_integral<TOut>::value && is_signed<TOut>::value &&
                sizeof(TIn) <= sizeof(TOut),
            bool>
canConvertNumber(TIn) {
  return true;
}
template <typename TOut, typename TIn>
enable_if_t<is_integral<TIn>::value && is_signed<TIn>::value &&
                is_integral<TOut>::value && is_unsigned<TOut>::value &&
                sizeof(TOut) >= sizeof(TIn),
            bool>
canConvertNumber(TIn value) {
  if (value < 0)
    return false;
  return TOut(value) <= numeric_limits<TOut>::highest();
}
template <typename TOut, typename TIn>
enable_if_t<is_integral<TIn>::value && is_signed<TIn>::value &&
                is_integral<TOut>::value && is_unsigned<TOut>::value &&
                sizeof(TOut) < sizeof(TIn),
            bool>
canConvertNumber(TIn value) {
  if (value < 0)
    return false;
  return value <= TIn(numeric_limits<TOut>::highest());
}
template <typename TOut, typename TIn>
enable_if_t<is_floating_point<TIn>::value && is_integral<TOut>::value &&
                sizeof(TOut) < sizeof(TIn),
            bool>
canConvertNumber(TIn value) {
  return value >= numeric_limits<TOut>::lowest() &&
         value <= numeric_limits<TOut>::highest();
}
template <typename TOut, typename TIn>
enable_if_t<is_floating_point<TIn>::value && is_integral<TOut>::value &&
                sizeof(TOut) >= sizeof(TIn),
            bool>
canConvertNumber(TIn value) {
  return value >= numeric_limits<TOut>::lowest() &&
         value <= FloatTraits<TIn>::template highest_for<TOut>();
}
template <typename TOut, typename TIn>
enable_if_t<is_floating_point<TIn>::value && is_floating_point<TOut>::value,
            bool>
canConvertNumber(TIn) {
  return true;
}
template <typename TOut, typename TIn>
TOut convertNumber(TIn value) {
  return canConvertNumber<TOut>(value) ? TOut(value) : 0;
}
ARDUINOJSON_END_PRIVATE_NAMESPACE
#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
class VariantData;
class ResourceManager;
class CollectionIterator {
  friend class CollectionData;
 public:
  CollectionIterator() : slot_(nullptr), currentId_(NULL_SLOT) {}
  void next(const ResourceManager* resources);
  bool done() const {
    return slot_ == nullptr;
  }
  bool operator==(const CollectionIterator& other) const {
    return slot_ == other.slot_;
  }
  bool operator!=(const CollectionIterator& other) const {
    return slot_ != other.slot_;
  }
  VariantData* operator->() {
    ARDUINOJSON_ASSERT(slot_ != nullptr);
    return data();
  }
  VariantData& operator*() {
    ARDUINOJSON_ASSERT(slot_ != nullptr);
    return *data();
  }
  const VariantData& operator*() const {
    ARDUINOJSON_ASSERT(slot_ != nullptr);
    return *data();
  }
  VariantData* data() {
    return reinterpret_cast<VariantData*>(slot_);
  }
  const VariantData* data() const {
    return reinterpret_cast<const VariantData*>(slot_);
  }
 private:
  CollectionIterator(VariantData* slot, SlotId slotId);
  VariantData* slot_;
  SlotId currentId_, nextId_;
};
class CollectionData {
  SlotId head_ = NULL_SLOT;
  SlotId tail_ = NULL_SLOT;
 public:
  static void* operator new(size_t, void* p) noexcept {
    return p;
  }
  static void operator delete(void*, void*) noexcept {}
  using iterator = CollectionIterator;
  iterator createIterator(const ResourceManager* resources) const;
  size_t size(const ResourceManager*) const;
  size_t nesting(const ResourceManager*) const;
  void clear(ResourceManager* resources);
  static void clear(CollectionData* collection, ResourceManager* resources) {
    if (!collection)
      return;
    collection->clear(resources);
  }
  SlotId head() const {
    return head_;
  }
 protected:
  void appendOne(Slot<VariantData> slot, const ResourceManager* resources);
  void appendPair(Slot<VariantData> key, Slot<VariantData> value,
                  const ResourceManager* resources);
  void removeOne(iterator it, ResourceManager* resources);
  void removePair(iterator it, ResourceManager* resources);
 private:
  Slot<VariantData> getPreviousSlot(VariantData*, const ResourceManager*) const;
};
inline const VariantData* collectionToVariant(
    const CollectionData* collection) {
  const void* data = collection;  // prevent warning cast-align
  return reinterpret_cast<const VariantData*>(data);
}
inline VariantData* collectionToVariant(CollectionData* collection) {
  void* data = collection;  // prevent warning cast-align
  return reinterpret_cast<VariantData*>(data);
}
class ArrayData : public CollectionData {
 public:
  VariantData* addElement(ResourceManager* resources);
  static VariantData* addElement(ArrayData* array, ResourceManager* resources) {
    if (!array)
      return nullptr;
    return array->addElement(resources);
  }
  template <typename T>
  bool addValue(T&& value, ResourceManager* resources);
  template <typename T>
  static bool addValue(ArrayData* array, T&& value,
                       ResourceManager* resources) {
    if (!array)
      return false;
    return array->addValue(value, resources);
  }
  VariantData* getOrAddElement(size_t index, ResourceManager* resources);
  VariantData* getElement(size_t index, const ResourceManager* resources) const;
  static VariantData* getElement(const ArrayData* array, size_t index,
                                 const ResourceManager* resources) {
    if (!array)
      return nullptr;
    return array->getElement(index, resources);
  }
  void removeElement(size_t index, ResourceManager* resources);
  static void removeElement(ArrayData* array, size_t index,
                            ResourceManager* resources) {
    if (!array)
      return;
    array->removeElement(index, resources);
  }
  void remove(iterator it, ResourceManager* resources) {
    CollectionData::removeOne(it, resources);
  }
  static void remove(ArrayData* array, iterator it,
                     ResourceManager* resources) {
    if (array)
      return array->remove(it, resources);
  }
 private:
  iterator at(size_t index, const ResourceManager* resources) const;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
#if ARDUINOJSON_USE_LONG_LONG
typedef int64_t JsonInteger;
typedef uint64_t JsonUInt;
#else
typedef long JsonInteger;
typedef unsigned long JsonUInt;
#endif
ARDUINOJSON_END_PUBLIC_NAMESPACE
#define ARDUINOJSON_ASSERT_INTEGER_TYPE_IS_SUPPORTED(T)                  \
  static_assert(sizeof(T) <= sizeof(FLArduinoJson::JsonInteger),           \
                "To use 64-bit integers with ArduinoJson, you must set " \
                "ARDUINOJSON_USE_LONG_LONG to 1. See "                   \
                "https://arduinojson.org/v7/api/config/use_long_long/");
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
class ObjectData : public CollectionData {
 public:
  template <typename TAdaptedString>  // also works with StringNode*
  VariantData* addMember(TAdaptedString key, ResourceManager* resources);
  template <typename TAdaptedString>
  VariantData* getOrAddMember(TAdaptedString key, ResourceManager* resources);
  template <typename TAdaptedString>
  VariantData* getMember(TAdaptedString key,
                         const ResourceManager* resources) const;
  template <typename TAdaptedString>
  static VariantData* getMember(const ObjectData* object, TAdaptedString key,
                                const ResourceManager* resources) {
    if (!object)
      return nullptr;
    return object->getMember(key, resources);
  }
  template <typename TAdaptedString>
  void removeMember(TAdaptedString key, ResourceManager* resources);
  template <typename TAdaptedString>
  static void removeMember(ObjectData* obj, TAdaptedString key,
                           ResourceManager* resources) {
    if (!obj)
      return;
    obj->removeMember(key, resources);
  }
  void remove(iterator it, ResourceManager* resources) {
    CollectionData::removePair(it, resources);
  }
  static void remove(ObjectData* obj, ObjectData::iterator it,
                     ResourceManager* resources) {
    if (!obj)
      return;
    obj->remove(it, resources);
  }
  size_t size(const ResourceManager* resources) const {
    return CollectionData::size(resources) / 2;
  }
  static size_t size(const ObjectData* obj, const ResourceManager* resources) {
    if (!obj)
      return 0;
    return obj->size(resources);
  }
 private:
  template <typename TAdaptedString>
  iterator findKey(TAdaptedString key, const ResourceManager* resources) const;
};
enum class VariantTypeBits : uint8_t {
  OwnedStringBit = 0x01,  // 0000 0001
  NumberBit = 0x08,       // 0000 1000
#if ARDUINOJSON_USE_EXTENSIONS
  ExtensionBit = 0x10,  // 0001 0000
#endif
  CollectionMask = 0x60,
};
enum class VariantType : uint8_t {
  Null = 0,             // 0000 0000
  RawString = 0x03,     // 0000 0011
  LinkedString = 0x04,  // 0000 0100
  OwnedString = 0x05,   // 0000 0101
  Boolean = 0x06,       // 0000 0110
  Uint32 = 0x0A,        // 0000 1010
  Int32 = 0x0C,         // 0000 1100
  Float = 0x0E,         // 0000 1110
#if ARDUINOJSON_USE_LONG_LONG
  Uint64 = 0x1A,  // 0001 1010
  Int64 = 0x1C,   // 0001 1100
#endif
#if ARDUINOJSON_USE_DOUBLE
  Double = 0x1E,  // 0001 1110
#endif
  Object = 0x20,
  Array = 0x40,
};
inline bool operator&(VariantType type, VariantTypeBits bit) {
  return (uint8_t(type) & uint8_t(bit)) != 0;
}
union VariantContent {
  VariantContent() {}
  float asFloat;
  bool asBoolean;
  uint32_t asUint32;
  int32_t asInt32;
#if ARDUINOJSON_USE_EXTENSIONS
  SlotId asSlotId;
#endif
  ArrayData asArray;
  ObjectData asObject;
  CollectionData asCollection;
  const char* asLinkedString;
  struct StringNode* asOwnedString;
};
#if ARDUINOJSON_USE_EXTENSIONS
union VariantExtension {
#  if ARDUINOJSON_USE_LONG_LONG
  uint64_t asUint64;
  int64_t asInt64;
#  endif
#  if ARDUINOJSON_USE_DOUBLE
  double asDouble;
#  endif
};
#endif
template <typename T>
T parseNumber(const char* s);
class VariantData {
  VariantContent content_;  // must be first to allow cast from array to variant
  VariantType type_;
  SlotId next_;
 public:
  static void* operator new(size_t, void* p) noexcept {
    return p;
  }
  static void operator delete(void*, void*) noexcept {}
  VariantData() : type_(VariantType::Null), next_(NULL_SLOT) {}
  SlotId next() const {
    return next_;
  }
  void setNext(SlotId slot) {
    next_ = slot;
  }
  template <typename TVisitor>
  typename TVisitor::result_type accept(
      TVisitor& visit, const ResourceManager* resources) const {
#if ARDUINOJSON_USE_EXTENSIONS
    auto extension = getExtension(resources);
#else
    (void)resources;  // silence warning
#endif
    switch (type_) {
      case VariantType::Float:
        return visit.visit(content_.asFloat);
#if ARDUINOJSON_USE_DOUBLE
      case VariantType::Double:
        return visit.visit(extension->asDouble);
#endif
      case VariantType::Array:
        return visit.visit(content_.asArray);
      case VariantType::Object:
        return visit.visit(content_.asObject);
      case VariantType::LinkedString:
        return visit.visit(JsonString(content_.asLinkedString));
      case VariantType::OwnedString:
        return visit.visit(JsonString(content_.asOwnedString->data,
                                      content_.asOwnedString->length,
                                      JsonString::Copied));
      case VariantType::RawString:
        return visit.visit(RawString(content_.asOwnedString->data,
                                     content_.asOwnedString->length));
      case VariantType::Int32:
        return visit.visit(static_cast<JsonInteger>(content_.asInt32));
      case VariantType::Uint32:
        return visit.visit(static_cast<JsonUInt>(content_.asUint32));
#if ARDUINOJSON_USE_LONG_LONG
      case VariantType::Int64:
        return visit.visit(extension->asInt64);
      case VariantType::Uint64:
        return visit.visit(extension->asUint64);
#endif
      case VariantType::Boolean:
        return visit.visit(content_.asBoolean != 0);
      default:
        return visit.visit(nullptr);
    }
  }
  template <typename TVisitor>
  static typename TVisitor::result_type accept(const VariantData* var,
                                               const ResourceManager* resources,
                                               TVisitor& visit) {
    if (var != 0)
      return var->accept(visit, resources);
    else
      return visit.visit(nullptr);
  }
  VariantData* addElement(ResourceManager* resources) {
    auto array = isNull() ? &toArray() : asArray();
    return detail::ArrayData::addElement(array, resources);
  }
  static VariantData* addElement(VariantData* var, ResourceManager* resources) {
    if (!var)
      return nullptr;
    return var->addElement(resources);
  }
  template <typename T>
  bool addValue(T&& value, ResourceManager* resources) {
    auto array = isNull() ? &toArray() : asArray();
    return detail::ArrayData::addValue(array, detail::forward<T>(value),
                                       resources);
  }
  template <typename T>
  static bool addValue(VariantData* var, T&& value,
                       ResourceManager* resources) {
    if (!var)
      return false;
    return var->addValue(value, resources);
  }
  bool asBoolean(const ResourceManager* resources) const {
#if ARDUINOJSON_USE_EXTENSIONS
    auto extension = getExtension(resources);
#else
    (void)resources;  // silence warning
#endif
    switch (type_) {
      case VariantType::Boolean:
        return content_.asBoolean;
      case VariantType::Uint32:
      case VariantType::Int32:
        return content_.asUint32 != 0;
      case VariantType::Float:
        return content_.asFloat != 0;
#if ARDUINOJSON_USE_DOUBLE
      case VariantType::Double:
        return extension->asDouble != 0;
#endif
      case VariantType::Null:
        return false;
#if ARDUINOJSON_USE_LONG_LONG
      case VariantType::Uint64:
      case VariantType::Int64:
        return extension->asUint64 != 0;
#endif
      default:
        return true;
    }
  }
  ArrayData* asArray() {
    return isArray() ? &content_.asArray : 0;
  }
  const ArrayData* asArray() const {
    return const_cast<VariantData*>(this)->asArray();
  }
  CollectionData* asCollection() {
    return isCollection() ? &content_.asCollection : 0;
  }
  const CollectionData* asCollection() const {
    return const_cast<VariantData*>(this)->asCollection();
  }
  template <typename T>
  T asFloat(const ResourceManager* resources) const {
    static_assert(is_floating_point<T>::value, "T must be a floating point");
#if ARDUINOJSON_USE_EXTENSIONS
    auto extension = getExtension(resources);
#else
    (void)resources;  // silence warning
#endif
    switch (type_) {
      case VariantType::Boolean:
        return static_cast<T>(content_.asBoolean);
      case VariantType::Uint32:
        return static_cast<T>(content_.asUint32);
      case VariantType::Int32:
        return static_cast<T>(content_.asInt32);
#if ARDUINOJSON_USE_LONG_LONG
      case VariantType::Uint64:
        return static_cast<T>(extension->asUint64);
      case VariantType::Int64:
        return static_cast<T>(extension->asInt64);
#endif
      case VariantType::LinkedString:
      case VariantType::OwnedString:
        return parseNumber<T>(content_.asOwnedString->data);
      case VariantType::Float:
        return static_cast<T>(content_.asFloat);
#if ARDUINOJSON_USE_DOUBLE
      case VariantType::Double:
        return static_cast<T>(extension->asDouble);
#endif
      default:
        return 0;
    }
  }
  template <typename T>
  T asIntegral(const ResourceManager* resources) const {
    static_assert(is_integral<T>::value, "T must be an integral type");
#if ARDUINOJSON_USE_EXTENSIONS
    auto extension = getExtension(resources);
#else
    (void)resources;  // silence warning
#endif
    switch (type_) {
      case VariantType::Boolean:
        return content_.asBoolean;
      case VariantType::Uint32:
        return convertNumber<T>(content_.asUint32);
      case VariantType::Int32:
        return convertNumber<T>(content_.asInt32);
#if ARDUINOJSON_USE_LONG_LONG
      case VariantType::Uint64:
        return convertNumber<T>(extension->asUint64);
      case VariantType::Int64:
        return convertNumber<T>(extension->asInt64);
#endif
      case VariantType::LinkedString:
        return parseNumber<T>(content_.asLinkedString);
      case VariantType::OwnedString:
        return parseNumber<T>(content_.asOwnedString->data);
      case VariantType::Float:
        return convertNumber<T>(content_.asFloat);
#if ARDUINOJSON_USE_DOUBLE
      case VariantType::Double:
        return convertNumber<T>(extension->asDouble);
#endif
      default:
        return 0;
    }
  }
  ObjectData* asObject() {
    return isObject() ? &content_.asObject : 0;
  }
  const ObjectData* asObject() const {
    return const_cast<VariantData*>(this)->asObject();
  }
  JsonString asRawString() const {
    switch (type_) {
      case VariantType::RawString:
        return JsonString(content_.asOwnedString->data,
                          content_.asOwnedString->length, JsonString::Copied);
      default:
        return JsonString();
    }
  }
  JsonString asString() const {
    switch (type_) {
      case VariantType::LinkedString:
        return JsonString(content_.asLinkedString, JsonString::Linked);
      case VariantType::OwnedString:
        return JsonString(content_.asOwnedString->data,
                          content_.asOwnedString->length, JsonString::Copied);
      default:
        return JsonString();
    }
  }
#if ARDUINOJSON_USE_EXTENSIONS
  const VariantExtension* getExtension(const ResourceManager* resources) const;
#endif
  VariantData* getElement(size_t index,
                          const ResourceManager* resources) const {
    return ArrayData::getElement(asArray(), index, resources);
  }
  static VariantData* getElement(const VariantData* var, size_t index,
                                 const ResourceManager* resources) {
    return var != 0 ? var->getElement(index, resources) : 0;
  }
  template <typename TAdaptedString>
  VariantData* getMember(TAdaptedString key,
                         const ResourceManager* resources) const {
    return ObjectData::getMember(asObject(), key, resources);
  }
  template <typename TAdaptedString>
  static VariantData* getMember(const VariantData* var, TAdaptedString key,
                                const ResourceManager* resources) {
    if (!var)
      return 0;
    return var->getMember(key, resources);
  }
  VariantData* getOrAddElement(size_t index, ResourceManager* resources) {
    auto array = isNull() ? &toArray() : asArray();
    if (!array)
      return nullptr;
    return array->getOrAddElement(index, resources);
  }
  template <typename TAdaptedString>
  VariantData* getOrAddMember(TAdaptedString key, ResourceManager* resources) {
    if (key.isNull())
      return nullptr;
    auto obj = isNull() ? &toObject() : asObject();
    if (!obj)
      return nullptr;
    return obj->getOrAddMember(key, resources);
  }
  bool isArray() const {
    return type_ == VariantType::Array;
  }
  bool isBoolean() const {
    return type_ == VariantType::Boolean;
  }
  bool isCollection() const {
    return type_ & VariantTypeBits::CollectionMask;
  }
  bool isFloat() const {
    return type_ & VariantTypeBits::NumberBit;
  }
  template <typename T>
  bool isInteger(const ResourceManager* resources) const {
#if ARDUINOJSON_USE_LONG_LONG
    auto extension = getExtension(resources);
#else
    (void)resources;  // silence warning
#endif
    switch (type_) {
      case VariantType::Uint32:
        return canConvertNumber<T>(content_.asUint32);
      case VariantType::Int32:
        return canConvertNumber<T>(content_.asInt32);
#if ARDUINOJSON_USE_LONG_LONG
      case VariantType::Uint64:
        return canConvertNumber<T>(extension->asUint64);
      case VariantType::Int64:
        return canConvertNumber<T>(extension->asInt64);
#endif
      default:
        return false;
    }
  }
  bool isNull() const {
    return type_ == VariantType::Null;
  }
  static bool isNull(const VariantData* var) {
    if (!var)
      return true;
    return var->isNull();
  }
  bool isObject() const {
    return type_ == VariantType::Object;
  }
  bool isString() const {
    return type_ == VariantType::LinkedString ||
           type_ == VariantType::OwnedString;
  }
  size_t nesting(const ResourceManager* resources) const {
    auto collection = asCollection();
    if (collection)
      return collection->nesting(resources);
    else
      return 0;
  }
  static size_t nesting(const VariantData* var,
                        const ResourceManager* resources) {
    if (!var)
      return 0;
    return var->nesting(resources);
  }
  void removeElement(size_t index, ResourceManager* resources) {
    ArrayData::removeElement(asArray(), index, resources);
  }
  static void removeElement(VariantData* var, size_t index,
                            ResourceManager* resources) {
    if (!var)
      return;
    var->removeElement(index, resources);
  }
  template <typename TAdaptedString>
  void removeMember(TAdaptedString key, ResourceManager* resources) {
    ObjectData::removeMember(asObject(), key, resources);
  }
  template <typename TAdaptedString>
  static void removeMember(VariantData* var, TAdaptedString key,
                           ResourceManager* resources) {
    if (!var)
      return;
    var->removeMember(key, resources);
  }
  void reset() {  // TODO: remove
    type_ = VariantType::Null;
  }
  void setBoolean(bool value) {
    ARDUINOJSON_ASSERT(type_ == VariantType::Null);  // must call clear() first
    type_ = VariantType::Boolean;
    content_.asBoolean = value;
  }
  template <typename T>
  enable_if_t<sizeof(T) == 4, bool> setFloat(T value, ResourceManager*) {
    ARDUINOJSON_ASSERT(type_ == VariantType::Null);  // must call clear() first
    type_ = VariantType::Float;
    content_.asFloat = value;
    return true;
  }
  template <typename T>
  enable_if_t<sizeof(T) == 8, bool> setFloat(T value, ResourceManager*);
  template <typename T>
  enable_if_t<is_signed<T>::value, bool> setInteger(T value,
                                                    ResourceManager* resources);
  template <typename T>
  enable_if_t<is_unsigned<T>::value, bool> setInteger(
      T value, ResourceManager* resources);
  void setRawString(StringNode* s) {
    ARDUINOJSON_ASSERT(type_ == VariantType::Null);  // must call clear() first
    ARDUINOJSON_ASSERT(s);
    type_ = VariantType::RawString;
    content_.asOwnedString = s;
  }
  template <typename T>
  void setRawString(SerializedValue<T> value, ResourceManager* resources);
  template <typename T>
  static void setRawString(VariantData* var, SerializedValue<T> value,
                           ResourceManager* resources) {
    if (!var)
      return;
    var->clear(resources);
    var->setRawString(value, resources);
  }
  template <typename TAdaptedString>
  bool setString(TAdaptedString value, ResourceManager* resources);
  bool setString(StringNode* s, ResourceManager*) {
    setOwnedString(s);
    return true;
  }
  template <typename TAdaptedString>
  static void setString(VariantData* var, TAdaptedString value,
                        ResourceManager* resources) {
    if (!var)
      return;
    var->clear(resources);
    var->setString(value, resources);
  }
  void setLinkedString(const char* s) {
    ARDUINOJSON_ASSERT(type_ == VariantType::Null);  // must call clear() first
    ARDUINOJSON_ASSERT(s);
    type_ = VariantType::LinkedString;
    content_.asLinkedString = s;
  }
  void setOwnedString(StringNode* s) {
    ARDUINOJSON_ASSERT(type_ == VariantType::Null);  // must call clear() first
    ARDUINOJSON_ASSERT(s);
    type_ = VariantType::OwnedString;
    content_.asOwnedString = s;
  }
  size_t size(const ResourceManager* resources) const {
    if (isObject())
      return content_.asObject.size(resources);
    if (isArray())
      return content_.asArray.size(resources);
    return 0;
  }
  static size_t size(const VariantData* var, const ResourceManager* resources) {
    return var != 0 ? var->size(resources) : 0;
  }
  ArrayData& toArray() {
    ARDUINOJSON_ASSERT(type_ == VariantType::Null);  // must call clear() first
    type_ = VariantType::Array;
    new (&content_.asArray) ArrayData();
    return content_.asArray;
  }
  static ArrayData* toArray(VariantData* var, ResourceManager* resources) {
    if (!var)
      return 0;
    var->clear(resources);
    return &var->toArray();
  }
  ObjectData& toObject() {
    ARDUINOJSON_ASSERT(type_ == VariantType::Null);  // must call clear() first
    type_ = VariantType::Object;
    new (&content_.asObject) ObjectData();
    return content_.asObject;
  }
  static ObjectData* toObject(VariantData* var, ResourceManager* resources) {
    if (!var)
      return 0;
    var->clear(resources);
    return &var->toObject();
  }
  VariantType type() const {
    return type_;
  }
  void clear(ResourceManager* resources);
  static void clear(VariantData* var, ResourceManager* resources) {
    if (!var)
      return;
    var->clear(resources);
  }
};
class VariantData;
class VariantWithId;
class ResourceManager {
  union SlotData {
    VariantData variant;
#if ARDUINOJSON_USE_EXTENSIONS
    VariantExtension extension;
#endif
  };
 public:
  constexpr static size_t slotSize = sizeof(SlotData);
  ResourceManager(Allocator* allocator = DefaultAllocator::instance())
      : allocator_(allocator), overflowed_(false) {}
  ~ResourceManager() {
    stringPool_.clear(allocator_);
    variantPools_.clear(allocator_);
  }
  ResourceManager(const ResourceManager&) = delete;
  ResourceManager& operator=(const ResourceManager& src) = delete;
  friend void swap(ResourceManager& a, ResourceManager& b) {
    swap(a.stringPool_, b.stringPool_);
    swap(a.variantPools_, b.variantPools_);
    swap_(a.allocator_, b.allocator_);
    swap_(a.overflowed_, b.overflowed_);
  }
  Allocator* allocator() const {
    return allocator_;
  }
  size_t size() const {
    return variantPools_.size() + stringPool_.size();
  }
  bool overflowed() const {
    return overflowed_;
  }
  Slot<VariantData> allocVariant();
  void freeVariant(Slot<VariantData> slot);
  VariantData* getVariant(SlotId id) const;
#if ARDUINOJSON_USE_EXTENSIONS
  Slot<VariantExtension> allocExtension();
  void freeExtension(SlotId slot);
  VariantExtension* getExtension(SlotId id) const;
#endif
  template <typename TAdaptedString>
  StringNode* saveString(TAdaptedString str) {
    if (str.isNull())
      return 0;
    auto node = stringPool_.add(str, allocator_);
    if (!node)
      overflowed_ = true;
    return node;
  }
  void saveString(StringNode* node) {
    stringPool_.add(node);
  }
  template <typename TAdaptedString>
  StringNode* getString(const TAdaptedString& str) const {
    return stringPool_.get(str);
  }
  StringNode* createString(size_t length) {
    auto node = StringNode::create(length, allocator_);
    if (!node)
      overflowed_ = true;
    return node;
  }
  StringNode* resizeString(StringNode* node, size_t length) {
    node = StringNode::resize(node, length, allocator_);
    if (!node)
      overflowed_ = true;
    return node;
  }
  void destroyString(StringNode* node) {
    StringNode::destroy(node, allocator_);
  }
  void dereferenceString(const char* s) {
    stringPool_.dereference(s, allocator_);
  }
  void clear() {
    variantPools_.clear(allocator_);
    overflowed_ = false;
    stringPool_.clear(allocator_);
  }
  void shrinkToFit() {
    variantPools_.shrinkToFit(allocator_);
  }
 private:
  Allocator* allocator_;
  bool overflowed_;
  StringPool stringPool_;
  MemoryPoolList<SlotData> variantPools_;
};
template <typename T, typename Enable = void>
struct IsString : false_type {};
template <typename T>
struct IsString<T, void_t<typename StringAdapter<T>::AdaptedString>>
    : true_type {};
ARDUINOJSON_END_PRIVATE_NAMESPACE
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
class JsonArray;
class JsonObject;
class JsonVariant;
ARDUINOJSON_END_PUBLIC_NAMESPACE
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename T>
struct VariantTo {};
template <>
struct VariantTo<JsonArray> {
  typedef JsonArray type;
};
template <>
struct VariantTo<JsonObject> {
  typedef JsonObject type;
};
template <>
struct VariantTo<JsonVariant> {
  typedef JsonVariant type;
};
class VariantAttorney {
 public:
  template <typename TClient>
  static auto getResourceManager(TClient& client)
      -> decltype(client.getResourceManager()) {
    return client.getResourceManager();
  }
  template <typename TClient>
  static auto getData(TClient& client) -> decltype(client.getData()) {
    return client.getData();
  }
  template <typename TClient>
  static VariantData* getOrCreateData(TClient& client) {
    return client.getOrCreateData();
  }
};
enum CompareResult {
  COMPARE_RESULT_DIFFER = 0,
  COMPARE_RESULT_EQUAL = 1,
  COMPARE_RESULT_GREATER = 2,
  COMPARE_RESULT_LESS = 4,
  COMPARE_RESULT_GREATER_OR_EQUAL = 3,
  COMPARE_RESULT_LESS_OR_EQUAL = 5
};
template <typename T>
CompareResult arithmeticCompare(const T& lhs, const T& rhs) {
  if (lhs < rhs)
    return COMPARE_RESULT_LESS;
  else if (lhs > rhs)
    return COMPARE_RESULT_GREATER;
  else
    return COMPARE_RESULT_EQUAL;
}
template <typename T1, typename T2>
CompareResult arithmeticCompare(
    const T1& lhs, const T2& rhs,
    enable_if_t<is_integral<T1>::value && is_integral<T2>::value &&
                sizeof(T1) < sizeof(T2)>* = 0) {
  return arithmeticCompare<T2>(static_cast<T2>(lhs), rhs);
}
template <typename T1, typename T2>
CompareResult arithmeticCompare(
    const T1& lhs, const T2& rhs,
    enable_if_t<is_integral<T1>::value && is_integral<T2>::value &&
                sizeof(T2) < sizeof(T1)>* = 0) {
  return arithmeticCompare<T1>(lhs, static_cast<T1>(rhs));
}
template <typename T1, typename T2>
CompareResult arithmeticCompare(
    const T1& lhs, const T2& rhs,
    enable_if_t<is_integral<T1>::value && is_integral<T2>::value &&
                is_signed<T1>::value == is_signed<T2>::value &&
                sizeof(T2) == sizeof(T1)>* = 0) {
  return arithmeticCompare<T1>(lhs, static_cast<T1>(rhs));
}
template <typename T1, typename T2>
CompareResult arithmeticCompare(
    const T1& lhs, const T2& rhs,
    enable_if_t<is_integral<T1>::value && is_integral<T2>::value &&
                is_unsigned<T1>::value && is_signed<T2>::value &&
                sizeof(T2) == sizeof(T1)>* = 0) {
  if (rhs < 0)
    return COMPARE_RESULT_GREATER;
  return arithmeticCompare<T1>(lhs, static_cast<T1>(rhs));
}
template <typename T1, typename T2>
CompareResult arithmeticCompare(
    const T1& lhs, const T2& rhs,
    enable_if_t<is_integral<T1>::value && is_integral<T2>::value &&
                is_signed<T1>::value && is_unsigned<T2>::value &&
                sizeof(T2) == sizeof(T1)>* = 0) {
  if (lhs < 0)
    return COMPARE_RESULT_LESS;
  return arithmeticCompare<T2>(static_cast<T2>(lhs), rhs);
}
template <typename T1, typename T2>
CompareResult arithmeticCompare(
    const T1& lhs, const T2& rhs,
    enable_if_t<is_floating_point<T1>::value || is_floating_point<T2>::value>* =
        0) {
  return arithmeticCompare<double>(static_cast<double>(lhs),
                                   static_cast<double>(rhs));
}
template <typename T2>
CompareResult arithmeticCompareNegateLeft(
    JsonUInt, const T2&, enable_if_t<is_unsigned<T2>::value>* = 0) {
  return COMPARE_RESULT_LESS;
}
template <typename T2>
CompareResult arithmeticCompareNegateLeft(
    JsonUInt lhs, const T2& rhs, enable_if_t<is_signed<T2>::value>* = 0) {
  if (rhs > 0)
    return COMPARE_RESULT_LESS;
  return arithmeticCompare(-rhs, static_cast<T2>(lhs));
}
template <typename T1>
CompareResult arithmeticCompareNegateRight(
    const T1&, JsonUInt, enable_if_t<is_unsigned<T1>::value>* = 0) {
  return COMPARE_RESULT_GREATER;
}
template <typename T1>
CompareResult arithmeticCompareNegateRight(
    const T1& lhs, JsonUInt rhs, enable_if_t<is_signed<T1>::value>* = 0) {
  if (lhs > 0)
    return COMPARE_RESULT_GREATER;
  return arithmeticCompare(static_cast<T1>(rhs), -lhs);
}
struct VariantTag {};
template <typename T>
struct IsVariant : is_base_of<VariantTag, T> {};
ARDUINOJSON_END_PRIVATE_NAMESPACE
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
class JsonVariantConst;
ARDUINOJSON_END_PUBLIC_NAMESPACE
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename T>
CompareResult compare(JsonVariantConst lhs,
                      const T& rhs);  // VariantCompare.cpp
struct VariantOperatorTag {};
template <typename TVariant>
struct VariantOperators : VariantOperatorTag {
  template <typename T>
  friend enable_if_t<!IsVariant<T>::value && !is_array<T>::value, T> operator|(
      const TVariant& variant, const T& defaultValue) {
    if (variant.template is<T>())
      return variant.template as<T>();
    else
      return defaultValue;
  }
  friend const char* operator|(const TVariant& variant,
                               const char* defaultValue) {
    if (variant.template is<const char*>())
      return variant.template as<const char*>();
    else
      return defaultValue;
  }
  template <typename T>
  friend enable_if_t<IsVariant<T>::value, JsonVariantConst> operator|(
      const TVariant& variant, T defaultValue) {
    if (variant)
      return variant;
    else
      return defaultValue;
  }
  template <typename T>
  friend bool operator==(T* lhs, TVariant rhs) {
    return compare(rhs, lhs) == COMPARE_RESULT_EQUAL;
  }
  template <typename T>
  friend bool operator==(const T& lhs, TVariant rhs) {
    return compare(rhs, lhs) == COMPARE_RESULT_EQUAL;
  }
  template <typename T>
  friend bool operator==(TVariant lhs, T* rhs) {
    return compare(lhs, rhs) == COMPARE_RESULT_EQUAL;
  }
  template <typename T>
  friend enable_if_t<!is_base_of<VariantOperatorTag, T>::value, bool>
  operator==(TVariant lhs, const T& rhs) {
    return compare(lhs, rhs) == COMPARE_RESULT_EQUAL;
  }
  template <typename T>
  friend bool operator!=(T* lhs, TVariant rhs) {
    return compare(rhs, lhs) != COMPARE_RESULT_EQUAL;
  }
  template <typename T>
  friend bool operator!=(const T& lhs, TVariant rhs) {
    return compare(rhs, lhs) != COMPARE_RESULT_EQUAL;
  }
  template <typename T>
  friend bool operator!=(TVariant lhs, T* rhs) {
    return compare(lhs, rhs) != COMPARE_RESULT_EQUAL;
  }
  template <typename T>
  friend enable_if_t<!is_base_of<VariantOperatorTag, T>::value, bool>
  operator!=(TVariant lhs, const T& rhs) {
    return compare(lhs, rhs) != COMPARE_RESULT_EQUAL;
  }
  template <typename T>
  friend bool operator<(T* lhs, TVariant rhs) {
    return compare(rhs, lhs) == COMPARE_RESULT_GREATER;
  }
  template <typename T>
  friend bool operator<(const T& lhs, TVariant rhs) {
    return compare(rhs, lhs) == COMPARE_RESULT_GREATER;
  }
  template <typename T>
  friend bool operator<(TVariant lhs, T* rhs) {
    return compare(lhs, rhs) == COMPARE_RESULT_LESS;
  }
  template <typename T>
  friend enable_if_t<!is_base_of<VariantOperatorTag, T>::value, bool> operator<(
      TVariant lhs, const T& rhs) {
    return compare(lhs, rhs) == COMPARE_RESULT_LESS;
  }
  template <typename T>
  friend bool operator<=(T* lhs, TVariant rhs) {
    return (compare(rhs, lhs) & COMPARE_RESULT_GREATER_OR_EQUAL) != 0;
  }
  template <typename T>
  friend bool operator<=(const T& lhs, TVariant rhs) {
    return (compare(rhs, lhs) & COMPARE_RESULT_GREATER_OR_EQUAL) != 0;
  }
  template <typename T>
  friend bool operator<=(TVariant lhs, T* rhs) {
    return (compare(lhs, rhs) & COMPARE_RESULT_LESS_OR_EQUAL) != 0;
  }
  template <typename T>
  friend enable_if_t<!is_base_of<VariantOperatorTag, T>::value, bool>
  operator<=(TVariant lhs, const T& rhs) {
    return (compare(lhs, rhs) & COMPARE_RESULT_LESS_OR_EQUAL) != 0;
  }
  template <typename T>
  friend bool operator>(T* lhs, TVariant rhs) {
    return compare(rhs, lhs) == COMPARE_RESULT_LESS;
  }
  template <typename T>
  friend bool operator>(const T& lhs, TVariant rhs) {
    return compare(rhs, lhs) == COMPARE_RESULT_LESS;
  }
  template <typename T>
  friend bool operator>(TVariant lhs, T* rhs) {
    return compare(lhs, rhs) == COMPARE_RESULT_GREATER;
  }
  template <typename T>
  friend enable_if_t<!is_base_of<VariantOperatorTag, T>::value, bool> operator>(
      TVariant lhs, const T& rhs) {
    return compare(lhs, rhs) == COMPARE_RESULT_GREATER;
  }
  template <typename T>
  friend bool operator>=(T* lhs, TVariant rhs) {
    return (compare(rhs, lhs) & COMPARE_RESULT_LESS_OR_EQUAL) != 0;
  }
  template <typename T>
  friend bool operator>=(const T& lhs, TVariant rhs) {
    return (compare(rhs, lhs) & COMPARE_RESULT_LESS_OR_EQUAL) != 0;
  }
  template <typename T>
  friend bool operator>=(TVariant lhs, T* rhs) {
    return (compare(lhs, rhs) & COMPARE_RESULT_GREATER_OR_EQUAL) != 0;
  }
  template <typename T>
  friend enable_if_t<!is_base_of<VariantOperatorTag, T>::value, bool>
  operator>=(TVariant lhs, const T& rhs) {
    return (compare(lhs, rhs) & COMPARE_RESULT_GREATER_OR_EQUAL) != 0;
  }
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
class JsonArray;
class JsonObject;
class JsonVariantConst : public detail::VariantTag,
                         public detail::VariantOperators<JsonVariantConst> {
  friend class detail::VariantAttorney;
  template <typename T>
  using ConversionSupported =
      detail::is_same<typename detail::function_traits<
                          decltype(&Converter<T>::fromJson)>::arg1_type,
                      JsonVariantConst>;
 public:
  JsonVariantConst() : data_(nullptr), resources_(nullptr) {}
  explicit JsonVariantConst(const detail::VariantData* data,
                            const detail::ResourceManager* resources)
      : data_(data), resources_(resources) {}
  bool isNull() const {
    return detail::VariantData::isNull(data_);
  }
  bool isUnbound() const {
    return !data_;
  }
  size_t nesting() const {
    return detail::VariantData::nesting(data_, resources_);
  }
  size_t size() const {
    return detail::VariantData::size(data_, resources_);
  }
  template <typename T,
            detail::enable_if_t<ConversionSupported<T>::value, bool> = true>
  T as() const {
    return Converter<T>::fromJson(*this);
  }
  template <typename T,
            detail::enable_if_t<!ConversionSupported<T>::value, bool> = true>
  detail::InvalidConversion<JsonVariantConst, T> as() const;
  template <typename T>
  detail::enable_if_t<ConversionSupported<T>::value, bool> is() const {
    return Converter<T>::checkJson(*this);
  }
  template <typename T>
  detail::enable_if_t<!ConversionSupported<T>::value, bool> is() const {
    return false;
  }
  template <typename T>
  operator T() const {
    return as<T>();
  }
  template <typename T>
  detail::enable_if_t<detail::is_integral<T>::value, JsonVariantConst>
  operator[](T index) const {
    return JsonVariantConst(
        detail::VariantData::getElement(data_, size_t(index), resources_),
        resources_);
  }
  template <typename TString>
  detail::enable_if_t<detail::IsString<TString>::value, JsonVariantConst>
  operator[](const TString& key) const {
    return JsonVariantConst(detail::VariantData::getMember(
                                data_, detail::adaptString(key), resources_),
                            resources_);
  }
  template <typename TChar>
  detail::enable_if_t<detail::IsString<TChar*>::value, JsonVariantConst>
  operator[](TChar* key) const {
    return JsonVariantConst(detail::VariantData::getMember(
                                data_, detail::adaptString(key), resources_),
                            resources_);
  }
  template <typename TVariant>
  detail::enable_if_t<detail::IsVariant<TVariant>::value, JsonVariantConst>
  operator[](const TVariant& key) const {
    if (key.template is<size_t>())
      return operator[](key.template as<size_t>());
    else
      return operator[](key.template as<const char*>());
  }
  template <typename TString>
  ARDUINOJSON_DEPRECATED("use var[key].is<T>() instead")
  detail::enable_if_t<detail::IsString<TString>::value, bool> containsKey(
      const TString& key) const {
    return detail::VariantData::getMember(getData(), detail::adaptString(key),
                                          resources_) != 0;
  }
  template <typename TChar>
  ARDUINOJSON_DEPRECATED("use obj[\"key\"].is<T>() instead")
  detail::enable_if_t<detail::IsString<TChar*>::value, bool> containsKey(
      TChar* key) const {
    return detail::VariantData::getMember(getData(), detail::adaptString(key),
                                          resources_) != 0;
  }
  template <typename TVariant>
  ARDUINOJSON_DEPRECATED("use var[key].is<T>() instead")
  detail::enable_if_t<detail::IsVariant<TVariant>::value, bool> containsKey(
      const TVariant& key) const {
    return containsKey(key.template as<const char*>());
  }
  ARDUINOJSON_DEPRECATED("always returns zero")
  size_t memoryUsage() const {
    return 0;
  }
 protected:
  const detail::VariantData* getData() const {
    return data_;
  }
  const detail::ResourceManager* getResourceManager() const {
    return resources_;
  }
 private:
  const detail::VariantData* data_;
  const detail::ResourceManager* resources_;
};
class JsonVariant;
ARDUINOJSON_END_PUBLIC_NAMESPACE
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename>
class ElementProxy;
template <typename, typename>
class MemberProxy;
template <typename TDerived>
class VariantRefBase : public VariantTag {
  friend class VariantAttorney;
 public:
  void clear() const {
    VariantData::clear(getOrCreateData(), getResourceManager());
  }
  bool isNull() const {
    return VariantData::isNull(getData());
  }
  bool isUnbound() const {
    return !getData();
  }
  template <typename T>
  T as() const;
  template <typename T, typename = enable_if_t<!is_same<T, TDerived>::value>>
  operator T() const {
    return as<T>();
  }
  template <typename T>
  enable_if_t<is_same<T, JsonArray>::value, JsonArray> to() const;
  template <typename T>
  enable_if_t<is_same<T, JsonObject>::value, JsonObject> to() const;
  template <typename T>
  enable_if_t<is_same<T, JsonVariant>::value, JsonVariant> to() const;
  template <typename T>
  FORCE_INLINE bool is() const;
  template <typename T>
  bool set(const T& value) const {
    return doSet<Converter<remove_cv_t<T>>>(value);
  }
  template <typename T>
  bool set(T* value) const {
    return doSet<Converter<T*>>(value);
  }
  size_t size() const {
    return VariantData::size(getData(), getResourceManager());
  }
  size_t nesting() const {
    return VariantData::nesting(getData(), getResourceManager());
  }
  template <typename T>
  enable_if_t<!is_same<T, JsonVariant>::value, T> add() const {
    return add<JsonVariant>().template to<T>();
  }
  template <typename T>
  enable_if_t<is_same<T, JsonVariant>::value, T> add() const;
  template <typename T>
  bool add(const T& value) const {
    return detail::VariantData::addValue(getOrCreateData(), value,
                                         getResourceManager());
  }
  template <typename T>
  bool add(T* value) const {
    return detail::VariantData::addValue(getOrCreateData(), value,
                                         getResourceManager());
  }
  void remove(size_t index) const {
    VariantData::removeElement(getData(), index, getResourceManager());
  }
  template <typename TChar>
  enable_if_t<IsString<TChar*>::value> remove(TChar* key) const {
    VariantData::removeMember(getData(), adaptString(key),
                              getResourceManager());
  }
  template <typename TString>
  enable_if_t<IsString<TString>::value> remove(const TString& key) const {
    VariantData::removeMember(getData(), adaptString(key),
                              getResourceManager());
  }
  template <typename TVariant>
  enable_if_t<IsVariant<TVariant>::value> remove(const TVariant& key) const {
    if (key.template is<size_t>())
      remove(key.template as<size_t>());
    else
      remove(key.template as<const char*>());
  }
  ElementProxy<TDerived> operator[](size_t index) const;
  template <typename TString>
  ARDUINOJSON_DEPRECATED("use obj[key].is<T>() instead")
  enable_if_t<IsString<TString>::value, bool> containsKey(
      const TString& key) const;
  template <typename TChar>
  ARDUINOJSON_DEPRECATED("use obj[\"key\"].is<T>() instead")
  enable_if_t<IsString<TChar*>::value, bool> containsKey(TChar* key) const;
  template <typename TVariant>
  ARDUINOJSON_DEPRECATED("use obj[key].is<T>() instead")
  enable_if_t<IsVariant<TVariant>::value, bool> containsKey(
      const TVariant& key) const;
  template <typename TString>
  FORCE_INLINE
      enable_if_t<IsString<TString>::value, MemberProxy<TDerived, TString>>
      operator[](const TString& key) const;
  template <typename TChar>
  FORCE_INLINE
      enable_if_t<IsString<TChar*>::value, MemberProxy<TDerived, TChar*>>
      operator[](TChar* key) const;
  template <typename TVariant>
  enable_if_t<IsVariant<TVariant>::value, JsonVariantConst> operator[](
      const TVariant& key) const {
    if (key.template is<size_t>())
      return operator[](key.template as<size_t>());
    else
      return operator[](key.template as<const char*>());
  }
  ARDUINOJSON_DEPRECATED("use add<JsonVariant>() instead")
  JsonVariant add() const;
  ARDUINOJSON_DEPRECATED("use add<JsonArray>() instead")
  JsonArray createNestedArray() const;
  template <typename TChar>
  ARDUINOJSON_DEPRECATED("use var[key].to<JsonArray>() instead")
  JsonArray createNestedArray(TChar* key) const;
  template <typename TString>
  ARDUINOJSON_DEPRECATED("use var[key].to<JsonArray>() instead")
  JsonArray createNestedArray(const TString& key) const;
  ARDUINOJSON_DEPRECATED("use add<JsonObject>() instead")
  JsonObject createNestedObject() const;
  template <typename TChar>
  ARDUINOJSON_DEPRECATED("use var[key].to<JsonObject>() instead")
  JsonObject createNestedObject(TChar* key) const;
  template <typename TString>
  ARDUINOJSON_DEPRECATED("use var[key].to<JsonObject>() instead")
  JsonObject createNestedObject(const TString& key) const;
  ARDUINOJSON_DEPRECATED("always returns zero")
  size_t memoryUsage() const {
    return 0;
  }
  ARDUINOJSON_DEPRECATED("performs a deep copy")
  void shallowCopy(JsonVariantConst src) const {
    set(src);
  }
 private:
  TDerived& derived() {
    return static_cast<TDerived&>(*this);
  }
  const TDerived& derived() const {
    return static_cast<const TDerived&>(*this);
  }
  ResourceManager* getResourceManager() const {
    return VariantAttorney::getResourceManager(derived());
  }
  VariantData* getData() const {
    return VariantAttorney::getData(derived());
  }
  VariantData* getOrCreateData() const {
    return VariantAttorney::getOrCreateData(derived());
  }
  FORCE_INLINE FLArduinoJson::JsonVariant getVariant() const;
  FORCE_INLINE FLArduinoJson::JsonVariantConst getVariantConst() const {
    return FLArduinoJson::JsonVariantConst(getData(), getResourceManager());
  }
  template <typename T>
  FORCE_INLINE enable_if_t<is_same<T, JsonVariantConst>::value, T> getVariant()
      const {
    return getVariantConst();
  }
  template <typename T>
  FORCE_INLINE enable_if_t<is_same<T, JsonVariant>::value, T> getVariant()
      const {
    return getVariant();
  }
  template <typename TConverter, typename T>
  bool doSet(T&& value) const {
    return doSet<TConverter>(
        detail::forward<T>(value),
        is_same<typename function_traits<
                    decltype(&TConverter::toJson)>::return_type,
                bool>{});
  }
  template <typename TConverter, typename T>
  bool doSet(T&& value, false_type) const;
  template <typename TConverter, typename T>
  bool doSet(T&& value, true_type) const;
  FLArduinoJson::JsonVariant getOrCreateVariant() const;
};
template <typename TUpstream>
class ElementProxy : public VariantRefBase<ElementProxy<TUpstream>>,
                     public VariantOperators<ElementProxy<TUpstream>> {
  friend class VariantAttorney;
 public:
  ElementProxy(TUpstream upstream, size_t index)
      : upstream_(upstream), index_(index) {}
  ElementProxy(const ElementProxy& src)
      : upstream_(src.upstream_), index_(src.index_) {}
  ElementProxy& operator=(const ElementProxy& src) {
    this->set(src);
    return *this;
  }
  template <typename T>
  ElementProxy& operator=(const T& src) {
    this->set(src);
    return *this;
  }
  template <typename T>
  ElementProxy& operator=(T* src) {
    this->set(src);
    return *this;
  }
 private:
  ResourceManager* getResourceManager() const {
    return VariantAttorney::getResourceManager(upstream_);
  }
  FORCE_INLINE VariantData* getData() const {
    return VariantData::getElement(
        VariantAttorney::getData(upstream_), index_,
        VariantAttorney::getResourceManager(upstream_));
  }
  VariantData* getOrCreateData() const {
    auto data = VariantAttorney::getOrCreateData(upstream_);
    if (!data)
      return nullptr;
    return data->getOrAddElement(
        index_, VariantAttorney::getResourceManager(upstream_));
  }
  TUpstream upstream_;
  size_t index_;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
class JsonVariant : public detail::VariantRefBase<JsonVariant>,
                    public detail::VariantOperators<JsonVariant> {
  friend class detail::VariantAttorney;
 public:
  JsonVariant() : data_(0), resources_(0) {}
  JsonVariant(detail::VariantData* data, detail::ResourceManager* resources)
      : data_(data), resources_(resources) {}
 private:
  detail::ResourceManager* getResourceManager() const {
    return resources_;
  }
  detail::VariantData* getData() const {
    return data_;
  }
  detail::VariantData* getOrCreateData() const {
    return data_;
  }
  detail::VariantData* data_;
  detail::ResourceManager* resources_;
};
namespace detail {
bool copyVariant(JsonVariant dst, JsonVariantConst src);
}
template <>
struct Converter<JsonVariant> : private detail::VariantAttorney {
  static void toJson(JsonVariantConst src, JsonVariant dst) {
    copyVariant(dst, src);
  }
  static JsonVariant fromJson(JsonVariant src) {
    return src;
  }
  static bool checkJson(JsonVariant src) {
    auto data = getData(src);
    return !!data;
  }
};
template <>
struct Converter<JsonVariantConst> : private detail::VariantAttorney {
  static void toJson(JsonVariantConst src, JsonVariant dst) {
    copyVariant(dst, src);
  }
  static JsonVariantConst fromJson(JsonVariantConst src) {
    return JsonVariantConst(getData(src), getResourceManager(src));
  }
  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    return !!data;
  }
};
template <typename T>
class Ref {
 public:
  Ref(T value) : value_(value) {}
  T* operator->() {
    return &value_;
  }
  T& operator*() {
    return value_;
  }
 private:
  T value_;
};
class JsonArrayIterator {
  friend class JsonArray;
 public:
  JsonArrayIterator() {}
  explicit JsonArrayIterator(detail::ArrayData::iterator iterator,
                             detail::ResourceManager* resources)
      : iterator_(iterator), resources_(resources) {}
  JsonVariant operator*() {
    return JsonVariant(iterator_.data(), resources_);
  }
  Ref<JsonVariant> operator->() {
    return operator*();
  }
  bool operator==(const JsonArrayIterator& other) const {
    return iterator_ == other.iterator_;
  }
  bool operator!=(const JsonArrayIterator& other) const {
    return iterator_ != other.iterator_;
  }
  JsonArrayIterator& operator++() {
    iterator_.next(resources_);
    return *this;
  }
 private:
  detail::ArrayData::iterator iterator_;
  detail::ResourceManager* resources_;
};
class JsonArrayConstIterator {
  friend class JsonArray;
 public:
  JsonArrayConstIterator() {}
  explicit JsonArrayConstIterator(detail::ArrayData::iterator iterator,
                                  const detail::ResourceManager* resources)
      : iterator_(iterator), resources_(resources) {}
  JsonVariantConst operator*() const {
    return JsonVariantConst(iterator_.data(), resources_);
  }
  Ref<JsonVariantConst> operator->() {
    return operator*();
  }
  bool operator==(const JsonArrayConstIterator& other) const {
    return iterator_ == other.iterator_;
  }
  bool operator!=(const JsonArrayConstIterator& other) const {
    return iterator_ != other.iterator_;
  }
  JsonArrayConstIterator& operator++() {
    iterator_.next(resources_);
    return *this;
  }
 private:
  detail::ArrayData::iterator iterator_;
  const detail::ResourceManager* resources_;
};
class JsonObject;
class JsonArrayConst : public detail::VariantOperators<JsonArrayConst> {
  friend class JsonArray;
  friend class detail::VariantAttorney;
 public:
  typedef JsonArrayConstIterator iterator;
  iterator begin() const {
    if (!data_)
      return iterator();
    return iterator(data_->createIterator(resources_), resources_);
  }
  iterator end() const {
    return iterator();
  }
  JsonArrayConst() : data_(0), resources_(0) {}
  JsonArrayConst(const detail::ArrayData* data,
                 const detail::ResourceManager* resources)
      : data_(data), resources_(resources) {}
  template <typename T>
  detail::enable_if_t<detail::is_integral<T>::value, JsonVariantConst>
  operator[](T index) const {
    return JsonVariantConst(
        detail::ArrayData::getElement(data_, size_t(index), resources_),
        resources_);
  }
  template <typename TVariant>
  detail::enable_if_t<detail::IsVariant<TVariant>::value, JsonVariantConst>
  operator[](const TVariant& variant) const {
    if (variant.template is<size_t>())
      return operator[](variant.template as<size_t>());
    else
      return JsonVariantConst();
  }
  operator JsonVariantConst() const {
    return JsonVariantConst(getData(), resources_);
  }
  bool isNull() const {
    return data_ == 0;
  }
  operator bool() const {
    return data_ != 0;
  }
  size_t nesting() const {
    return detail::VariantData::nesting(getData(), resources_);
  }
  size_t size() const {
    return data_ ? data_->size(resources_) : 0;
  }
  ARDUINOJSON_DEPRECATED("always returns zero")
  size_t memoryUsage() const {
    return 0;
  }
 private:
  const detail::VariantData* getData() const {
    return collectionToVariant(data_);
  }
  const detail::ArrayData* data_;
  const detail::ResourceManager* resources_;
};
inline bool operator==(JsonArrayConst lhs, JsonArrayConst rhs) {
  if (!lhs && !rhs)
    return true;
  if (!lhs || !rhs)
    return false;
  auto a = lhs.begin();
  auto b = rhs.begin();
  for (;;) {
    if (a == b)  // same pointer or both null
      return true;
    if (a == lhs.end() || b == rhs.end())
      return false;
    if (*a != *b)
      return false;
    ++a;
    ++b;
  }
}
class JsonObject;
class JsonArray : public detail::VariantOperators<JsonArray> {
  friend class detail::VariantAttorney;
 public:
  typedef JsonArrayIterator iterator;
  JsonArray() : data_(0), resources_(0) {}
  JsonArray(detail::ArrayData* data, detail::ResourceManager* resources)
      : data_(data), resources_(resources) {}
  operator JsonVariant() {
    void* data = data_;  // prevent warning cast-align
    return JsonVariant(reinterpret_cast<detail::VariantData*>(data),
                       resources_);
  }
  operator JsonArrayConst() const {
    return JsonArrayConst(data_, resources_);
  }
  template <typename T>
  detail::enable_if_t<!detail::is_same<T, JsonVariant>::value, T> add() const {
    return add<JsonVariant>().to<T>();
  }
  template <typename T>
  detail::enable_if_t<detail::is_same<T, JsonVariant>::value, T> add() const {
    return JsonVariant(detail::ArrayData::addElement(data_, resources_),
                       resources_);
  }
  template <typename T>
  bool add(const T& value) const {
    return detail::ArrayData::addValue(data_, value, resources_);
  }
  template <typename T>
  bool add(T* value) const {
    return detail::ArrayData::addValue(data_, value, resources_);
  }
  iterator begin() const {
    if (!data_)
      return iterator();
    return iterator(data_->createIterator(resources_), resources_);
  }
  iterator end() const {
    return iterator();
  }
  bool set(JsonArrayConst src) const {
    if (!data_)
      return false;
    clear();
    for (auto element : src) {
      if (!add(element))
        return false;
    }
    return true;
  }
  void remove(iterator it) const {
    detail::ArrayData::remove(data_, it.iterator_, resources_);
  }
  void remove(size_t index) const {
    detail::ArrayData::removeElement(data_, index, resources_);
  }
  template <typename TVariant>
  detail::enable_if_t<detail::IsVariant<TVariant>::value> remove(
      TVariant variant) const {
    if (variant.template is<size_t>())
      remove(variant.template as<size_t>());
  }
  void clear() const {
    detail::ArrayData::clear(data_, resources_);
  }
  template <typename T>
  detail::enable_if_t<detail::is_integral<T>::value,
                      detail::ElementProxy<JsonArray>>
  operator[](T index) const {
    return {*this, size_t(index)};
  }
  template <typename TVariant>
  detail::enable_if_t<detail::IsVariant<TVariant>::value,
                      detail::ElementProxy<JsonArray>>
  operator[](const TVariant& variant) const {
    if (variant.template is<size_t>())
      return operator[](variant.template as<size_t>());
    else
      return {*this, size_t(-1)};
  }
  operator JsonVariantConst() const {
    return JsonVariantConst(collectionToVariant(data_), resources_);
  }
  bool isNull() const {
    return data_ == 0;
  }
  operator bool() const {
    return data_ != 0;
  }
  size_t nesting() const {
    return detail::VariantData::nesting(collectionToVariant(data_), resources_);
  }
  size_t size() const {
    return data_ ? data_->size(resources_) : 0;
  }
  ARDUINOJSON_DEPRECATED("use add<JsonVariant>() instead")
  JsonVariant add() const {
    return add<JsonVariant>();
  }
  ARDUINOJSON_DEPRECATED("use add<JsonArray>() instead")
  JsonArray createNestedArray() const {
    return add<JsonArray>();
  }
  ARDUINOJSON_DEPRECATED("use add<JsonObject>() instead")
  JsonObject createNestedObject() const;
  ARDUINOJSON_DEPRECATED("always returns zero")
  size_t memoryUsage() const {
    return 0;
  }
 private:
  detail::ResourceManager* getResourceManager() const {
    return resources_;
  }
  detail::VariantData* getData() const {
    return collectionToVariant(data_);
  }
  detail::VariantData* getOrCreateData() const {
    return collectionToVariant(data_);
  }
  detail::ArrayData* data_;
  detail::ResourceManager* resources_;
};
class JsonPair {
 public:
  JsonPair(detail::ObjectData::iterator iterator,
           detail::ResourceManager* resources) {
    if (!iterator.done()) {
      key_ = iterator->asString();
      iterator.next(resources);
      value_ = JsonVariant(iterator.data(), resources);
    }
  }
  JsonString key() const {
    return key_;
  }
  JsonVariant value() {
    return value_;
  }
 private:
  JsonString key_;
  JsonVariant value_;
};
class JsonPairConst {
 public:
  JsonPairConst(detail::ObjectData::iterator iterator,
                const detail::ResourceManager* resources) {
    if (!iterator.done()) {
      key_ = iterator->asString();
      iterator.next(resources);
      value_ = JsonVariantConst(iterator.data(), resources);
    }
  }
  JsonString key() const {
    return key_;
  }
  JsonVariantConst value() const {
    return value_;
  }
 private:
  JsonString key_;
  JsonVariantConst value_;
};
class JsonObjectIterator {
  friend class JsonObject;
 public:
  JsonObjectIterator() {}
  explicit JsonObjectIterator(detail::ObjectData::iterator iterator,
                              detail::ResourceManager* resources)
      : iterator_(iterator), resources_(resources) {}
  JsonPair operator*() const {
    return JsonPair(iterator_, resources_);
  }
  Ref<JsonPair> operator->() {
    return operator*();
  }
  bool operator==(const JsonObjectIterator& other) const {
    return iterator_ == other.iterator_;
  }
  bool operator!=(const JsonObjectIterator& other) const {
    return iterator_ != other.iterator_;
  }
  JsonObjectIterator& operator++() {
    iterator_.next(resources_);  // key
    iterator_.next(resources_);  // value
    return *this;
  }
 private:
  detail::ObjectData::iterator iterator_;
  detail::ResourceManager* resources_;
};
class JsonObjectConstIterator {
  friend class JsonObject;
 public:
  JsonObjectConstIterator() {}
  explicit JsonObjectConstIterator(detail::ObjectData::iterator iterator,
                                   const detail::ResourceManager* resources)
      : iterator_(iterator), resources_(resources) {}
  JsonPairConst operator*() const {
    return JsonPairConst(iterator_, resources_);
  }
  Ref<JsonPairConst> operator->() {
    return operator*();
  }
  bool operator==(const JsonObjectConstIterator& other) const {
    return iterator_ == other.iterator_;
  }
  bool operator!=(const JsonObjectConstIterator& other) const {
    return iterator_ != other.iterator_;
  }
  JsonObjectConstIterator& operator++() {
    iterator_.next(resources_);  // key
    iterator_.next(resources_);  // value
    return *this;
  }
 private:
  detail::ObjectData::iterator iterator_;
  const detail::ResourceManager* resources_;
};
class JsonObjectConst : public detail::VariantOperators<JsonObjectConst> {
  friend class JsonObject;
  friend class detail::VariantAttorney;
 public:
  typedef JsonObjectConstIterator iterator;
  JsonObjectConst() : data_(0), resources_(0) {}
  JsonObjectConst(const detail::ObjectData* data,
                  const detail::ResourceManager* resources)
      : data_(data), resources_(resources) {}
  operator JsonVariantConst() const {
    return JsonVariantConst(getData(), resources_);
  }
  bool isNull() const {
    return data_ == 0;
  }
  operator bool() const {
    return data_ != 0;
  }
  size_t nesting() const {
    return detail::VariantData::nesting(getData(), resources_);
  }
  size_t size() const {
    return data_ ? data_->size(resources_) : 0;
  }
  iterator begin() const {
    if (!data_)
      return iterator();
    return iterator(data_->createIterator(resources_), resources_);
  }
  iterator end() const {
    return iterator();
  }
  template <typename TString>
  ARDUINOJSON_DEPRECATED("use obj[key].is<T>() instead")
  detail::enable_if_t<detail::IsString<TString>::value, bool> containsKey(
      const TString& key) const {
    return detail::ObjectData::getMember(data_, detail::adaptString(key),
                                         resources_) != 0;
  }
  template <typename TChar>
  ARDUINOJSON_DEPRECATED("use obj[\"key\"].is<T>() instead")
  bool containsKey(TChar* key) const {
    return detail::ObjectData::getMember(data_, detail::adaptString(key),
                                         resources_) != 0;
  }
  template <typename TVariant>
  ARDUINOJSON_DEPRECATED("use obj[key].is<T>() instead")
  detail::enable_if_t<detail::IsVariant<TVariant>::value, bool> containsKey(
      const TVariant& key) const {
    return containsKey(key.template as<const char*>());
  }
  template <typename TString>
  detail::enable_if_t<detail::IsString<TString>::value, JsonVariantConst>
  operator[](const TString& key) const {
    return JsonVariantConst(detail::ObjectData::getMember(
                                data_, detail::adaptString(key), resources_),
                            resources_);
  }
  template <typename TChar>
  detail::enable_if_t<detail::IsString<TChar*>::value, JsonVariantConst>
  operator[](TChar* key) const {
    return JsonVariantConst(detail::ObjectData::getMember(
                                data_, detail::adaptString(key), resources_),
                            resources_);
  }
  template <typename TVariant>
  detail::enable_if_t<detail::IsVariant<TVariant>::value, JsonVariantConst>
  operator[](const TVariant& key) const {
    if (key.template is<const char*>())
      return operator[](key.template as<const char*>());
    else
      return JsonVariantConst();
  }
  ARDUINOJSON_DEPRECATED("always returns zero")
  size_t memoryUsage() const {
    return 0;
  }
 private:
  const detail::VariantData* getData() const {
    return collectionToVariant(data_);
  }
  const detail::ObjectData* data_;
  const detail::ResourceManager* resources_;
};
inline bool operator==(JsonObjectConst lhs, JsonObjectConst rhs) {
  if (!lhs && !rhs)  // both are null
    return true;
  if (!lhs || !rhs)  // only one is null
    return false;
  size_t count = 0;
  for (auto kvp : lhs) {
    auto rhsValue = rhs[kvp.key()];
    if (rhsValue.isUnbound())
      return false;
    if (kvp.value() != rhsValue)
      return false;
    count++;
  }
  return count == rhs.size();
}
ARDUINOJSON_END_PUBLIC_NAMESPACE
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename TUpstream, typename TStringRef>
class MemberProxy
    : public VariantRefBase<MemberProxy<TUpstream, TStringRef>>,
      public VariantOperators<MemberProxy<TUpstream, TStringRef>> {
  friend class VariantAttorney;
 public:
  MemberProxy(TUpstream upstream, TStringRef key)
      : upstream_(upstream), key_(key) {}
  MemberProxy(const MemberProxy& src)
      : upstream_(src.upstream_), key_(src.key_) {}
  MemberProxy& operator=(const MemberProxy& src) {
    this->set(src);
    return *this;
  }
  template <typename T>
  MemberProxy& operator=(const T& src) {
    this->set(src);
    return *this;
  }
  template <typename T>
  MemberProxy& operator=(T* src) {
    this->set(src);
    return *this;
  }
 private:
  ResourceManager* getResourceManager() const {
    return VariantAttorney::getResourceManager(upstream_);
  }
  VariantData* getData() const {
    return VariantData::getMember(
        VariantAttorney::getData(upstream_), adaptString(key_),
        VariantAttorney::getResourceManager(upstream_));
  }
  VariantData* getOrCreateData() const {
    auto data = VariantAttorney::getOrCreateData(upstream_);
    if (!data)
      return nullptr;
    return data->getOrAddMember(adaptString(key_),
                                VariantAttorney::getResourceManager(upstream_));
  }
 private:
  TUpstream upstream_;
  TStringRef key_;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
class JsonArray;
class JsonObject : public detail::VariantOperators<JsonObject> {
  friend class detail::VariantAttorney;
 public:
  typedef JsonObjectIterator iterator;
  JsonObject() : data_(0), resources_(0) {}
  JsonObject(detail::ObjectData* data, detail::ResourceManager* resource)
      : data_(data), resources_(resource) {}
  operator JsonVariant() const {
    void* data = data_;  // prevent warning cast-align
    return JsonVariant(reinterpret_cast<detail::VariantData*>(data),
                       resources_);
  }
  operator JsonObjectConst() const {
    return JsonObjectConst(data_, resources_);
  }
  operator JsonVariantConst() const {
    return JsonVariantConst(collectionToVariant(data_), resources_);
  }
  bool isNull() const {
    return data_ == 0;
  }
  operator bool() const {
    return data_ != 0;
  }
  size_t nesting() const {
    return detail::VariantData::nesting(collectionToVariant(data_), resources_);
  }
  size_t size() const {
    return data_ ? data_->size(resources_) : 0;
  }
  iterator begin() const {
    if (!data_)
      return iterator();
    return iterator(data_->createIterator(resources_), resources_);
  }
  iterator end() const {
    return iterator();
  }
  void clear() const {
    detail::ObjectData::clear(data_, resources_);
  }
  bool set(JsonObjectConst src) {
    if (!data_ || !src.data_)
      return false;
    clear();
    for (auto kvp : src) {
      if (!operator[](kvp.key()).set(kvp.value()))
        return false;
    }
    return true;
  }
  template <typename TString>
  detail::enable_if_t<detail::IsString<TString>::value,
                      detail::MemberProxy<JsonObject, TString>>
  operator[](const TString& key) const {
    return {*this, key};
  }
  template <typename TChar>
  detail::enable_if_t<detail::IsString<TChar*>::value,
                      detail::MemberProxy<JsonObject, TChar*>>
  operator[](TChar* key) const {
    return {*this, key};
  }
  template <typename TVariant>
  detail::enable_if_t<detail::IsVariant<TVariant>::value,
                      detail::MemberProxy<JsonObject, const char*>>
  operator[](const TVariant& key) const {
    if (key.template is<const char*>())
      return {*this, key.template as<const char*>()};
    else
      return {*this, nullptr};
  }
  FORCE_INLINE void remove(iterator it) const {
    detail::ObjectData::remove(data_, it.iterator_, resources_);
  }
  template <typename TString>
  detail::enable_if_t<detail::IsString<TString>::value> remove(
      const TString& key) const {
    detail::ObjectData::removeMember(data_, detail::adaptString(key),
                                     resources_);
  }
  template <typename TVariant>
  detail::enable_if_t<detail::IsVariant<TVariant>::value> remove(
      const TVariant& key) const {
    if (key.template is<const char*>())
      remove(key.template as<const char*>());
  }
  template <typename TChar>
  FORCE_INLINE void remove(TChar* key) const {
    detail::ObjectData::removeMember(data_, detail::adaptString(key),
                                     resources_);
  }
  template <typename TString>
  ARDUINOJSON_DEPRECATED("use obj[key].is<T>() instead")
  detail::enable_if_t<detail::IsString<TString>::value, bool> containsKey(
      const TString& key) const {
    return detail::ObjectData::getMember(data_, detail::adaptString(key),
                                         resources_) != 0;
  }
  template <typename TChar>
  ARDUINOJSON_DEPRECATED("use obj[\"key\"].is<T>() instead")
  detail::enable_if_t<detail::IsString<TChar*>::value, bool> containsKey(
      TChar* key) const {
    return detail::ObjectData::getMember(data_, detail::adaptString(key),
                                         resources_) != 0;
  }
  template <typename TVariant>
  ARDUINOJSON_DEPRECATED("use obj[key].is<T>() instead")
  detail::enable_if_t<detail::IsVariant<TVariant>::value, bool> containsKey(
      const TVariant& key) const {
    return containsKey(key.template as<const char*>());
  }
  template <typename TChar>
  ARDUINOJSON_DEPRECATED("use obj[key].to<JsonArray>() instead")
  JsonArray createNestedArray(TChar* key) const {
    return operator[](key).template to<JsonArray>();
  }
  template <typename TString>
  ARDUINOJSON_DEPRECATED("use obj[key].to<JsonArray>() instead")
  JsonArray createNestedArray(const TString& key) const {
    return operator[](key).template to<JsonArray>();
  }
  template <typename TChar>
  ARDUINOJSON_DEPRECATED("use obj[key].to<JsonObject>() instead")
  JsonObject createNestedObject(TChar* key) {
    return operator[](key).template to<JsonObject>();
  }
  template <typename TString>
  ARDUINOJSON_DEPRECATED("use obj[key].to<JsonObject>() instead")
  JsonObject createNestedObject(const TString& key) {
    return operator[](key).template to<JsonObject>();
  }
  ARDUINOJSON_DEPRECATED("always returns zero")
  size_t memoryUsage() const {
    return 0;
  }
 private:
  detail::ResourceManager* getResourceManager() const {
    return resources_;
  }
  detail::VariantData* getData() const {
    return detail::collectionToVariant(data_);
  }
  detail::VariantData* getOrCreateData() const {
    return detail::collectionToVariant(data_);
  }
  detail::ObjectData* data_;
  detail::ResourceManager* resources_;
};
class JsonDocument : public detail::VariantOperators<const JsonDocument&> {
  friend class detail::VariantAttorney;
 public:
  explicit JsonDocument(Allocator* alloc = detail::DefaultAllocator::instance())
      : resources_(alloc) {}
  JsonDocument(const JsonDocument& src) : JsonDocument(src.allocator()) {
    set(src);
  }
  JsonDocument(JsonDocument&& src)
      : JsonDocument(detail::DefaultAllocator::instance()) {
    swap(*this, src);
  }
  template <typename T>
  JsonDocument(
      const T& src, Allocator* alloc = detail::DefaultAllocator::instance(),
      detail::enable_if_t<detail::IsVariant<T>::value ||
                          detail::is_same<T, JsonArray>::value ||
                          detail::is_same<T, JsonArrayConst>::value ||
                          detail::is_same<T, JsonObject>::value ||
                          detail::is_same<T, JsonObjectConst>::value>* = 0)
      : JsonDocument(alloc) {
    set(src);
  }
  JsonDocument& operator=(JsonDocument src) {
    swap(*this, src);
    return *this;
  }
  template <typename T>
  JsonDocument& operator=(const T& src) {
    set(src);
    return *this;
  }
  Allocator* allocator() const {
    return resources_.allocator();
  }
  void shrinkToFit() {
    resources_.shrinkToFit();
  }
  template <typename T>
  T as() {
    return getVariant().template as<T>();
  }
  template <typename T>
  T as() const {
    return getVariant().template as<T>();
  }
  void clear() {
    resources_.clear();
    data_.reset();
  }
  template <typename T>
  bool is() {
    return getVariant().template is<T>();
  }
  template <typename T>
  bool is() const {
    return getVariant().template is<T>();
  }
  bool isNull() const {
    return getVariant().isNull();
  }
  bool overflowed() const {
    return resources_.overflowed();
  }
  size_t nesting() const {
    return data_.nesting(&resources_);
  }
  size_t size() const {
    return data_.size(&resources_);
  }
  bool set(const JsonDocument& src) {
    return to<JsonVariant>().set(src.as<JsonVariantConst>());
  }
  template <typename T>
  detail::enable_if_t<!detail::is_base_of<JsonDocument, T>::value, bool> set(
      const T& src) {
    return to<JsonVariant>().set(src);
  }
  template <typename T>
  typename detail::VariantTo<T>::type to() {
    clear();
    return getVariant().template to<T>();
  }
  template <typename TChar>
  ARDUINOJSON_DEPRECATED("use doc[\"key\"].is<T>() instead")
  bool containsKey(TChar* key) const {
    return data_.getMember(detail::adaptString(key), &resources_) != 0;
  }
  template <typename TString>
  ARDUINOJSON_DEPRECATED("use doc[key].is<T>() instead")
  detail::enable_if_t<detail::IsString<TString>::value, bool> containsKey(
      const TString& key) const {
    return data_.getMember(detail::adaptString(key), &resources_) != 0;
  }
  template <typename TVariant>
  ARDUINOJSON_DEPRECATED("use doc[key].is<T>() instead")
  detail::enable_if_t<detail::IsVariant<TVariant>::value, bool> containsKey(
      const TVariant& key) const {
    return containsKey(key.template as<const char*>());
  }
  template <typename TString>
  detail::enable_if_t<detail::IsString<TString>::value,
                      detail::MemberProxy<JsonDocument&, TString>>
  operator[](const TString& key) {
    return {*this, key};
  }
  template <typename TChar>
  detail::enable_if_t<detail::IsString<TChar*>::value,
                      detail::MemberProxy<JsonDocument&, TChar*>>
  operator[](TChar* key) {
    return {*this, key};
  }
  template <typename TString>
  detail::enable_if_t<detail::IsString<TString>::value, JsonVariantConst>
  operator[](const TString& key) const {
    return JsonVariantConst(
        data_.getMember(detail::adaptString(key), &resources_), &resources_);
  }
  template <typename TChar>
  detail::enable_if_t<detail::IsString<TChar*>::value, JsonVariantConst>
  operator[](TChar* key) const {
    return JsonVariantConst(
        data_.getMember(detail::adaptString(key), &resources_), &resources_);
  }
  template <typename T>
  detail::enable_if_t<detail::is_integral<T>::value,
                      detail::ElementProxy<JsonDocument&>>
  operator[](T index) {
    return {*this, size_t(index)};
  }
  JsonVariantConst operator[](size_t index) const {
    return JsonVariantConst(data_.getElement(index, &resources_), &resources_);
  }
  template <typename TVariant>
  detail::enable_if_t<detail::IsVariant<TVariant>::value, JsonVariantConst>
  operator[](const TVariant& key) const {
    if (key.template is<const char*>())
      return operator[](key.template as<const char*>());
    if (key.template is<size_t>())
      return operator[](key.template as<size_t>());
    return {};
  }
  template <typename T>
  detail::enable_if_t<!detail::is_same<T, JsonVariant>::value, T> add() {
    return add<JsonVariant>().to<T>();
  }
  template <typename T>
  detail::enable_if_t<detail::is_same<T, JsonVariant>::value, T> add() {
    return JsonVariant(data_.addElement(&resources_), &resources_);
  }
  template <typename TValue>
  bool add(const TValue& value) {
    return data_.addValue(value, &resources_);
  }
  template <typename TChar>
  bool add(TChar* value) {
    return data_.addValue(value, &resources_);
  }
  template <typename T>
  detail::enable_if_t<detail::is_integral<T>::value> remove(T index) {
    detail::VariantData::removeElement(getData(), size_t(index),
                                       getResourceManager());
  }
  template <typename TChar>
  detail::enable_if_t<detail::IsString<TChar*>::value> remove(TChar* key) {
    detail::VariantData::removeMember(getData(), detail::adaptString(key),
                                      getResourceManager());
  }
  template <typename TString>
  detail::enable_if_t<detail::IsString<TString>::value> remove(
      const TString& key) {
    detail::VariantData::removeMember(getData(), detail::adaptString(key),
                                      getResourceManager());
  }
  template <typename TVariant>
  detail::enable_if_t<detail::IsVariant<TVariant>::value> remove(
      const TVariant& key) {
    if (key.template is<const char*>())
      remove(key.template as<const char*>());
    if (key.template is<size_t>())
      remove(key.template as<size_t>());
  }
  operator JsonVariant() {
    return getVariant();
  }
  operator JsonVariantConst() const {
    return getVariant();
  }
  friend void swap(JsonDocument& a, JsonDocument& b) {
    swap(a.resources_, b.resources_);
    swap_(a.data_, b.data_);
  }
  ARDUINOJSON_DEPRECATED("use add<JsonVariant>() instead")
  JsonVariant add() {
    return add<JsonVariant>();
  }
  ARDUINOJSON_DEPRECATED("use add<JsonArray>() instead")
  JsonArray createNestedArray() {
    return add<JsonArray>();
  }
  template <typename TChar>
  ARDUINOJSON_DEPRECATED("use doc[key].to<JsonArray>() instead")
  JsonArray createNestedArray(TChar* key) {
    return operator[](key).template to<JsonArray>();
  }
  template <typename TString>
  ARDUINOJSON_DEPRECATED("use doc[key].to<JsonArray>() instead")
  JsonArray createNestedArray(const TString& key) {
    return operator[](key).template to<JsonArray>();
  }
  ARDUINOJSON_DEPRECATED("use add<JsonObject>() instead")
  JsonObject createNestedObject() {
    return add<JsonObject>();
  }
  template <typename TChar>
  ARDUINOJSON_DEPRECATED("use doc[key].to<JsonObject>() instead")
  JsonObject createNestedObject(TChar* key) {
    return operator[](key).template to<JsonObject>();
  }
  template <typename TString>
  ARDUINOJSON_DEPRECATED("use doc[key].to<JsonObject>() instead")
  JsonObject createNestedObject(const TString& key) {
    return operator[](key).template to<JsonObject>();
  }
  ARDUINOJSON_DEPRECATED("always returns zero")
  size_t memoryUsage() const {
    return 0;
  }
 private:
  JsonVariant getVariant() {
    return JsonVariant(&data_, &resources_);
  }
  JsonVariantConst getVariant() const {
    return JsonVariantConst(&data_, &resources_);
  }
  detail::ResourceManager* getResourceManager() {
    return &resources_;
  }
  detail::VariantData* getData() {
    return &data_;
  }
  const detail::VariantData* getData() const {
    return &data_;
  }
  detail::VariantData* getOrCreateData() {
    return &data_;
  }
  detail::ResourceManager resources_;
  detail::VariantData data_;
};
inline void convertToJson(const JsonDocument& src, JsonVariant dst) {
  dst.set(src.as<JsonVariantConst>());
}
ARDUINOJSON_END_PUBLIC_NAMESPACE
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename TResult>
struct VariantDataVisitor {
  typedef TResult result_type;
  template <typename T>
  TResult visit(const T&) {
    return TResult();
  }
};
template <typename TResult>
struct JsonVariantVisitor {
  typedef TResult result_type;
  template <typename T>
  TResult visit(const T&) {
    return TResult();
  }
};
template <typename TVisitor>
class VisitorAdapter {
 public:
  using result_type = typename TVisitor::result_type;
  VisitorAdapter(TVisitor& visitor, const ResourceManager* resources)
      : visitor_(&visitor), resources_(resources) {}
  result_type visit(const ArrayData& value) {
    return visitor_->visit(JsonArrayConst(&value, resources_));
  }
  result_type visit(const ObjectData& value) {
    return visitor_->visit(JsonObjectConst(&value, resources_));
  }
  template <typename T>
  result_type visit(const T& value) {
    return visitor_->visit(value);
  }
 private:
  TVisitor* visitor_;
  const ResourceManager* resources_;
};
template <typename TVisitor>
typename TVisitor::result_type accept(JsonVariantConst variant,
                                      TVisitor& visit) {
  auto data = VariantAttorney::getData(variant);
  if (!data)
    return visit.visit(nullptr);
  auto resources = VariantAttorney::getResourceManager(variant);
  VisitorAdapter<TVisitor> adapter(visit, resources);
  return data->accept(adapter, resources);
}
struct ComparerBase : JsonVariantVisitor<CompareResult> {};
template <typename T, typename Enable = void>
struct Comparer;
template <typename T>
struct Comparer<T, enable_if_t<IsString<T>::value>> : ComparerBase {
  T rhs;  // TODO: store adapted string?
  explicit Comparer(T value) : rhs(value) {}
  CompareResult visit(JsonString lhs) {
    int i = stringCompare(adaptString(rhs), adaptString(lhs));
    if (i < 0)
      return COMPARE_RESULT_GREATER;
    else if (i > 0)
      return COMPARE_RESULT_LESS;
    else
      return COMPARE_RESULT_EQUAL;
  }
  CompareResult visit(nullptr_t) {
    if (adaptString(rhs).isNull())
      return COMPARE_RESULT_EQUAL;
    else
      return COMPARE_RESULT_DIFFER;
  }
  using ComparerBase::visit;
};
template <typename T>
struct Comparer<
    T, enable_if_t<is_integral<T>::value || is_floating_point<T>::value>>
    : ComparerBase {
  T rhs;
  explicit Comparer(T value) : rhs(value) {}
  template <typename U>
  enable_if_t<is_floating_point<U>::value || is_integral<U>::value,
              CompareResult>
  visit(const U& lhs) {
    return arithmeticCompare(lhs, rhs);
  }
  template <typename U>
  enable_if_t<!is_floating_point<U>::value && !is_integral<U>::value,
              CompareResult>
  visit(const U& lhs) {
    return ComparerBase::visit(lhs);
  }
};
struct NullComparer : ComparerBase {
  CompareResult visit(nullptr_t) {
    return COMPARE_RESULT_EQUAL;
  }
  using ComparerBase::visit;
};
template <>
struct Comparer<nullptr_t, void> : NullComparer {
  explicit Comparer(nullptr_t) : NullComparer() {}
};
struct ArrayComparer : ComparerBase {
  JsonArrayConst rhs_;
  explicit ArrayComparer(JsonArrayConst rhs) : rhs_(rhs) {}
  CompareResult visit(JsonArrayConst lhs) {
    if (rhs_ == lhs)
      return COMPARE_RESULT_EQUAL;
    else
      return COMPARE_RESULT_DIFFER;
  }
  using ComparerBase::visit;
};
struct ObjectComparer : ComparerBase {
  JsonObjectConst rhs_;
  explicit ObjectComparer(JsonObjectConst rhs) : rhs_(rhs) {}
  CompareResult visit(JsonObjectConst lhs) {
    if (lhs == rhs_)
      return COMPARE_RESULT_EQUAL;
    else
      return COMPARE_RESULT_DIFFER;
  }
  using ComparerBase::visit;
};
struct RawComparer : ComparerBase {
  RawString rhs_;
  explicit RawComparer(RawString rhs) : rhs_(rhs) {}
  CompareResult visit(RawString lhs) {
    size_t size = rhs_.size() < lhs.size() ? rhs_.size() : lhs.size();
    int n = memcmp(lhs.data(), rhs_.data(), size);
    if (n < 0)
      return COMPARE_RESULT_LESS;
    else if (n > 0)
      return COMPARE_RESULT_GREATER;
    else
      return COMPARE_RESULT_EQUAL;
  }
  using ComparerBase::visit;
};
struct VariantComparer : ComparerBase {
  JsonVariantConst rhs;
  explicit VariantComparer(JsonVariantConst value) : rhs(value) {}
  CompareResult visit(JsonArrayConst lhs) {
    ArrayComparer comparer(lhs);
    return reverseResult(comparer);
  }
  CompareResult visit(JsonObjectConst lhs) {
    ObjectComparer comparer(lhs);
    return reverseResult(comparer);
  }
  CompareResult visit(JsonFloat lhs) {
    Comparer<JsonFloat> comparer(lhs);
    return reverseResult(comparer);
  }
  CompareResult visit(JsonString lhs) {
    Comparer<JsonString> comparer(lhs);
    return reverseResult(comparer);
  }
  CompareResult visit(RawString value) {
    RawComparer comparer(value);
    return reverseResult(comparer);
  }
  CompareResult visit(JsonInteger lhs) {
    Comparer<JsonInteger> comparer(lhs);
    return reverseResult(comparer);
  }
  CompareResult visit(JsonUInt lhs) {
    Comparer<JsonUInt> comparer(lhs);
    return reverseResult(comparer);
  }
  CompareResult visit(bool lhs) {
    Comparer<bool> comparer(lhs);
    return reverseResult(comparer);
  }
  CompareResult visit(nullptr_t) {
    NullComparer comparer;
    return reverseResult(comparer);
  }
 private:
  template <typename TComparer>
  CompareResult reverseResult(TComparer& comparer) {
    CompareResult reversedResult = accept(rhs, comparer);
    switch (reversedResult) {
      case COMPARE_RESULT_GREATER:
        return COMPARE_RESULT_LESS;
      case COMPARE_RESULT_LESS:
        return COMPARE_RESULT_GREATER;
      default:
        return reversedResult;
    }
  }
};
template <typename T>
struct Comparer<
    T, enable_if_t<is_convertible<T, FLArduinoJson::JsonVariantConst>::value>>
    : VariantComparer {
  explicit Comparer(const T& value)
      : VariantComparer(static_cast<JsonVariantConst>(value)) {}
};
template <typename T>
CompareResult compare(FLArduinoJson::JsonVariantConst lhs, const T& rhs) {
  Comparer<T> comparer(rhs);
  return accept(lhs, comparer);
}
inline ArrayData::iterator ArrayData::at(
    size_t index, const ResourceManager* resources) const {
  auto it = createIterator(resources);
  while (!it.done() && index) {
    it.next(resources);
    --index;
  }
  return it;
}
inline VariantData* ArrayData::addElement(ResourceManager* resources) {
  auto slot = resources->allocVariant();
  if (!slot)
    return nullptr;
  CollectionData::appendOne(slot, resources);
  return slot.ptr();
}
inline VariantData* ArrayData::getOrAddElement(size_t index,
                                               ResourceManager* resources) {
  auto it = createIterator(resources);
  while (!it.done() && index > 0) {
    it.next(resources);
    index--;
  }
  if (it.done())
    index++;
  VariantData* element = it.data();
  while (index > 0) {
    element = addElement(resources);
    if (!element)
      return nullptr;
    index--;
  }
  return element;
}
inline VariantData* ArrayData::getElement(
    size_t index, const ResourceManager* resources) const {
  return at(index, resources).data();
}
inline void ArrayData::removeElement(size_t index, ResourceManager* resources) {
  remove(at(index, resources), resources);
}
template <typename T>
inline bool ArrayData::addValue(T&& value, ResourceManager* resources) {
  ARDUINOJSON_ASSERT(resources != nullptr);
  auto slot = resources->allocVariant();
  if (!slot)
    return false;
  JsonVariant variant(slot.ptr(), resources);
  if (!variant.set(detail::forward<T>(value))) {
    resources->freeVariant(slot);
    return false;
  }
  CollectionData::appendOne(slot, resources);
  return true;
}
constexpr size_t sizeofArray(size_t n) {
  return n * ResourceManager::slotSize;
}
ARDUINOJSON_END_PRIVATE_NAMESPACE
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
template <typename T>
inline detail::enable_if_t<!detail::is_array<T>::value, bool> copyArray(
    const T& src, JsonVariant dst) {
  return dst.set(src);
}
template <typename T, size_t N, typename TDestination>
inline detail::enable_if_t<
    !detail::is_base_of<JsonDocument, TDestination>::value, bool>
copyArray(T (&src)[N], const TDestination& dst) {
  return copyArray(src, N, dst);
}
template <typename T, typename TDestination>
inline detail::enable_if_t<
    !detail::is_base_of<JsonDocument, TDestination>::value, bool>
copyArray(const T* src, size_t len, const TDestination& dst) {
  bool ok = true;
  for (size_t i = 0; i < len; i++) {
    ok &= copyArray(src[i], dst.template add<JsonVariant>());
  }
  return ok;
}
template <typename TDestination>
inline bool copyArray(const char* src, size_t, const TDestination& dst) {
  return dst.set(src);
}
template <typename T>
inline bool copyArray(const T& src, JsonDocument& dst) {
  return copyArray(src, dst.to<JsonArray>());
}
template <typename T>
inline bool copyArray(const T* src, size_t len, JsonDocument& dst) {
  return copyArray(src, len, dst.to<JsonArray>());
}
template <typename T>
inline detail::enable_if_t<!detail::is_array<T>::value, size_t> copyArray(
    JsonVariantConst src, T& dst) {
  dst = src.as<T>();
  return 1;
}
template <typename T, size_t N>
inline size_t copyArray(JsonArrayConst src, T (&dst)[N]) {
  return copyArray(src, dst, N);
}
template <typename T>
inline size_t copyArray(JsonArrayConst src, T* dst, size_t len) {
  size_t i = 0;
  for (JsonArrayConst::iterator it = src.begin(); it != src.end() && i < len;
       ++it)
    copyArray(*it, dst[i++]);
  return i;
}
template <size_t N>
inline size_t copyArray(JsonVariantConst src, char (&dst)[N]) {
  JsonString s = src;
  size_t len = N - 1;
  if (len > s.size())
    len = s.size();
  memcpy(dst, s.c_str(), len);
  dst[len] = 0;
  return 1;
}
template <typename TSource, typename T>
inline detail::enable_if_t<detail::is_array<T>::value &&
                               detail::is_base_of<JsonDocument, TSource>::value,
                           size_t>
copyArray(const TSource& src, T& dst) {
  return copyArray(src.template as<JsonArrayConst>(), dst);
}
ARDUINOJSON_END_PUBLIC_NAMESPACE
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
#if ARDUINOJSON_ENABLE_ALIGNMENT
inline bool isAligned(size_t value) {
  const size_t mask = sizeof(void*) - 1;
  size_t addr = value;
  return (addr & mask) == 0;
}
inline size_t addPadding(size_t bytes) {
  const size_t mask = sizeof(void*) - 1;
  return (bytes + mask) & ~mask;
}
template <size_t bytes>
struct AddPadding {
  static const size_t mask = sizeof(void*) - 1;
  static const size_t value = (bytes + mask) & ~mask;
};
#else
inline bool isAligned(size_t) {
  return true;
}
inline size_t addPadding(size_t bytes) {
  return bytes;
}
template <size_t bytes>
struct AddPadding {
  static const size_t value = bytes;
};
#endif
template <typename T>
inline bool isAligned(T* ptr) {
  return isAligned(reinterpret_cast<size_t>(ptr));
}
template <typename T>
inline T* addPadding(T* p) {
  size_t address = addPadding(reinterpret_cast<size_t>(p));
  return reinterpret_cast<T*>(address);
}
inline CollectionIterator::CollectionIterator(VariantData* slot, SlotId slotId)
    : slot_(slot), currentId_(slotId) {
  nextId_ = slot_ ? slot_->next() : NULL_SLOT;
}
inline void CollectionIterator::next(const ResourceManager* resources) {
  ARDUINOJSON_ASSERT(currentId_ != NULL_SLOT);
  slot_ = resources->getVariant(nextId_);
  currentId_ = nextId_;
  if (slot_)
    nextId_ = slot_->next();
}
inline CollectionData::iterator CollectionData::createIterator(
    const ResourceManager* resources) const {
  return iterator(resources->getVariant(head_), head_);
}
inline void CollectionData::appendOne(Slot<VariantData> slot,
                                      const ResourceManager* resources) {
  if (tail_ != NULL_SLOT) {
    auto tail = resources->getVariant(tail_);
    tail->setNext(slot.id());
    tail_ = slot.id();
  } else {
    head_ = slot.id();
    tail_ = slot.id();
  }
}
inline void CollectionData::appendPair(Slot<VariantData> key,
                                       Slot<VariantData> value,
                                       const ResourceManager* resources) {
  key->setNext(value.id());
  if (tail_ != NULL_SLOT) {
    auto tail = resources->getVariant(tail_);
    tail->setNext(key.id());
    tail_ = value.id();
  } else {
    head_ = key.id();
    tail_ = value.id();
  }
}
inline void CollectionData::clear(ResourceManager* resources) {
  auto next = head_;
  while (next != NULL_SLOT) {
    auto currId = next;
    auto slot = resources->getVariant(next);
    next = slot->next();
    resources->freeVariant({slot, currId});
  }
  head_ = NULL_SLOT;
  tail_ = NULL_SLOT;
}
inline Slot<VariantData> CollectionData::getPreviousSlot(
    VariantData* target, const ResourceManager* resources) const {
  auto prev = Slot<VariantData>();
  auto currentId = head_;
  while (currentId != NULL_SLOT) {
    auto currentSlot = resources->getVariant(currentId);
    if (currentSlot == target)
      break;
    prev = Slot<VariantData>(currentSlot, currentId);
    currentId = currentSlot->next();
  }
  return prev;
}
inline void CollectionData::removeOne(iterator it, ResourceManager* resources) {
  if (it.done())
    return;
  auto curr = it.slot_;
  auto prev = getPreviousSlot(curr, resources);
  auto next = curr->next();
  if (prev)
    prev->setNext(next);
  else
    head_ = next;
  if (next == NULL_SLOT)
    tail_ = prev.id();
  resources->freeVariant({it.slot_, it.currentId_});
}
inline void CollectionData::removePair(ObjectData::iterator it,
                                       ResourceManager* resources) {
  if (it.done())
    return;
  auto keySlot = it.slot_;
  auto valueId = it.nextId_;
  auto valueSlot = resources->getVariant(valueId);
  keySlot->setNext(valueSlot->next());
  resources->freeVariant({valueSlot, valueId});
  removeOne(it, resources);
}
inline size_t CollectionData::nesting(const ResourceManager* resources) const {
  size_t maxChildNesting = 0;
  for (auto it = createIterator(resources); !it.done(); it.next(resources)) {
    size_t childNesting = it->nesting(resources);
    if (childNesting > maxChildNesting)
      maxChildNesting = childNesting;
  }
  return maxChildNesting + 1;
}
inline size_t CollectionData::size(const ResourceManager* resources) const {
  size_t count = 0;
  for (auto it = createIterator(resources); !it.done(); it.next(resources))
    count++;
  return count;
}
inline Slot<VariantData> ResourceManager::allocVariant() {
  auto p = variantPools_.allocSlot(allocator_);
  if (!p) {
    overflowed_ = true;
    return {};
  }
  return {new (&p->variant) VariantData, p.id()};
}
inline void ResourceManager::freeVariant(Slot<VariantData> variant) {
  variant->clear(this);
  variantPools_.freeSlot({alias_cast<SlotData*>(variant.ptr()), variant.id()});
}
inline VariantData* ResourceManager::getVariant(SlotId id) const {
  return reinterpret_cast<VariantData*>(variantPools_.getSlot(id));
}
#if ARDUINOJSON_USE_EXTENSIONS
inline Slot<VariantExtension> ResourceManager::allocExtension() {
  auto p = variantPools_.allocSlot(allocator_);
  if (!p) {
    overflowed_ = true;
    return {};
  }
  return {&p->extension, p.id()};
}
inline void ResourceManager::freeExtension(SlotId id) {
  auto p = getExtension(id);
  variantPools_.freeSlot({reinterpret_cast<SlotData*>(p), id});
}
inline VariantExtension* ResourceManager::getExtension(SlotId id) const {
  return &variantPools_.getSlot(id)->extension;
}
#endif
template <typename TAdaptedString>
inline VariantData* ObjectData::getMember(
    TAdaptedString key, const ResourceManager* resources) const {
  auto it = findKey(key, resources);
  if (it.done())
    return nullptr;
  it.next(resources);
  return it.data();
}
template <typename TAdaptedString>
VariantData* ObjectData::getOrAddMember(TAdaptedString key,
                                        ResourceManager* resources) {
  auto data = getMember(key, resources);
  if (data)
    return data;
  return addMember(key, resources);
}
template <typename TAdaptedString>
inline ObjectData::iterator ObjectData::findKey(
    TAdaptedString key, const ResourceManager* resources) const {
  if (key.isNull())
    return iterator();
  bool isKey = true;
  for (auto it = createIterator(resources); !it.done(); it.next(resources)) {
    if (isKey && stringEquals(key, adaptString(it->asString())))
      return it;
    isKey = !isKey;
  }
  return iterator();
}
template <typename TAdaptedString>
inline void ObjectData::removeMember(TAdaptedString key,
                                     ResourceManager* resources) {
  remove(findKey(key, resources), resources);
}
template <typename TAdaptedString>
inline VariantData* ObjectData::addMember(TAdaptedString key,
                                          ResourceManager* resources) {
  auto keySlot = resources->allocVariant();
  if (!keySlot)
    return nullptr;
  auto valueSlot = resources->allocVariant();
  if (!valueSlot)
    return nullptr;
  if (!keySlot->setString(key, resources))
    return nullptr;
  CollectionData::appendPair(keySlot, valueSlot, resources);
  return valueSlot.ptr();
}
constexpr size_t sizeofObject(size_t n) {
  return 2 * n * ResourceManager::slotSize;
}
class EscapeSequence {
 public:
  static char escapeChar(char c) {
    const char* p = escapeTable(true);
    while (p[0] && p[1] != c) {
      p += 2;
    }
    return p[0];
  }
  static char unescapeChar(char c) {
    const char* p = escapeTable(false);
    for (;;) {
      if (p[0] == '\0')
        return 0;
      if (p[0] == c)
        return p[1];
      p += 2;
    }
  }
 private:
  static const char* escapeTable(bool isSerializing) {
    return &"//''\"\"\\\\b\bf\fn\nr\rt\t"[isSerializing ? 4 : 0];
  }
};
struct FloatParts {
  uint32_t integral;
  uint32_t decimal;
  int16_t exponent;
  int8_t decimalPlaces;
};
template <typename TFloat>
inline int16_t normalize(TFloat& value) {
  typedef FloatTraits<TFloat> traits;
  int16_t powersOf10 = 0;
  int8_t index = sizeof(TFloat) == 8 ? 8 : 5;
  int bit = 1 << index;
  if (value >= ARDUINOJSON_POSITIVE_EXPONENTIATION_THRESHOLD) {
    for (; index >= 0; index--) {
      if (value >= traits::positiveBinaryPowersOfTen()[index]) {
        value *= traits::negativeBinaryPowersOfTen()[index];
        powersOf10 = int16_t(powersOf10 + bit);
      }
      bit >>= 1;
    }
  }
  if (value > 0 && value <= ARDUINOJSON_NEGATIVE_EXPONENTIATION_THRESHOLD) {
    for (; index >= 0; index--) {
      if (value < traits::negativeBinaryPowersOfTen()[index] * 10) {
        value *= traits::positiveBinaryPowersOfTen()[index];
        powersOf10 = int16_t(powersOf10 - bit);
      }
      bit >>= 1;
    }
  }
  return powersOf10;
}
constexpr uint32_t pow10(int exponent) {
  return (exponent == 0) ? 1 : 10 * pow10(exponent - 1);
}
inline FloatParts decomposeFloat(JsonFloat value, int8_t decimalPlaces) {
  uint32_t maxDecimalPart = pow10(decimalPlaces);
  int16_t exponent = normalize(value);
  uint32_t integral = uint32_t(value);
  for (uint32_t tmp = integral; tmp >= 10; tmp /= 10) {
    maxDecimalPart /= 10;
    decimalPlaces--;
  }
  JsonFloat remainder =
      (value - JsonFloat(integral)) * JsonFloat(maxDecimalPart);
  uint32_t decimal = uint32_t(remainder);
  remainder = remainder - JsonFloat(decimal);
  decimal += uint32_t(remainder * 2);
  if (decimal >= maxDecimalPart) {
    decimal = 0;
    integral++;
    if (exponent && integral >= 10) {
      exponent++;
      integral = 1;
    }
  }
  while (decimal % 10 == 0 && decimalPlaces > 0) {
    decimal /= 10;
    decimalPlaces--;
  }
  return {integral, decimal, exponent, decimalPlaces};
}
template <typename TWriter>
class CountingDecorator {
 public:
  explicit CountingDecorator(TWriter& writer) : writer_(writer), count_(0) {}
  void write(uint8_t c) {
    count_ += writer_.write(c);
  }
  void write(const uint8_t* s, size_t n) {
    count_ += writer_.write(s, n);
  }
  size_t count() const {
    return count_;
  }
 private:
  TWriter writer_;
  size_t count_;
};
template <typename TWriter>
class TextFormatter {
 public:
  explicit TextFormatter(TWriter writer) : writer_(writer) {}
  TextFormatter& operator=(const TextFormatter&) = delete;
  size_t bytesWritten() const {
    return writer_.count();
  }
  void writeBoolean(bool value) {
    if (value)
      writeRaw("true");
    else
      writeRaw("false");
  }
  void writeString(const char* value) {
    ARDUINOJSON_ASSERT(value != NULL);
    writeRaw('\"');
    while (*value)
      writeChar(*value++);
    writeRaw('\"');
  }
  void writeString(const char* value, size_t n) {
    ARDUINOJSON_ASSERT(value != NULL);
    writeRaw('\"');
    while (n--)
      writeChar(*value++);
    writeRaw('\"');
  }
  void writeChar(char c) {
    char specialChar = EscapeSequence::escapeChar(c);
    if (specialChar) {
      writeRaw('\\');
      writeRaw(specialChar);
    } else if (c) {
      writeRaw(c);
    } else {
      writeRaw("\\u0000");
    }
  }
  template <typename T>
  void writeFloat(T value) {
    writeFloat(JsonFloat(value), sizeof(T) >= 8 ? 9 : 6);
  }
  void writeFloat(JsonFloat value, int8_t decimalPlaces) {
    if (isnan(value))
      return writeRaw(ARDUINOJSON_ENABLE_NAN ? "NaN" : "null");
#if ARDUINOJSON_ENABLE_INFINITY
    if (value < 0.0) {
      writeRaw('-');
      value = -value;
    }
    if (isinf(value))
      return writeRaw("Infinity");
#else
    if (isinf(value))
      return writeRaw("null");
    if (value < 0.0) {
      writeRaw('-');
      value = -value;
    }
#endif
    auto parts = decomposeFloat(value, decimalPlaces);
    writeInteger(parts.integral);
    if (parts.decimalPlaces)
      writeDecimals(parts.decimal, parts.decimalPlaces);
    if (parts.exponent) {
      writeRaw('e');
      writeInteger(parts.exponent);
    }
  }
  template <typename T>
  enable_if_t<is_signed<T>::value> writeInteger(T value) {
    typedef make_unsigned_t<T> unsigned_type;
    unsigned_type unsigned_value;
    if (value < 0) {
      writeRaw('-');
      unsigned_value = unsigned_type(unsigned_type(~value) + 1);
    } else {
      unsigned_value = unsigned_type(value);
    }
    writeInteger(unsigned_value);
  }
  template <typename T>
  enable_if_t<is_unsigned<T>::value> writeInteger(T value) {
    char buffer[22];
    char* end = buffer + sizeof(buffer);
    char* begin = end;
    do {
      *--begin = char(value % 10 + '0');
      value = T(value / 10);
    } while (value);
    writeRaw(begin, end);
  }
  void writeDecimals(uint32_t value, int8_t width) {
    char buffer[16];
    char* end = buffer + sizeof(buffer);
    char* begin = end;
    while (width--) {
      *--begin = char(value % 10 + '0');
      value /= 10;
    }
    *--begin = '.';
    writeRaw(begin, end);
  }
  void writeRaw(const char* s) {
    writer_.write(reinterpret_cast<const uint8_t*>(s), strlen(s));
  }
  void writeRaw(const char* s, size_t n) {
    writer_.write(reinterpret_cast<const uint8_t*>(s), n);
  }
  void writeRaw(const char* begin, const char* end) {
    writer_.write(reinterpret_cast<const uint8_t*>(begin),
                  static_cast<size_t>(end - begin));
  }
  template <size_t N>
  void writeRaw(const char (&s)[N]) {
    writer_.write(reinterpret_cast<const uint8_t*>(s), N - 1);
  }
  void writeRaw(char c) {
    writer_.write(static_cast<uint8_t>(c));
  }
 protected:
  CountingDecorator<TWriter> writer_;
};
class DummyWriter {
 public:
  size_t write(uint8_t) {
    return 1;
  }
  size_t write(const uint8_t*, size_t n) {
    return n;
  }
};
template <template <typename> class TSerializer>
size_t measure(FLArduinoJson::JsonVariantConst source) {
  DummyWriter dp;
  auto data = VariantAttorney::getData(source);
  auto resources = VariantAttorney::getResourceManager(source);
  TSerializer<DummyWriter> serializer(dp, resources);
  return VariantData::accept(data, resources, serializer);
}
template <typename TDestination, typename Enable = void>
class Writer {
 public:
  explicit Writer(TDestination& dest) : dest_(&dest) {}
  size_t write(uint8_t c) {
    return dest_->write(c);
  }
  size_t write(const uint8_t* s, size_t n) {
    return dest_->write(s, n);
  }
 private:
  TDestination* dest_;
};
class StaticStringWriter {
 public:
  StaticStringWriter(char* buf, size_t size) : end(buf + size), p(buf) {}
  size_t write(uint8_t c) {
    if (p >= end)
      return 0;
    *p++ = static_cast<char>(c);
    return 1;
  }
  size_t write(const uint8_t* s, size_t n) {
    char* begin = p;
    while (p < end && n > 0) {
      *p++ = static_cast<char>(*s++);
      n--;
    }
    return size_t(p - begin);
  }
 private:
  char* end;
  char* p;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#if ARDUINOJSON_ENABLE_STD_STRING
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <class T, typename = void>
struct is_std_string : false_type {};
template <class T>
struct is_std_string<
    T, enable_if_t<is_same<void, decltype(T().push_back('a'))>::value &&
                   is_same<T&, decltype(T().append(""))>::value>> : true_type {
};
template <typename TDestination>
class Writer<TDestination, enable_if_t<is_std_string<TDestination>::value>> {
 public:
  Writer(TDestination& str) : str_(&str) {
    str.clear();
  }
  size_t write(uint8_t c) {
    str_->push_back(static_cast<char>(c));
    return 1;
  }
  size_t write(const uint8_t* s, size_t n) {
    str_->append(reinterpret_cast<const char*>(s), n);
    return n;
  }
 private:
  TDestination* str_;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#endif
#if ARDUINOJSON_ENABLE_ARDUINO_STRING
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <>
class Writer<::String, void> {
  static const size_t bufferCapacity = ARDUINOJSON_STRING_BUFFER_SIZE;
 public:
  explicit Writer(::String& str) : destination_(&str), size_(0) {
    str = static_cast<const char*>(0);
  }
  ~Writer() {
    flush();
  }
  size_t write(uint8_t c) {
    if (size_ + 1 >= bufferCapacity)
      if (flush() != 0)
        return 0;
    buffer_[size_++] = static_cast<char>(c);
    return 1;
  }
  size_t write(const uint8_t* s, size_t n) {
    for (size_t i = 0; i < n; i++) {
      write(s[i]);
    }
    return n;
  }
  size_t flush() {
    ARDUINOJSON_ASSERT(size_ < bufferCapacity);
    buffer_[size_] = 0;
    if (destination_->concat(buffer_))
      size_ = 0;
    return size_;
  }
 private:
  ::String* destination_;
  char buffer_[bufferCapacity];
  size_t size_;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#endif
#if ARDUINOJSON_ENABLE_STD_STREAM
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename TDestination>
class Writer<TDestination,
             enable_if_t<is_base_of<std::ostream, TDestination>::value>> {
 public:
  explicit Writer(std::ostream& os) : os_(&os) {}
  size_t write(uint8_t c) {
    os_->put(static_cast<char>(c));
    return 1;
  }
  size_t write(const uint8_t* s, size_t n) {
    os_->write(reinterpret_cast<const char*>(s),
               static_cast<std::streamsize>(n));
    return n;
  }
 private:
  std::ostream* os_;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#endif
#if ARDUINOJSON_ENABLE_ARDUINO_PRINT
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename TDestination>
class Writer<TDestination,
             enable_if_t<is_base_of<::Print, TDestination>::value>> {
 public:
  explicit Writer(::Print& print) : print_(&print) {}
  size_t write(uint8_t c) {
    return print_->write(c);
  }
  size_t write(const uint8_t* s, size_t n) {
    return print_->write(s, n);
  }
 private:
  ::Print* print_;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <template <typename> class TSerializer, typename TWriter>
size_t doSerialize(FLArduinoJson::JsonVariantConst source, TWriter writer) {
  auto data = VariantAttorney::getData(source);
  auto resources = VariantAttorney::getResourceManager(source);
  TSerializer<TWriter> serializer(writer, resources);
  return VariantData::accept(data, resources, serializer);
}
template <template <typename> class TSerializer, typename TDestination>
size_t serialize(FLArduinoJson::JsonVariantConst source,
                 TDestination& destination) {
  Writer<TDestination> writer(destination);
  return doSerialize<TSerializer>(source, writer);
}
template <template <typename> class TSerializer>
enable_if_t<!TSerializer<StaticStringWriter>::producesText, size_t> serialize(
    FLArduinoJson::JsonVariantConst source, void* buffer, size_t bufferSize) {
  StaticStringWriter writer(reinterpret_cast<char*>(buffer), bufferSize);
  return doSerialize<TSerializer>(source, writer);
}
template <template <typename> class TSerializer>
enable_if_t<TSerializer<StaticStringWriter>::producesText, size_t> serialize(
    FLArduinoJson::JsonVariantConst source, void* buffer, size_t bufferSize) {
  StaticStringWriter writer(reinterpret_cast<char*>(buffer), bufferSize);
  size_t n = doSerialize<TSerializer>(source, writer);
  if (n < bufferSize)
    reinterpret_cast<char*>(buffer)[n] = 0;
  return n;
}
template <template <typename> class TSerializer, typename TChar, size_t N>
enable_if_t<IsChar<TChar>::value, size_t> serialize(
    FLArduinoJson::JsonVariantConst source, TChar (&buffer)[N]) {
  return serialize<TSerializer>(source, buffer, N);
}
template <typename TWriter>
class JsonSerializer : public VariantDataVisitor<size_t> {
 public:
  static const bool producesText = true;
  JsonSerializer(TWriter writer, const ResourceManager* resources)
      : formatter_(writer), resources_(resources) {}
  size_t visit(const ArrayData& array) {
    write('[');
    auto slotId = array.head();
    while (slotId != NULL_SLOT) {
      auto slot = resources_->getVariant(slotId);
      slot->accept(*this, resources_);
      slotId = slot->next();
      if (slotId != NULL_SLOT)
        write(',');
    }
    write(']');
    return bytesWritten();
  }
  size_t visit(const ObjectData& object) {
    write('{');
    auto slotId = object.head();
    bool isKey = true;
    while (slotId != NULL_SLOT) {
      auto slot = resources_->getVariant(slotId);
      slot->accept(*this, resources_);
      slotId = slot->next();
      if (slotId != NULL_SLOT)
        write(isKey ? ':' : ',');
      isKey = !isKey;
    }
    write('}');
    return bytesWritten();
  }
  template <typename T>
  enable_if_t<is_floating_point<T>::value, size_t> visit(T value) {
    formatter_.writeFloat(value);
    return bytesWritten();
  }
  size_t visit(const char* value) {
    formatter_.writeString(value);
    return bytesWritten();
  }
  size_t visit(JsonString value) {
    formatter_.writeString(value.c_str(), value.size());
    return bytesWritten();
  }
  size_t visit(RawString value) {
    formatter_.writeRaw(value.data(), value.size());
    return bytesWritten();
  }
  size_t visit(JsonInteger value) {
    formatter_.writeInteger(value);
    return bytesWritten();
  }
  size_t visit(JsonUInt value) {
    formatter_.writeInteger(value);
    return bytesWritten();
  }
  size_t visit(bool value) {
    formatter_.writeBoolean(value);
    return bytesWritten();
  }
  size_t visit(nullptr_t) {
    formatter_.writeRaw("null");
    return bytesWritten();
  }
 protected:
  size_t bytesWritten() const {
    return formatter_.bytesWritten();
  }
  void write(char c) {
    formatter_.writeRaw(c);
  }
  void write(const char* s) {
    formatter_.writeRaw(s);
  }
 private:
  TextFormatter<TWriter> formatter_;
 protected:
  const ResourceManager* resources_;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
template <typename TDestination>
detail::enable_if_t<!detail::is_pointer<TDestination>::value, size_t>
serializeJson(JsonVariantConst source, TDestination& destination) {
  using namespace detail;
  return serialize<JsonSerializer>(source, destination);
}
inline size_t serializeJson(JsonVariantConst source, void* buffer,
                            size_t bufferSize) {
  using namespace detail;
  return serialize<JsonSerializer>(source, buffer, bufferSize);
}
inline size_t measureJson(JsonVariantConst source) {
  using namespace detail;
  return measure<JsonSerializer>(source);
}
#if ARDUINOJSON_ENABLE_STD_STREAM
template <typename T>
inline detail::enable_if_t<detail::is_convertible<T, JsonVariantConst>::value,
                           std::ostream&>
operator<<(std::ostream& os, const T& source) {
  serializeJson(source, os);
  return os;
}
#endif
ARDUINOJSON_END_PUBLIC_NAMESPACE
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
class StringBuilder {
 public:
  static const size_t initialCapacity = 31;
  StringBuilder(ResourceManager* resources) : resources_(resources) {}
  ~StringBuilder() {
    if (node_)
      resources_->destroyString(node_);
  }
  void startString() {
    size_ = 0;
    if (!node_)
      node_ = resources_->createString(initialCapacity);
  }
  StringNode* save() {
    ARDUINOJSON_ASSERT(node_ != nullptr);
    node_->data[size_] = 0;
    StringNode* node = resources_->getString(adaptString(node_->data, size_));
    if (!node) {
      node = resources_->resizeString(node_, size_);
      ARDUINOJSON_ASSERT(node != nullptr);  // realloc to smaller can't fail
      resources_->saveString(node);
      node_ = nullptr;  // next time we need a new string
    } else {
      node->references++;
    }
    return node;
  }
  void append(const char* s) {
    while (*s)
      append(*s++);
  }
  void append(const char* s, size_t n) {
    while (n-- > 0)  // TODO: memcpy
      append(*s++);
  }
  void append(char c) {
    if (node_ && size_ == node_->length)
      node_ = resources_->resizeString(node_, size_ * 2U + 1);
    if (node_)
      node_->data[size_++] = c;
  }
  bool isValid() const {
    return node_ != nullptr;
  }
  size_t size() const {
    return size_;
  }
  JsonString str() const {
    ARDUINOJSON_ASSERT(node_ != nullptr);
    node_->data[size_] = 0;
    return JsonString(node_->data, size_, JsonString::Copied);
  }
 private:
  ResourceManager* resources_;
  StringNode* node_ = nullptr;
  size_t size_ = 0;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#if ARDUINOJSON_ENABLE_STD_STRING
#include <string>
#endif
#if ARDUINOJSON_ENABLE_STRING_VIEW
#include <string_view>
#endif
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
template <typename T, typename Enable>
struct Converter {
  static_assert(!detail::is_same<T, char>::value,
                "type 'char' is not supported, use 'signed char', 'unsigned "
                "char' or another integer type instead");
  static void toJson(const T& src, JsonVariant dst) {
    convertToJson(src, dst); // Error here? See https://arduinojson.org/v7/unsupported-set/
  }
  static T fromJson(JsonVariantConst src) {
    static_assert(!detail::is_same<T, char*>::value,
                  "type 'char*' is not supported, use 'const char*' instead");
    T result; // Error here? See https://arduinojson.org/v7/non-default-constructible/
    convertFromJson(src, result);  // Error here? See https://arduinojson.org/v7/unsupported-as/
    return result;
  }
  static bool checkJson(JsonVariantConst src) {
    static_assert(!detail::is_same<T, char*>::value,
                  "type 'char*' is not supported, use 'const char*' instead");
    T dummy = T();
    return canConvertFromJson(src, dummy);  // Error here? See https://arduinojson.org/v7/unsupported-is/
  }
};
template <typename T>
struct Converter<T, detail::enable_if_t<detail::is_integral<T>::value &&
                                        !detail::is_same<bool, T>::value &&
                                        !detail::is_same<char, T>::value>>
    : private detail::VariantAttorney {
  static bool toJson(T src, JsonVariant dst) {
    ARDUINOJSON_ASSERT_INTEGER_TYPE_IS_SUPPORTED(T);
    auto data = getData(dst);
    if (!data)
      return false;
    auto resources = getResourceManager(dst);
    data->clear(resources);
    return data->setInteger(src, resources);
  }
  static T fromJson(JsonVariantConst src) {
    ARDUINOJSON_ASSERT_INTEGER_TYPE_IS_SUPPORTED(T);
    auto data = getData(src);
    auto resources = getResourceManager(src);
    return data ? data->template asIntegral<T>(resources) : T();
  }
  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    auto resources = getResourceManager(src);
    return data && data->template isInteger<T>(resources);
  }
};
template <typename T>
struct Converter<T, detail::enable_if_t<detail::is_enum<T>::value>>
    : private detail::VariantAttorney {
  static bool toJson(T src, JsonVariant dst) {
    return dst.set(static_cast<JsonInteger>(src));
  }
  static T fromJson(JsonVariantConst src) {
    auto data = getData(src);
    auto resources = getResourceManager(src);
    return data ? static_cast<T>(data->template asIntegral<int>(resources))
                : T();
  }
  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    auto resources = getResourceManager(src);
    return data && data->template isInteger<int>(resources);
  }
};
template <>
struct Converter<bool> : private detail::VariantAttorney {
  static bool toJson(bool src, JsonVariant dst) {
    auto data = getData(dst);
    if (!data)
      return false;
    auto resources = getResourceManager(dst);
    data->clear(resources);
    data->setBoolean(src);
    return true;
  }
  static bool fromJson(JsonVariantConst src) {
    auto data = getData(src);
    auto resources = getResourceManager(src);
    return data ? data->asBoolean(resources) : false;
  }
  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    return data && data->isBoolean();
  }
};
template <typename T>
struct Converter<T, detail::enable_if_t<detail::is_floating_point<T>::value>>
    : private detail::VariantAttorney {
  static bool toJson(T src, JsonVariant dst) {
    auto data = getData(dst);
    if (!data)
      return false;
    auto resources = getResourceManager(dst);
    data->clear(resources);
    return data->setFloat(src, resources);
  }
  static T fromJson(JsonVariantConst src) {
    auto data = getData(src);
    auto resources = getResourceManager(src);
    return data ? data->template asFloat<T>(resources) : 0;
  }
  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    return data && data->isFloat();
  }
};
template <>
struct Converter<const char*> : private detail::VariantAttorney {
  static void toJson(const char* src, JsonVariant dst) {
    detail::VariantData::setString(getData(dst), detail::adaptString(src),
                                   getResourceManager(dst));
  }
  static const char* fromJson(JsonVariantConst src) {
    auto data = getData(src);
    return data ? data->asString().c_str() : 0;
  }
  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    return data && data->isString();
  }
};
template <>
struct Converter<JsonString> : private detail::VariantAttorney {
  static void toJson(JsonString src, JsonVariant dst) {
    detail::VariantData::setString(getData(dst), detail::adaptString(src),
                                   getResourceManager(dst));
  }
  static JsonString fromJson(JsonVariantConst src) {
    auto data = getData(src);
    return data ? data->asString() : 0;
  }
  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    return data && data->isString();
  }
};
template <typename T>
inline detail::enable_if_t<detail::IsString<T>::value> convertToJson(
    const T& src, JsonVariant dst) {
  using namespace detail;
  auto data = VariantAttorney::getData(dst);
  auto resources = VariantAttorney::getResourceManager(dst);
  detail::VariantData::setString(data, adaptString(src), resources);
}
template <typename T>
struct Converter<SerializedValue<T>> : private detail::VariantAttorney {
  static void toJson(SerializedValue<T> src, JsonVariant dst) {
    detail::VariantData::setRawString(getData(dst), src,
                                      getResourceManager(dst));
  }
};
template <>
struct Converter<detail::nullptr_t> : private detail::VariantAttorney {
  static void toJson(detail::nullptr_t, JsonVariant dst) {
    detail::VariantData::clear(getData(dst), getResourceManager(dst));
  }
  static detail::nullptr_t fromJson(JsonVariantConst) {
    return nullptr;
  }
  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    return data == 0 || data->isNull();
  }
};
#if ARDUINOJSON_ENABLE_ARDUINO_STREAM
namespace detail {
class StringBuilderPrint : public Print {
 public:
  StringBuilderPrint(ResourceManager* resources) : copier_(resources) {
    copier_.startString();
  }
  StringNode* save() {
    ARDUINOJSON_ASSERT(!overflowed());
    return copier_.save();
  }
  size_t write(uint8_t c) {
    copier_.append(char(c));
    return copier_.isValid() ? 1 : 0;
  }
  size_t write(const uint8_t* buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
      copier_.append(char(buffer[i]));
      if (!copier_.isValid())
        return i;
    }
    return size;
  }
  bool overflowed() const {
    return !copier_.isValid();
  }
 private:
  StringBuilder copier_;
};
}  // namespace detail

// Modified to work when printable.h isn't present. -- Zach Vorhies.
template<typename Printable>
inline void convertToJson(const Printable& src, JsonVariant dst) {
  auto resources = detail::VariantAttorney::getResourceManager(dst);
  auto data = detail::VariantAttorney::getData(dst);
  if (!resources || !data)
    return;
  data->clear(resources);
  detail::StringBuilderPrint print(resources);
  src.printTo(print);
  if (print.overflowed())
    return;
  data->setOwnedString(print.save());
}

#endif
#if ARDUINOJSON_ENABLE_ARDUINO_STRING
inline void convertFromJson(JsonVariantConst src, ::String& dst) {
  JsonString str = src.as<JsonString>();
  if (str)
    dst = str.c_str();
  else
    serializeJson(src, dst);
}
inline bool canConvertFromJson(JsonVariantConst src, const ::String&) {
  return src.is<JsonString>();
}
#endif
#if ARDUINOJSON_ENABLE_STD_STRING
inline void convertFromJson(JsonVariantConst src, std::string& dst) {
  JsonString str = src.as<JsonString>();
  if (str)
    dst.assign(str.c_str(), str.size());
  else
    serializeJson(src, dst);
}
inline bool canConvertFromJson(JsonVariantConst src, const std::string&) {
  return src.is<JsonString>();
}
#endif
#if ARDUINOJSON_ENABLE_STRING_VIEW
inline void convertFromJson(JsonVariantConst src, std::string_view& dst) {
  JsonString str = src.as<JsonString>();
  if (str)  // the standard doesn't allow passing null to the constructor
    dst = std::string_view(str.c_str(), str.size());
}
inline bool canConvertFromJson(JsonVariantConst src, const std::string_view&) {
  return src.is<JsonString>();
}
#endif
template <>
struct Converter<JsonArrayConst> : private detail::VariantAttorney {
  static void toJson(JsonArrayConst src, JsonVariant dst) {
    if (src.isNull())
      dst.set(nullptr);
    else
      dst.to<JsonArray>().set(src);
  }
  static JsonArrayConst fromJson(JsonVariantConst src) {
    auto data = getData(src);
    auto array = data ? data->asArray() : nullptr;
    return JsonArrayConst(array, getResourceManager(src));
  }
  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    return data && data->isArray();
  }
};
template <>
struct Converter<JsonArray> : private detail::VariantAttorney {
  static void toJson(JsonVariantConst src, JsonVariant dst) {
    if (src.isNull())
      dst.set(nullptr);
    else
      dst.to<JsonArray>().set(src);
  }
  static JsonArray fromJson(JsonVariant src) {
    auto data = getData(src);
    auto resources = getResourceManager(src);
    return JsonArray(data != 0 ? data->asArray() : 0, resources);
  }
  static bool checkJson(JsonVariant src) {
    auto data = getData(src);
    return data && data->isArray();
  }
};
template <>
struct Converter<JsonObjectConst> : private detail::VariantAttorney {
  static void toJson(JsonVariantConst src, JsonVariant dst) {
    if (src.isNull())
      dst.set(nullptr);
    else
      dst.to<JsonObject>().set(src);
  }
  static JsonObjectConst fromJson(JsonVariantConst src) {
    auto data = getData(src);
    auto object = data != 0 ? data->asObject() : nullptr;
    return JsonObjectConst(object, getResourceManager(src));
  }
  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    return data && data->isObject();
  }
};
template <>
struct Converter<JsonObject> : private detail::VariantAttorney {
  static void toJson(JsonVariantConst src, JsonVariant dst) {
    if (src.isNull())
      dst.set(nullptr);
    else
      dst.to<JsonObject>().set(src);
  }
  static JsonObject fromJson(JsonVariant src) {
    auto data = getData(src);
    auto resources = getResourceManager(src);
    return JsonObject(data != 0 ? data->asObject() : 0, resources);
  }
  static bool checkJson(JsonVariant src) {
    auto data = getData(src);
    return data && data->isObject();
  }
};
ARDUINOJSON_END_PUBLIC_NAMESPACE
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
class JsonVariantCopier {
 public:
  typedef bool result_type;
  JsonVariantCopier(JsonVariant dst) : dst_(dst) {}
  template <typename T>
  bool visit(T src) {
    return dst_.set(src);
  }
 private:
  JsonVariant dst_;
};
inline bool copyVariant(JsonVariant dst, JsonVariantConst src) {
  if (dst.isUnbound())
    return false;
  JsonVariantCopier copier(dst);
  return accept(src, copier);
}
template <typename T>
inline void VariantData::setRawString(SerializedValue<T> value,
                                      ResourceManager* resources) {
  ARDUINOJSON_ASSERT(type_ == VariantType::Null);  // must call clear() first
  auto dup = resources->saveString(adaptString(value.data(), value.size()));
  if (dup)
    setRawString(dup);
}
template <typename TAdaptedString>
inline bool VariantData::setString(TAdaptedString value,
                                   ResourceManager* resources) {
  ARDUINOJSON_ASSERT(type_ == VariantType::Null);  // must call clear() first
  if (value.isNull())
    return false;
  if (value.isLinked()) {
    setLinkedString(value.data());
    return true;
  }
  auto dup = resources->saveString(value);
  if (dup) {
    setOwnedString(dup);
    return true;
  }
  return false;
}
inline void VariantData::clear(ResourceManager* resources) {
  if (type_ & VariantTypeBits::OwnedStringBit)
    resources->dereferenceString(content_.asOwnedString->data);
#if ARDUINOJSON_USE_EXTENSIONS
  if (type_ & VariantTypeBits::ExtensionBit)
    resources->freeExtension(content_.asSlotId);
#endif
  auto collection = asCollection();
  if (collection)
    collection->clear(resources);
  type_ = VariantType::Null;
}
#if ARDUINOJSON_USE_EXTENSIONS
inline const VariantExtension* VariantData::getExtension(
    const ResourceManager* resources) const {
  return type_ & VariantTypeBits::ExtensionBit
             ? resources->getExtension(content_.asSlotId)
             : nullptr;
}
#endif
template <typename T>
enable_if_t<sizeof(T) == 8, bool> VariantData::setFloat(
    T value, ResourceManager* resources) {
  ARDUINOJSON_ASSERT(type_ == VariantType::Null);  // must call clear() first
  (void)resources;                                 // silence warning
  float valueAsFloat = static_cast<float>(value);
#if ARDUINOJSON_USE_DOUBLE
  if (value == valueAsFloat) {
    type_ = VariantType::Float;
    content_.asFloat = valueAsFloat;
  } else {
    auto extension = resources->allocExtension();
    if (!extension)
      return false;
    type_ = VariantType::Double;
    content_.asSlotId = extension.id();
    extension->asDouble = value;
  }
#else
  type_ = VariantType::Float;
  content_.asFloat = valueAsFloat;
#endif
  return true;
}
template <typename T>
enable_if_t<is_signed<T>::value, bool> VariantData::setInteger(
    T value, ResourceManager* resources) {
  ARDUINOJSON_ASSERT(type_ == VariantType::Null);  // must call clear() first
  (void)resources;                                 // silence warning
  if (canConvertNumber<int32_t>(value)) {
    type_ = VariantType::Int32;
    content_.asInt32 = static_cast<int32_t>(value);
  }
#if ARDUINOJSON_USE_LONG_LONG
  else {
    auto extension = resources->allocExtension();
    if (!extension)
      return false;
    type_ = VariantType::Int64;
    content_.asSlotId = extension.id();
    extension->asInt64 = value;
  }
#endif
  return true;
}
template <typename T>
enable_if_t<is_unsigned<T>::value, bool> VariantData::setInteger(
    T value, ResourceManager* resources) {
  ARDUINOJSON_ASSERT(type_ == VariantType::Null);  // must call clear() first
  (void)resources;                                 // silence warning
  if (canConvertNumber<uint32_t>(value)) {
    type_ = VariantType::Uint32;
    content_.asUint32 = static_cast<uint32_t>(value);
  }
#if ARDUINOJSON_USE_LONG_LONG
  else {
    auto extension = resources->allocExtension();
    if (!extension)
      return false;
    type_ = VariantType::Uint64;
    content_.asSlotId = extension.id();
    extension->asUint64 = value;
  }
#endif
  return true;
}
template <typename TDerived>
inline JsonVariant VariantRefBase<TDerived>::add() const {
  return add<JsonVariant>();
}
template <typename TDerived>
template <typename T>
inline T VariantRefBase<TDerived>::as() const {
  using variant_type =  // JsonVariantConst or JsonVariant?
      typename function_traits<decltype(&Converter<T>::fromJson)>::arg1_type;
  return Converter<T>::fromJson(getVariant<variant_type>());
}
template <typename TDerived>
inline JsonArray VariantRefBase<TDerived>::createNestedArray() const {
  return add<JsonArray>();
}
template <typename TDerived>
template <typename TChar>
inline JsonArray VariantRefBase<TDerived>::createNestedArray(TChar* key) const {
  return operator[](key).template to<JsonArray>();
}
template <typename TDerived>
template <typename TString>
inline JsonArray VariantRefBase<TDerived>::createNestedArray(
    const TString& key) const {
  return operator[](key).template to<JsonArray>();
}
template <typename TDerived>
inline JsonObject VariantRefBase<TDerived>::createNestedObject() const {
  return add<JsonObject>();
}
template <typename TDerived>
template <typename TChar>
inline JsonObject VariantRefBase<TDerived>::createNestedObject(
    TChar* key) const {
  return operator[](key).template to<JsonObject>();
}
template <typename TDerived>
template <typename TString>
inline JsonObject VariantRefBase<TDerived>::createNestedObject(
    const TString& key) const {
  return operator[](key).template to<JsonObject>();
}
template <typename TDerived>
inline void convertToJson(const VariantRefBase<TDerived>& src,
                          JsonVariant dst) {
  dst.set(src.template as<JsonVariantConst>());
}
template <typename TDerived>
template <typename T>
inline enable_if_t<is_same<T, JsonVariant>::value, T>
VariantRefBase<TDerived>::add() const {
  return JsonVariant(
      detail::VariantData::addElement(getOrCreateData(), getResourceManager()),
      getResourceManager());
}
template <typename TDerived>
template <typename TString>
inline enable_if_t<IsString<TString>::value, bool>
VariantRefBase<TDerived>::containsKey(const TString& key) const {
  return VariantData::getMember(getData(), adaptString(key),
                                getResourceManager()) != 0;
}
template <typename TDerived>
template <typename TChar>
inline enable_if_t<IsString<TChar*>::value, bool>
VariantRefBase<TDerived>::containsKey(TChar* key) const {
  return VariantData::getMember(getData(), adaptString(key),
                                getResourceManager()) != 0;
}
template <typename TDerived>
template <typename TVariant>
inline enable_if_t<IsVariant<TVariant>::value, bool>
VariantRefBase<TDerived>::containsKey(const TVariant& key) const {
  return containsKey(key.template as<const char*>());
}
template <typename TDerived>
inline JsonVariant VariantRefBase<TDerived>::getVariant() const {
  return JsonVariant(getData(), getResourceManager());
}
template <typename TDerived>
inline JsonVariant VariantRefBase<TDerived>::getOrCreateVariant() const {
  return JsonVariant(getOrCreateData(), getResourceManager());
}
template <typename TDerived>
template <typename T>
inline bool VariantRefBase<TDerived>::is() const {
  using variant_type =  // JsonVariantConst or JsonVariant?
      typename function_traits<decltype(&Converter<T>::checkJson)>::arg1_type;
  return Converter<T>::checkJson(getVariant<variant_type>());
}
template <typename TDerived>
inline ElementProxy<TDerived> VariantRefBase<TDerived>::operator[](
    size_t index) const {
  return ElementProxy<TDerived>(derived(), index);
}
template <typename TDerived>
template <typename TString>
inline enable_if_t<IsString<TString*>::value, MemberProxy<TDerived, TString*>>
VariantRefBase<TDerived>::operator[](TString* key) const {
  return MemberProxy<TDerived, TString*>(derived(), key);
}
template <typename TDerived>
template <typename TString>
inline enable_if_t<IsString<TString>::value, MemberProxy<TDerived, TString>>
VariantRefBase<TDerived>::operator[](const TString& key) const {
  return MemberProxy<TDerived, TString>(derived(), key);
}
template <typename TDerived>
template <typename TConverter, typename T>
inline bool VariantRefBase<TDerived>::doSet(T&& value, false_type) const {
  TConverter::toJson(value, getOrCreateVariant());
  auto resources = getResourceManager();
  return resources && !resources->overflowed();
}
template <typename TDerived>
template <typename TConverter, typename T>
inline bool VariantRefBase<TDerived>::doSet(T&& value, true_type) const {
  return TConverter::toJson(value, getOrCreateVariant());
}
template <typename TDerived>
template <typename T>
inline enable_if_t<is_same<T, JsonArray>::value, JsonArray>
VariantRefBase<TDerived>::to() const {
  return JsonArray(
      VariantData::toArray(getOrCreateData(), getResourceManager()),
      getResourceManager());
}
template <typename TDerived>
template <typename T>
enable_if_t<is_same<T, JsonObject>::value, JsonObject>
VariantRefBase<TDerived>::to() const {
  return JsonObject(
      VariantData::toObject(getOrCreateData(), getResourceManager()),
      getResourceManager());
}
template <typename TDerived>
template <typename T>
enable_if_t<is_same<T, JsonVariant>::value, JsonVariant>
VariantRefBase<TDerived>::to() const {
  auto data = getOrCreateData();
  auto resources = getResourceManager();
  detail::VariantData::clear(data, resources);
  return JsonVariant(data, resources);
}
ARDUINOJSON_END_PRIVATE_NAMESPACE
#if ARDUINOJSON_ENABLE_STD_STREAM
#endif
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
class DeserializationError {
 public:
  enum Code {
    Ok,
    EmptyInput,
    IncompleteInput,
    InvalidInput,
    NoMemory,
    TooDeep
  };
  DeserializationError() {}
  DeserializationError(Code c) : code_(c) {}
  friend bool operator==(const DeserializationError& lhs,
                         const DeserializationError& rhs) {
    return lhs.code_ == rhs.code_;
  }
  friend bool operator!=(const DeserializationError& lhs,
                         const DeserializationError& rhs) {
    return lhs.code_ != rhs.code_;
  }
  friend bool operator==(const DeserializationError& lhs, Code rhs) {
    return lhs.code_ == rhs;
  }
  friend bool operator==(Code lhs, const DeserializationError& rhs) {
    return lhs == rhs.code_;
  }
  friend bool operator!=(const DeserializationError& lhs, Code rhs) {
    return lhs.code_ != rhs;
  }
  friend bool operator!=(Code lhs, const DeserializationError& rhs) {
    return lhs != rhs.code_;
  }
  explicit operator bool() const {
    return code_ != Ok;
  }
  Code code() const {
    return code_;
  }
  const char* c_str() const {
    static const char* messages[] = {
        "Ok",           "EmptyInput", "IncompleteInput",
        "InvalidInput", "NoMemory",   "TooDeep"};
    ARDUINOJSON_ASSERT(static_cast<size_t>(code_) <
                       sizeof(messages) / sizeof(messages[0]));
    return messages[code_];
  }
#if ARDUINOJSON_ENABLE_PROGMEM
  const __FlashStringHelper* f_str() const {
    ARDUINOJSON_DEFINE_PROGMEM_ARRAY(char, s0, "Ok");
    ARDUINOJSON_DEFINE_PROGMEM_ARRAY(char, s1, "EmptyInput");
    ARDUINOJSON_DEFINE_PROGMEM_ARRAY(char, s2, "IncompleteInput");
    ARDUINOJSON_DEFINE_PROGMEM_ARRAY(char, s3, "InvalidInput");
    ARDUINOJSON_DEFINE_PROGMEM_ARRAY(char, s4, "NoMemory");
    ARDUINOJSON_DEFINE_PROGMEM_ARRAY(char, s5, "TooDeep");
    ARDUINOJSON_DEFINE_PROGMEM_ARRAY(const char*, messages,
                                     {s0, s1, s2, s3, s4, s5});
    return reinterpret_cast<const __FlashStringHelper*>(
        detail::pgm_read(messages + code_));
  }
#endif
 private:
  Code code_;
};
#if ARDUINOJSON_ENABLE_STD_STREAM
inline std::ostream& operator<<(std::ostream& s,
                                const DeserializationError& e) {
  s << e.c_str();
  return s;
}
inline std::ostream& operator<<(std::ostream& s, DeserializationError::Code c) {
  s << DeserializationError(c).c_str();
  return s;
}
#endif
namespace DeserializationOption {
class Filter {
 public:
#if ARDUINOJSON_AUTO_SHRINK
  explicit Filter(JsonDocument& doc) : variant_(doc) {
    doc.shrinkToFit();
  }
#endif
  explicit Filter(JsonVariantConst variant) : variant_(variant) {}
  bool allow() const {
    return variant_;
  }
  bool allowArray() const {
    return variant_ == true || variant_.is<JsonArrayConst>();
  }
  bool allowObject() const {
    return variant_ == true || variant_.is<JsonObjectConst>();
  }
  bool allowValue() const {
    return variant_ == true;
  }
  template <typename TKey>
  Filter operator[](const TKey& key) const {
    if (variant_ == true)  // "true" means "allow recursively"
      return *this;
    JsonVariantConst member = variant_[key];
    return Filter(member.isNull() ? variant_["*"] : member);
  }
 private:
  JsonVariantConst variant_;
};
}  // namespace DeserializationOption
namespace detail {
struct AllowAllFilter {
  bool allow() const {
    return true;
  }
  bool allowArray() const {
    return true;
  }
  bool allowObject() const {
    return true;
  }
  bool allowValue() const {
    return true;
  }
  template <typename TKey>
  AllowAllFilter operator[](const TKey&) const {
    return AllowAllFilter();
  }
};
}  // namespace detail
namespace DeserializationOption {
class NestingLimit {
 public:
  NestingLimit() : value_(ARDUINOJSON_DEFAULT_NESTING_LIMIT) {}
  explicit NestingLimit(uint8_t n) : value_(n) {}
  NestingLimit decrement() const {
    ARDUINOJSON_ASSERT(value_ > 0);
    return NestingLimit(static_cast<uint8_t>(value_ - 1));
  }
  bool reached() const {
    return value_ == 0;
  }
 private:
  uint8_t value_;
};
}  // namespace DeserializationOption
ARDUINOJSON_END_PUBLIC_NAMESPACE
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename TFilter>
struct DeserializationOptions {
  TFilter filter;
  DeserializationOption::NestingLimit nestingLimit;
};
template <typename TFilter>
inline DeserializationOptions<TFilter> makeDeserializationOptions(
    TFilter filter, DeserializationOption::NestingLimit nestingLimit = {}) {
  return {filter, nestingLimit};
}
template <typename TFilter>
inline DeserializationOptions<TFilter> makeDeserializationOptions(
    DeserializationOption::NestingLimit nestingLimit, TFilter filter) {
  return {filter, nestingLimit};
}
inline DeserializationOptions<AllowAllFilter> makeDeserializationOptions(
    DeserializationOption::NestingLimit nestingLimit = {}) {
  return {{}, nestingLimit};
}
template <typename TSource, typename Enable = void>
struct Reader {
 public:
  Reader(TSource& source) : source_(&source) {}
  int read() {
    return source_->read();  // Error here? See https://arduinojson.org/v7/invalid-input/
  }
  size_t readBytes(char* buffer, size_t length) {
    return source_->readBytes(buffer, length);
  }
 private:
  TSource* source_;
};
template <typename TSource, typename Enable = void>
struct BoundedReader {
};
template <typename TIterator>
class IteratorReader {
  TIterator ptr_, end_;
 public:
  explicit IteratorReader(TIterator begin, TIterator end)
      : ptr_(begin), end_(end) {}
  int read() {
    if (ptr_ < end_)
      return static_cast<unsigned char>(*ptr_++);
    else
      return -1;
  }
  size_t readBytes(char* buffer, size_t length) {
    size_t i = 0;
    while (i < length && ptr_ < end_)
      buffer[i++] = *ptr_++;
    return i;
  }
};
template <typename TSource>
struct Reader<TSource, void_t<typename TSource::const_iterator>>
    : IteratorReader<typename TSource::const_iterator> {
  explicit Reader(const TSource& source)
      : IteratorReader<typename TSource::const_iterator>(source.begin(),
                                                         source.end()) {}
};
template <typename T>
struct IsCharOrVoid {
  static const bool value =
      is_same<T, void>::value || is_same<T, char>::value ||
      is_same<T, unsigned char>::value || is_same<T, signed char>::value;
};
template <typename T>
struct IsCharOrVoid<const T> : IsCharOrVoid<T> {};
template <typename TSource>
struct Reader<TSource*, enable_if_t<IsCharOrVoid<TSource>::value>> {
  const char* ptr_;
 public:
  explicit Reader(const void* ptr)
      : ptr_(ptr ? reinterpret_cast<const char*>(ptr) : "") {}
  int read() {
    return static_cast<unsigned char>(*ptr_++);
  }
  size_t readBytes(char* buffer, size_t length) {
    for (size_t i = 0; i < length; i++)
      buffer[i] = *ptr_++;
    return length;
  }
};
template <typename TSource>
struct BoundedReader<TSource*, enable_if_t<IsCharOrVoid<TSource>::value>>
    : public IteratorReader<const char*> {
 public:
  explicit BoundedReader(const void* ptr, size_t len)
      : IteratorReader<const char*>(reinterpret_cast<const char*>(ptr),
                                    reinterpret_cast<const char*>(ptr) + len) {}
};
template <typename TVariant>
struct Reader<TVariant, enable_if_t<IsVariant<TVariant>::value>>
    : Reader<char*, void> {
  explicit Reader(const TVariant& x)
      : Reader<char*, void>(x.template as<const char*>()) {}
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#if ARDUINOJSON_ENABLE_ARDUINO_STREAM
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename TSource>
struct Reader<TSource, enable_if_t<is_base_of<Stream, TSource>::value>> {
 public:
  explicit Reader(Stream& stream) : stream_(&stream) {}
  int read() {
    char c;
    return stream_->readBytes(&c, 1) ? static_cast<unsigned char>(c) : -1;
  }
  size_t readBytes(char* buffer, size_t length) {
    return stream_->readBytes(buffer, length);
  }
 private:
  Stream* stream_;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#endif
#if ARDUINOJSON_ENABLE_ARDUINO_STRING
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename TSource>
struct Reader<TSource, enable_if_t<is_base_of<::String, TSource>::value>>
    : BoundedReader<const char*> {
  explicit Reader(const ::String& s)
      : BoundedReader<const char*>(s.c_str(), s.length()) {}
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#endif
#if ARDUINOJSON_ENABLE_PROGMEM
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <>
struct Reader<const __FlashStringHelper*, void> {
  const char* ptr_;
 public:
  explicit Reader(const __FlashStringHelper* ptr)
      : ptr_(reinterpret_cast<const char*>(ptr)) {}
  int read() {
    return pgm_read_byte(ptr_++);
  }
  size_t readBytes(char* buffer, size_t length) {
    memcpy_P(buffer, ptr_, length);
    ptr_ += length;
    return length;
  }
};
template <>
struct BoundedReader<const __FlashStringHelper*, void> {
  const char* ptr_;
  const char* end_;
 public:
  explicit BoundedReader(const __FlashStringHelper* ptr, size_t size)
      : ptr_(reinterpret_cast<const char*>(ptr)), end_(ptr_ + size) {}
  int read() {
    if (ptr_ < end_)
      return pgm_read_byte(ptr_++);
    else
      return -1;
  }
  size_t readBytes(char* buffer, size_t length) {
    size_t available = static_cast<size_t>(end_ - ptr_);
    if (available < length)
      length = available;
    memcpy_P(buffer, ptr_, length);
    ptr_ += length;
    return length;
  }
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#endif
#if ARDUINOJSON_ENABLE_STD_STREAM
#include <istream>
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename TSource>
struct Reader<TSource, enable_if_t<is_base_of<std::istream, TSource>::value>> {
 public:
  explicit Reader(std::istream& stream) : stream_(&stream) {}
  int read() {
    return stream_->get();
  }
  size_t readBytes(char* buffer, size_t length) {
    stream_->read(buffer, static_cast<std::streamsize>(length));
    return static_cast<size_t>(stream_->gcount());
  }
 private:
  std::istream* stream_;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename TInput>
Reader<remove_reference_t<TInput>> makeReader(TInput&& input) {
  return Reader<remove_reference_t<TInput>>{detail::forward<TInput>(input)};
}
template <typename TChar>
BoundedReader<TChar*> makeReader(TChar* input, size_t inputSize) {
  return BoundedReader<TChar*>{input, inputSize};
}
template <typename...>
struct first_or_void {
  using type = void;
};
template <typename T, typename... Rest>
struct first_or_void<T, Rest...> {
  using type = T;
};
template <class T, class = void>
struct is_deserialize_destination : false_type {};
template <class T>
struct is_deserialize_destination<
    T, enable_if_t<is_same<decltype(VariantAttorney::getResourceManager(
                               detail::declval<T&>())),
                           ResourceManager*>::value>> : true_type {};
template <typename TDestination>
inline void shrinkJsonDocument(TDestination&) {
}
#if ARDUINOJSON_AUTO_SHRINK
inline void shrinkJsonDocument(JsonDocument& doc) {
  doc.shrinkToFit();
}
#endif
template <template <typename> class TDeserializer, typename TDestination,
          typename TReader, typename TOptions>
DeserializationError doDeserialize(TDestination&& dst, TReader reader,
                                   TOptions options) {
  auto data = VariantAttorney::getOrCreateData(dst);
  if (!data)
    return DeserializationError::NoMemory;
  auto resources = VariantAttorney::getResourceManager(dst);
  dst.clear();
  auto err = TDeserializer<TReader>(resources, reader)
                 .parse(*data, options.filter, options.nestingLimit);
  shrinkJsonDocument(dst);
  return err;
}
template <template <typename> class TDeserializer, typename TDestination,
          typename TStream, typename... Args,
          typename = enable_if_t<  // issue #1897
              !is_integral<typename first_or_void<Args...>::type>::value>>
DeserializationError deserialize(TDestination&& dst, TStream&& input,
                                 Args... args) {
  return doDeserialize<TDeserializer>(
      dst, makeReader(detail::forward<TStream>(input)),
      makeDeserializationOptions(args...));
}
template <template <typename> class TDeserializer, typename TDestination,
          typename TChar, typename Size, typename... Args,
          typename = enable_if_t<is_integral<Size>::value>>
DeserializationError deserialize(TDestination&& dst, TChar* input,
                                 Size inputSize, Args... args) {
  return doDeserialize<TDeserializer>(dst, makeReader(input, size_t(inputSize)),
                                      makeDeserializationOptions(args...));
}
template <typename TReader>
class Latch {
 public:
  Latch(TReader reader) : reader_(reader), loaded_(false) {
#if ARDUINOJSON_DEBUG
    ended_ = false;
#endif
  }
  void clear() {
    loaded_ = false;
  }
  int last() const {
    return current_;
  }
  FORCE_INLINE char current() {
    if (!loaded_) {
      load();
    }
    return current_;
  }
 private:
  void load() {
    ARDUINOJSON_ASSERT(!ended_);
    int c = reader_.read();
#if ARDUINOJSON_DEBUG
    if (c <= 0)
      ended_ = true;
#endif
    current_ = static_cast<char>(c > 0 ? c : 0);
    loaded_ = true;
  }
  TReader reader_;
  char current_;  // NOLINT(clang-analyzer-optin.cplusplus.UninitializedObject)
  bool loaded_;
#if ARDUINOJSON_DEBUG
  bool ended_;
#endif
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
#if defined(__GNUC__)
#  if __GNUC__ >= 7
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#  endif
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
namespace Utf16 {
inline bool isHighSurrogate(uint16_t codeunit) {
  return codeunit >= 0xD800 && codeunit < 0xDC00;
}
inline bool isLowSurrogate(uint16_t codeunit) {
  return codeunit >= 0xDC00 && codeunit < 0xE000;
}
class Codepoint {
 public:
  Codepoint() : highSurrogate_(0), codepoint_(0) {}
  bool append(uint16_t codeunit) {
    if (isHighSurrogate(codeunit)) {
      highSurrogate_ = codeunit & 0x3FF;
      return false;
    }
    if (isLowSurrogate(codeunit)) {
      codepoint_ =
          uint32_t(0x10000 + ((highSurrogate_ << 10) | (codeunit & 0x3FF)));
      return true;
    }
    codepoint_ = codeunit;
    return true;
  }
  uint32_t value() const {
    return codepoint_;
  }
 private:
  uint16_t highSurrogate_;
  uint32_t codepoint_;
};
}  // namespace Utf16
ARDUINOJSON_END_PRIVATE_NAMESPACE
#if defined(__GNUC__)
#  if __GNUC__ >= 8
#    pragma GCC diagnostic pop
#  endif
#endif
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
namespace Utf8 {
template <typename TStringBuilder>
inline void encodeCodepoint(uint32_t codepoint32, TStringBuilder& str) {
  if (codepoint32 < 0x80) {
    str.append(char(codepoint32));
  } else {
    char buf[5];
    char* p = buf;
    *(p++) = 0;
    *(p++) = char((codepoint32 | 0x80) & 0xBF);
    uint16_t codepoint16 = uint16_t(codepoint32 >> 6);
    if (codepoint16 < 0x20) {  // 0x800
      *(p++) = char(codepoint16 | 0xC0);
    } else {
      *(p++) = char((codepoint16 | 0x80) & 0xBF);
      codepoint16 = uint16_t(codepoint16 >> 6);
      if (codepoint16 < 0x10) {  // 0x10000
        *(p++) = char(codepoint16 | 0xE0);
      } else {
        *(p++) = char((codepoint16 | 0x80) & 0xBF);
        codepoint16 = uint16_t(codepoint16 >> 6);
        *(p++) = char(codepoint16 | 0xF0);
      }
    }
    while (*(--p)) {
      str.append(*p);
    }
  }
}
}  // namespace Utf8
#ifndef isdigit
inline bool isdigit(char c) {
  return '0' <= c && c <= '9';
}
#endif
inline bool issign(char c) {
  return '-' == c || c == '+';
}
template <typename A, typename B>
using largest_type = conditional_t<(sizeof(A) > sizeof(B)), A, B>;
enum class NumberType : uint8_t {
  Invalid,
  Float,
  SignedInteger,
  UnsignedInteger,
#if ARDUINOJSON_USE_DOUBLE
  Double,
#endif
};
union NumberValue {
  NumberValue() {}
  NumberValue(float x) : asFloat(x) {}
  NumberValue(JsonInteger x) : asSignedInteger(x) {}
  NumberValue(JsonUInt x) : asUnsignedInteger(x) {}
#if ARDUINOJSON_USE_DOUBLE
  NumberValue(double x) : asDouble(x) {}
#endif
  JsonInteger asSignedInteger;
  JsonUInt asUnsignedInteger;
  float asFloat;
#if ARDUINOJSON_USE_DOUBLE
  double asDouble;
#endif
};
class Number {
  NumberType type_;
  NumberValue value_;
 public:
  Number() : type_(NumberType::Invalid) {}
  Number(float value) : type_(NumberType::Float), value_(value) {}
  Number(JsonInteger value) : type_(NumberType::SignedInteger), value_(value) {}
  Number(JsonUInt value) : type_(NumberType::UnsignedInteger), value_(value) {}
#if ARDUINOJSON_USE_DOUBLE
  Number(double value) : type_(NumberType::Double), value_(value) {}
#endif
  template <typename T>
  T convertTo() const {
    switch (type_) {
      case NumberType::Float:
        return convertNumber<T>(value_.asFloat);
      case NumberType::SignedInteger:
        return convertNumber<T>(value_.asSignedInteger);
      case NumberType::UnsignedInteger:
        return convertNumber<T>(value_.asUnsignedInteger);
#if ARDUINOJSON_USE_DOUBLE
      case NumberType::Double:
        return convertNumber<T>(value_.asDouble);
#endif
      default:
        return T();
    }
  }
  NumberType type() const {
    return type_;
  }
  JsonInteger asSignedInteger() const {
    ARDUINOJSON_ASSERT(type_ == NumberType::SignedInteger);
    return value_.asSignedInteger;
  }
  JsonUInt asUnsignedInteger() const {
    ARDUINOJSON_ASSERT(type_ == NumberType::UnsignedInteger);
    return value_.asUnsignedInteger;
  }
  float asFloat() const {
    ARDUINOJSON_ASSERT(type_ == NumberType::Float);
    return value_.asFloat;
  }
#if ARDUINOJSON_USE_DOUBLE
  double asDouble() const {
    ARDUINOJSON_ASSERT(type_ == NumberType::Double);
    return value_.asDouble;
  }
#endif
};
inline Number parseNumber(const char* s) {
  typedef FloatTraits<JsonFloat> traits;
  typedef largest_type<traits::mantissa_type, JsonUInt> mantissa_t;
  typedef traits::exponent_type exponent_t;
  ARDUINOJSON_ASSERT(s != 0);
  bool is_negative = false;
  switch (*s) {
    case '-':
      is_negative = true;
      s++;
      break;
    case '+':
      s++;
      break;
  }
#if ARDUINOJSON_ENABLE_NAN
  if (*s == 'n' || *s == 'N') {
    return Number(traits::nan());
  }
#endif
#if ARDUINOJSON_ENABLE_INFINITY
  if (*s == 'i' || *s == 'I') {
    return Number(is_negative ? -traits::inf() : traits::inf());
  }
#endif
  if (!isdigit(*s) && *s != '.')
    return Number();
  mantissa_t mantissa = 0;
  exponent_t exponent_offset = 0;
  const mantissa_t maxUint = JsonUInt(-1);
  while (isdigit(*s)) {
    uint8_t digit = uint8_t(*s - '0');
    if (mantissa > maxUint / 10)
      break;
    mantissa *= 10;
    if (mantissa > maxUint - digit)
      break;
    mantissa += digit;
    s++;
  }
  if (*s == '\0') {
    if (is_negative) {
      const mantissa_t sintMantissaMax = mantissa_t(1)
                                         << (sizeof(JsonInteger) * 8 - 1);
      if (mantissa <= sintMantissaMax) {
        return Number(JsonInteger(~mantissa + 1));
      }
    } else {
      return Number(JsonUInt(mantissa));
    }
  }
  while (mantissa > traits::mantissa_max) {
    mantissa /= 10;
    exponent_offset++;
  }
  while (isdigit(*s)) {
    exponent_offset++;
    s++;
  }
  if (*s == '.') {
    s++;
    while (isdigit(*s)) {
      if (mantissa < traits::mantissa_max / 10) {
        mantissa = mantissa * 10 + uint8_t(*s - '0');
        exponent_offset--;
      }
      s++;
    }
  }
  int exponent = 0;
  if (*s == 'e' || *s == 'E') {
    s++;
    bool negative_exponent = false;
    if (*s == '-') {
      negative_exponent = true;
      s++;
    } else if (*s == '+') {
      s++;
    }
    while (isdigit(*s)) {
      exponent = exponent * 10 + (*s - '0');
      if (exponent + exponent_offset > traits::exponent_max) {
        if (negative_exponent)
          return Number(is_negative ? -0.0f : 0.0f);
        else
          return Number(is_negative ? -traits::inf() : traits::inf());
      }
      s++;
    }
    if (negative_exponent)
      exponent = -exponent;
  }
  exponent += exponent_offset;
  if (*s != '\0')
    return Number();
#if ARDUINOJSON_USE_DOUBLE
  bool isDouble = exponent < -FloatTraits<float>::exponent_max ||
                  exponent > FloatTraits<float>::exponent_max ||
                  mantissa > FloatTraits<float>::mantissa_max;
  if (isDouble) {
    auto final_result = make_float(double(mantissa), exponent);
    return Number(is_negative ? -final_result : final_result);
  } else
#endif
  {
    auto final_result = make_float(float(mantissa), exponent);
    return Number(is_negative ? -final_result : final_result);
  }
}
template <typename T>
inline T parseNumber(const char* s) {
  return parseNumber(s).convertTo<T>();
}
template <typename TReader>
class JsonDeserializer {
 public:
  JsonDeserializer(ResourceManager* resources, TReader reader)
      : stringBuilder_(resources),
        foundSomething_(false),
        latch_(reader),
        resources_(resources) {}
  template <typename TFilter>
  DeserializationError parse(VariantData& variant, TFilter filter,
                             DeserializationOption::NestingLimit nestingLimit) {
    DeserializationError::Code err;
    err = parseVariant(variant, filter, nestingLimit);
    if (!err && latch_.last() != 0 && variant.isFloat()) {
      return DeserializationError::InvalidInput;
    }
    return err;
  }
 private:
  char current() {
    return latch_.current();
  }
  void move() {
    latch_.clear();
  }
  bool eat(char charToSkip) {
    if (current() != charToSkip)
      return false;
    move();
    return true;
  }
  template <typename TFilter>
  DeserializationError::Code parseVariant(
      VariantData& variant, TFilter filter,
      DeserializationOption::NestingLimit nestingLimit) {
    DeserializationError::Code err;
    err = skipSpacesAndComments();
    if (err)
      return err;
    switch (current()) {
      case '[':
        if (filter.allowArray())
          return parseArray(variant.toArray(), filter, nestingLimit);
        else
          return skipArray(nestingLimit);
      case '{':
        if (filter.allowObject())
          return parseObject(variant.toObject(), filter, nestingLimit);
        else
          return skipObject(nestingLimit);
      case '\"':
      case '\'':
        if (filter.allowValue())
          return parseStringValue(variant);
        else
          return skipQuotedString();
      case 't':
        if (filter.allowValue())
          variant.setBoolean(true);
        return skipKeyword("true");
      case 'f':
        if (filter.allowValue())
          variant.setBoolean(false);
        return skipKeyword("false");
      case 'n':
        return skipKeyword("null");
      default:
        if (filter.allowValue())
          return parseNumericValue(variant);
        else
          return skipNumericValue();
    }
  }
  DeserializationError::Code skipVariant(
      DeserializationOption::NestingLimit nestingLimit) {
    DeserializationError::Code err;
    err = skipSpacesAndComments();
    if (err)
      return err;
    switch (current()) {
      case '[':
        return skipArray(nestingLimit);
      case '{':
        return skipObject(nestingLimit);
      case '\"':
      case '\'':
        return skipQuotedString();
      case 't':
        return skipKeyword("true");
      case 'f':
        return skipKeyword("false");
      case 'n':
        return skipKeyword("null");
      default:
        return skipNumericValue();
    }
  }
  template <typename TFilter>
  DeserializationError::Code parseArray(
      ArrayData& array, TFilter filter,
      DeserializationOption::NestingLimit nestingLimit) {
    DeserializationError::Code err;
    if (nestingLimit.reached())
      return DeserializationError::TooDeep;
    ARDUINOJSON_ASSERT(current() == '[');
    move();
    err = skipSpacesAndComments();
    if (err)
      return err;
    if (eat(']'))
      return DeserializationError::Ok;
    TFilter elementFilter = filter[0UL];
    for (;;) {
      if (elementFilter.allow()) {
        VariantData* value = array.addElement(resources_);
        if (!value)
          return DeserializationError::NoMemory;
        err = parseVariant(*value, elementFilter, nestingLimit.decrement());
        if (err)
          return err;
      } else {
        err = skipVariant(nestingLimit.decrement());
        if (err)
          return err;
      }
      err = skipSpacesAndComments();
      if (err)
        return err;
      if (eat(']'))
        return DeserializationError::Ok;
      if (!eat(','))
        return DeserializationError::InvalidInput;
    }
  }
  DeserializationError::Code skipArray(
      DeserializationOption::NestingLimit nestingLimit) {
    DeserializationError::Code err;
    if (nestingLimit.reached())
      return DeserializationError::TooDeep;
    ARDUINOJSON_ASSERT(current() == '[');
    move();
    for (;;) {
      err = skipVariant(nestingLimit.decrement());
      if (err)
        return err;
      err = skipSpacesAndComments();
      if (err)
        return err;
      if (eat(']'))
        return DeserializationError::Ok;
      if (!eat(','))
        return DeserializationError::InvalidInput;
    }
  }
  template <typename TFilter>
  DeserializationError::Code parseObject(
      ObjectData& object, TFilter filter,
      DeserializationOption::NestingLimit nestingLimit) {
    DeserializationError::Code err;
    if (nestingLimit.reached())
      return DeserializationError::TooDeep;
    ARDUINOJSON_ASSERT(current() == '{');
    move();
    err = skipSpacesAndComments();
    if (err)
      return err;
    if (eat('}'))
      return DeserializationError::Ok;
    for (;;) {
      err = parseKey();
      if (err)
        return err;
      err = skipSpacesAndComments();
      if (err)
        return err;
      if (!eat(':'))
        return DeserializationError::InvalidInput;
      JsonString key = stringBuilder_.str();
      TFilter memberFilter = filter[key.c_str()];
      if (memberFilter.allow()) {
        auto member = object.getMember(adaptString(key.c_str()), resources_);
        if (!member) {
          auto savedKey = stringBuilder_.save();
          member = object.addMember(savedKey, resources_);
          if (!member)
            return DeserializationError::NoMemory;
        } else {
          member->clear(resources_);
        }
        err = parseVariant(*member, memberFilter, nestingLimit.decrement());
        if (err)
          return err;
      } else {
        err = skipVariant(nestingLimit.decrement());
        if (err)
          return err;
      }
      err = skipSpacesAndComments();
      if (err)
        return err;
      if (eat('}'))
        return DeserializationError::Ok;
      if (!eat(','))
        return DeserializationError::InvalidInput;
      err = skipSpacesAndComments();
      if (err)
        return err;
    }
  }
  DeserializationError::Code skipObject(
      DeserializationOption::NestingLimit nestingLimit) {
    DeserializationError::Code err;
    if (nestingLimit.reached())
      return DeserializationError::TooDeep;
    ARDUINOJSON_ASSERT(current() == '{');
    move();
    err = skipSpacesAndComments();
    if (err)
      return err;
    if (eat('}'))
      return DeserializationError::Ok;
    for (;;) {
      err = skipKey();
      if (err)
        return err;
      err = skipSpacesAndComments();
      if (err)
        return err;
      if (!eat(':'))
        return DeserializationError::InvalidInput;
      err = skipVariant(nestingLimit.decrement());
      if (err)
        return err;
      err = skipSpacesAndComments();
      if (err)
        return err;
      if (eat('}'))
        return DeserializationError::Ok;
      if (!eat(','))
        return DeserializationError::InvalidInput;
      err = skipSpacesAndComments();
      if (err)
        return err;
    }
  }
  DeserializationError::Code parseKey() {
    stringBuilder_.startString();
    if (isQuote(current())) {
      return parseQuotedString();
    } else {
      return parseNonQuotedString();
    }
  }
  DeserializationError::Code parseStringValue(VariantData& variant) {
    DeserializationError::Code err;
    stringBuilder_.startString();
    err = parseQuotedString();
    if (err)
      return err;
    variant.setOwnedString(stringBuilder_.save());
    return DeserializationError::Ok;
  }
  DeserializationError::Code parseQuotedString() {
#if ARDUINOJSON_DECODE_UNICODE
    Utf16::Codepoint codepoint;
    DeserializationError::Code err;
#endif
    const char stopChar = current();
    move();
    for (;;) {
      char c = current();
      move();
      if (c == stopChar)
        break;
      if (c == '\0')
        return DeserializationError::IncompleteInput;
      if (c == '\\') {
        c = current();
        if (c == '\0')
          return DeserializationError::IncompleteInput;
        if (c == 'u') {
#if ARDUINOJSON_DECODE_UNICODE
          move();
          uint16_t codeunit;
          err = parseHex4(codeunit);
          if (err)
            return err;
          if (codepoint.append(codeunit))
            Utf8::encodeCodepoint(codepoint.value(), stringBuilder_);
#else
          stringBuilder_.append('\\');
#endif
          continue;
        }
        c = EscapeSequence::unescapeChar(c);
        if (c == '\0')
          return DeserializationError::InvalidInput;
        move();
      }
      stringBuilder_.append(c);
    }
    if (!stringBuilder_.isValid())
      return DeserializationError::NoMemory;
    return DeserializationError::Ok;
  }
  DeserializationError::Code parseNonQuotedString() {
    char c = current();
    ARDUINOJSON_ASSERT(c);
    if (canBeInNonQuotedString(c)) {  // no quotes
      do {
        move();
        stringBuilder_.append(c);
        c = current();
      } while (canBeInNonQuotedString(c));
    } else {
      return DeserializationError::InvalidInput;
    }
    if (!stringBuilder_.isValid())
      return DeserializationError::NoMemory;
    return DeserializationError::Ok;
  }
  DeserializationError::Code skipKey() {
    if (isQuote(current())) {
      return skipQuotedString();
    } else {
      return skipNonQuotedString();
    }
  }
  DeserializationError::Code skipQuotedString() {
    const char stopChar = current();
    move();
    for (;;) {
      char c = current();
      move();
      if (c == stopChar)
        break;
      if (c == '\0')
        return DeserializationError::IncompleteInput;
      if (c == '\\') {
        if (current() != '\0')
          move();
      }
    }
    return DeserializationError::Ok;
  }
  DeserializationError::Code skipNonQuotedString() {
    char c = current();
    while (canBeInNonQuotedString(c)) {
      move();
      c = current();
    }
    return DeserializationError::Ok;
  }
  DeserializationError::Code parseNumericValue(VariantData& result) {
    uint8_t n = 0;
    char c = current();
    while (canBeInNumber(c) && n < 63) {
      move();
      buffer_[n++] = c;
      c = current();
    }
    buffer_[n] = 0;
    auto number = parseNumber(buffer_);
    switch (number.type()) {
      case NumberType::UnsignedInteger:
        if (result.setInteger(number.asUnsignedInteger(), resources_))
          return DeserializationError::Ok;
        else
          return DeserializationError::NoMemory;
      case NumberType::SignedInteger:
        if (result.setInteger(number.asSignedInteger(), resources_))
          return DeserializationError::Ok;
        else
          return DeserializationError::NoMemory;
      case NumberType::Float:
        if (result.setFloat(number.asFloat(), resources_))
          return DeserializationError::Ok;
        else
          return DeserializationError::NoMemory;
#if ARDUINOJSON_USE_DOUBLE
      case NumberType::Double:
        if (result.setFloat(number.asDouble(), resources_))
          return DeserializationError::Ok;
        else
          return DeserializationError::NoMemory;
#endif
      default:
        return DeserializationError::InvalidInput;
    }
  }
  DeserializationError::Code skipNumericValue() {
    char c = current();
    while (canBeInNumber(c)) {
      move();
      c = current();
    }
    return DeserializationError::Ok;
  }
  DeserializationError::Code parseHex4(uint16_t& result) {
    result = 0;
    for (uint8_t i = 0; i < 4; ++i) {
      char digit = current();
      if (!digit)
        return DeserializationError::IncompleteInput;
      uint8_t value = decodeHex(digit);
      if (value > 0x0F)
        return DeserializationError::InvalidInput;
      result = uint16_t((result << 4) | value);
      move();
    }
    return DeserializationError::Ok;
  }
  static inline bool isBetween(char c, char min, char max) {
    return min <= c && c <= max;
  }
  static inline bool canBeInNumber(char c) {
    return isBetween(c, '0', '9') || c == '+' || c == '-' || c == '.' ||
#if ARDUINOJSON_ENABLE_NAN || ARDUINOJSON_ENABLE_INFINITY
           isBetween(c, 'A', 'Z') || isBetween(c, 'a', 'z');
#else
           c == 'e' || c == 'E';
#endif
  }
  static inline bool canBeInNonQuotedString(char c) {
    return isBetween(c, '0', '9') || isBetween(c, '_', 'z') ||
           isBetween(c, 'A', 'Z');
  }
  static inline bool isQuote(char c) {
    return c == '\'' || c == '\"';
  }
  static inline uint8_t decodeHex(char c) {
    if (c < 'A')
      return uint8_t(c - '0');
    c = char(c & ~0x20);  // uppercase
    return uint8_t(c - 'A' + 10);
  }
  DeserializationError::Code skipSpacesAndComments() {
    for (;;) {
      switch (current()) {
        case '\0':
          return foundSomething_ ? DeserializationError::IncompleteInput
                                 : DeserializationError::EmptyInput;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
          move();
          continue;
#if ARDUINOJSON_ENABLE_COMMENTS
        case '/':
          move();  // skip '/'
          switch (current()) {
            case '*': {
              move();  // skip '*'
              bool wasStar = false;
              for (;;) {
                char c = current();
                if (c == '\0')
                  return DeserializationError::IncompleteInput;
                if (c == '/' && wasStar) {
                  move();
                  break;
                }
                wasStar = c == '*';
                move();
              }
              break;
            }
            case '/':
              for (;;) {
                move();
                char c = current();
                if (c == '\0')
                  return DeserializationError::IncompleteInput;
                if (c == '\n')
                  break;
              }
              break;
            default:
              return DeserializationError::InvalidInput;
          }
          break;
#endif
        default:
          foundSomething_ = true;
          return DeserializationError::Ok;
      }
    }
  }
  DeserializationError::Code skipKeyword(const char* s) {
    while (*s) {
      char c = current();
      if (c == '\0')
        return DeserializationError::IncompleteInput;
      if (*s != c)
        return DeserializationError::InvalidInput;
      ++s;
      move();
    }
    return DeserializationError::Ok;
  }
  StringBuilder stringBuilder_;
  bool foundSomething_;
  Latch<TReader> latch_;
  ResourceManager* resources_;
  char buffer_[64];  // using a member instead of a local variable because it
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
template <typename TDestination, typename... Args>
detail::enable_if_t<detail::is_deserialize_destination<TDestination>::value,
                    DeserializationError>
deserializeJson(TDestination&& dst, Args&&... args) {
  using namespace detail;
  return deserialize<JsonDeserializer>(detail::forward<TDestination>(dst),
                                       detail::forward<Args>(args)...);
}
template <typename TDestination, typename TChar, typename... Args>
detail::enable_if_t<detail::is_deserialize_destination<TDestination>::value,
                    DeserializationError>
deserializeJson(TDestination&& dst, TChar* input, Args&&... args) {
  using namespace detail;
  return deserialize<JsonDeserializer>(detail::forward<TDestination>(dst),
                                       input, detail::forward<Args>(args)...);
}
ARDUINOJSON_END_PUBLIC_NAMESPACE
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename TWriter>
class PrettyJsonSerializer : public JsonSerializer<TWriter> {
  typedef JsonSerializer<TWriter> base;
 public:
  PrettyJsonSerializer(TWriter writer, const ResourceManager* resources)
      : base(writer, resources), nesting_(0) {}
  size_t visit(const ArrayData& array) {
    auto it = array.createIterator(base::resources_);
    if (!it.done()) {
      base::write("[\r\n");
      nesting_++;
      while (!it.done()) {
        indent();
        it->accept(*this, base::resources_);
        it.next(base::resources_);
        base::write(it.done() ? "\r\n" : ",\r\n");
      }
      nesting_--;
      indent();
      base::write("]");
    } else {
      base::write("[]");
    }
    return this->bytesWritten();
  }
  size_t visit(const ObjectData& object) {
    auto it = object.createIterator(base::resources_);
    if (!it.done()) {
      base::write("{\r\n");
      nesting_++;
      bool isKey = true;
      while (!it.done()) {
        if (isKey)
          indent();
        it->accept(*this, base::resources_);
        it.next(base::resources_);
        if (isKey)
          base::write(": ");
        else
          base::write(it.done() ? "\r\n" : ",\r\n");
        isKey = !isKey;
      }
      nesting_--;
      indent();
      base::write("}");
    } else {
      base::write("{}");
    }
    return this->bytesWritten();
  }
  using base::visit;
 private:
  void indent() {
    for (uint8_t i = 0; i < nesting_; i++)
      base::write(ARDUINOJSON_TAB);
  }
  uint8_t nesting_;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
template <typename TDestination>
detail::enable_if_t<!detail::is_pointer<TDestination>::value, size_t>
serializeJsonPretty(JsonVariantConst source, TDestination& destination) {
  using namespace FLArduinoJson::detail;
  return serialize<PrettyJsonSerializer>(source, destination);
}
inline size_t serializeJsonPretty(JsonVariantConst source, void* buffer,
                                  size_t bufferSize) {
  using namespace FLArduinoJson::detail;
  return serialize<PrettyJsonSerializer>(source, buffer, bufferSize);
}
inline size_t measureJsonPretty(JsonVariantConst source) {
  using namespace FLArduinoJson::detail;
  return measure<PrettyJsonSerializer>(source);
}
class MsgPackBinary {
 public:
  MsgPackBinary() : data_(nullptr), size_(0) {}
  explicit MsgPackBinary(const void* c, size_t size) : data_(c), size_(size) {}
  const void* data() const {
    return data_;
  }
  size_t size() const {
    return size_;
  }
 private:
  const void* data_;
  size_t size_;
};
template <>
struct Converter<MsgPackBinary> : private detail::VariantAttorney {
  static void toJson(MsgPackBinary src, JsonVariant dst) {
    auto data = VariantAttorney::getData(dst);
    if (!data)
      return;
    auto resources = getResourceManager(dst);
    data->clear(resources);
    if (src.data()) {
      size_t headerSize = src.size() >= 0x10000 ? 5
                          : src.size() >= 0x100 ? 3
                                                : 2;
      auto str = resources->createString(src.size() + headerSize);
      if (str) {
        resources->saveString(str);
        auto ptr = reinterpret_cast<uint8_t*>(str->data);
        switch (headerSize) {
          case 2:
            ptr[0] = uint8_t(0xc4);
            ptr[1] = uint8_t(src.size() & 0xff);
            break;
          case 3:
            ptr[0] = uint8_t(0xc5);
            ptr[1] = uint8_t(src.size() >> 8 & 0xff);
            ptr[2] = uint8_t(src.size() & 0xff);
            break;
          case 5:
            ptr[0] = uint8_t(0xc6);
            ptr[1] = uint8_t(src.size() >> 24 & 0xff);
            ptr[2] = uint8_t(src.size() >> 16 & 0xff);
            ptr[3] = uint8_t(src.size() >> 8 & 0xff);
            ptr[4] = uint8_t(src.size() & 0xff);
            break;
          default:
            ARDUINOJSON_ASSERT(false);
        }
        memcpy(ptr + headerSize, src.data(), src.size());
        data->setRawString(str);
        return;
      }
    }
  }
  static MsgPackBinary fromJson(JsonVariantConst src) {
    auto data = getData(src);
    if (!data)
      return {};
    auto rawstr = data->asRawString();
    auto p = reinterpret_cast<const uint8_t*>(rawstr.c_str());
    auto n = rawstr.size();
    if (n >= 2 && p[0] == 0xc4) {  // bin 8
      size_t size = p[1];
      if (size + 2 == n)
        return MsgPackBinary(p + 2, size);
    } else if (n >= 3 && p[0] == 0xc5) {  // bin 16
      size_t size = size_t(p[1] << 8) | p[2];
      if (size + 3 == n)
        return MsgPackBinary(p + 3, size);
    } else if (n >= 5 && p[0] == 0xc6) {  // bin 32
      size_t size =
          size_t(p[1] << 24) | size_t(p[2] << 16) | size_t(p[3] << 8) | p[4];
      if (size + 5 == n)
        return MsgPackBinary(p + 5, size);
    }
    return {};
  }
  static bool checkJson(JsonVariantConst src) {
    return fromJson(src).data() != nullptr;
  }
};
ARDUINOJSON_END_PUBLIC_NAMESPACE
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
class StringBuffer {
 public:
  StringBuffer(ResourceManager* resources) : resources_(resources) {}
  ~StringBuffer() {
    if (node_)
      resources_->destroyString(node_);
  }
  char* reserve(size_t capacity) {
    if (node_ && capacity > node_->length) {
      resources_->destroyString(node_);
      node_ = nullptr;
    }
    if (!node_)
      node_ = resources_->createString(capacity);
    if (!node_)
      return nullptr;
    size_ = capacity;
    node_->data[capacity] = 0;  // null-terminate the string
    return node_->data;
  }
  StringNode* save() {
    ARDUINOJSON_ASSERT(node_ != nullptr);
    node_->data[size_] = 0;
    auto node = resources_->getString(adaptString(node_->data, size_));
    if (node) {
      node->references++;
      return node;
    }
    if (node_->length != size_) {
      node = resources_->resizeString(node_, size_);
      ARDUINOJSON_ASSERT(node != nullptr);  // realloc to smaller can't fail
    } else {
      node = node_;
    }
    node_ = nullptr;
    resources_->saveString(node);
    return node;
  }
  JsonString str() const {
    ARDUINOJSON_ASSERT(node_ != nullptr);
    return JsonString(node_->data, node_->length, JsonString::Copied);
  }
 private:
  ResourceManager* resources_;
  StringNode* node_ = nullptr;
  size_t size_ = 0;
};
#if ARDUINOJSON_LITTLE_ENDIAN
inline void swapBytes(uint8_t& a, uint8_t& b) {
  uint8_t t(a);
  a = b;
  b = t;
}
inline void fixEndianness(uint8_t* p, integral_constant<size_t, 8>) {
  swapBytes(p[0], p[7]);
  swapBytes(p[1], p[6]);
  swapBytes(p[2], p[5]);
  swapBytes(p[3], p[4]);
}
inline void fixEndianness(uint8_t* p, integral_constant<size_t, 4>) {
  swapBytes(p[0], p[3]);
  swapBytes(p[1], p[2]);
}
inline void fixEndianness(uint8_t* p, integral_constant<size_t, 2>) {
  swapBytes(p[0], p[1]);
}
inline void fixEndianness(uint8_t*, integral_constant<size_t, 1>) {}
template <typename T>
inline void fixEndianness(T& value) {
  fixEndianness(reinterpret_cast<uint8_t*>(&value),
                integral_constant<size_t, sizeof(T)>());
}
#else
template <typename T>
inline void fixEndianness(T&) {}
#endif
inline void doubleToFloat(const uint8_t d[8], uint8_t f[4]) {
  f[0] = uint8_t((d[0] & 0xC0) | (d[0] << 3 & 0x3f) | (d[1] >> 5));
  f[1] = uint8_t((d[1] << 3) | (d[2] >> 5));
  f[2] = uint8_t((d[2] << 3) | (d[3] >> 5));
  f[3] = uint8_t((d[3] << 3) | (d[4] >> 5));
}
template <typename TReader>
class MsgPackDeserializer {
 public:
  MsgPackDeserializer(ResourceManager* resources, TReader reader)
      : resources_(resources),
        reader_(reader),
        stringBuffer_(resources),
        foundSomething_(false) {}
  template <typename TFilter>
  DeserializationError parse(VariantData& variant, TFilter filter,
                             DeserializationOption::NestingLimit nestingLimit) {
    DeserializationError::Code err;
    err = parseVariant(&variant, filter, nestingLimit);
    return foundSomething_ ? err : DeserializationError::EmptyInput;
  }
 private:
  template <typename TFilter>
  DeserializationError::Code parseVariant(
      VariantData* variant, TFilter filter,
      DeserializationOption::NestingLimit nestingLimit) {
    DeserializationError::Code err;
    uint8_t header[5];
    err = readBytes(header, 1);
    if (err)
      return err;
    const uint8_t& code = header[0];
    foundSomething_ = true;
    bool allowValue = filter.allowValue();
    if (allowValue) {
      ARDUINOJSON_ASSERT(variant != 0);
    }
    if (code >= 0xcc && code <= 0xd3) {
      auto width = uint8_t(1U << ((code - 0xcc) % 4));
      if (allowValue)
        return readInteger(variant, width, code >= 0xd0);
      else
        return skipBytes(width);
    }
    switch (code) {
      case 0xc0:
        return DeserializationError::Ok;
      case 0xc1:
        return DeserializationError::InvalidInput;
      case 0xc2:
      case 0xc3:
        if (allowValue)
          variant->setBoolean(code == 0xc3);
        return DeserializationError::Ok;
      case 0xca:
        if (allowValue)
          return readFloat<float>(variant);
        else
          return skipBytes(4);
      case 0xcb:
        if (allowValue)
          return readDouble<double>(variant);
        else
          return skipBytes(8);
    }
    if (code <= 0x7f || code >= 0xe0) {  // fixint
      if (allowValue)
        variant->setInteger(static_cast<int8_t>(code), resources_);
      return DeserializationError::Ok;
    }
    uint8_t sizeBytes = 0;
    size_t size = 0;
    bool isExtension = code >= 0xc7 && code <= 0xc9;
    switch (code) {
      case 0xc4:  // bin 8
      case 0xc7:  // ext 8
      case 0xd9:  // str 8
        sizeBytes = 1;
        break;
      case 0xc5:  // bin 16
      case 0xc8:  // ext 16
      case 0xda:  // str 16
      case 0xdc:  // array 16
      case 0xde:  // map 16
        sizeBytes = 2;
        break;
      case 0xc6:  // bin 32
      case 0xc9:  // ext 32
      case 0xdb:  // str 32
      case 0xdd:  // array 32
      case 0xdf:  // map 32
        sizeBytes = 4;
        break;
    }
    if (code >= 0xd4 && code <= 0xd8) {  // fixext
      size = size_t(1) << (code - 0xd4);
      isExtension = true;
    }
    switch (code & 0xf0) {
      case 0x90:  // fixarray
      case 0x80:  // fixmap
        size = code & 0x0F;
        break;
    }
    switch (code & 0xe0) {
      case 0xa0:  // fixstr
        size = code & 0x1f;
        break;
    }
    if (sizeBytes) {
      err = readBytes(header + 1, sizeBytes);
      if (err)
        return err;
      uint32_t size32 = 0;
      for (uint8_t i = 0; i < sizeBytes; i++)
        size32 = (size32 << 8) | header[i + 1];
      size = size_t(size32);
      if (size < size32)                        // integer overflow
        return DeserializationError::NoMemory;  // (not testable on 32/64-bit)
    }
    if (code == 0xdc || code == 0xdd || (code & 0xf0) == 0x90)
      return readArray(variant, size, filter, nestingLimit);
    if (code == 0xde || code == 0xdf || (code & 0xf0) == 0x80)
      return readObject(variant, size, filter, nestingLimit);
    if (code == 0xd9 || code == 0xda || code == 0xdb || (code & 0xe0) == 0xa0) {
      if (allowValue)
        return readString(variant, size);
      else
        return skipBytes(size);
    }
    if (isExtension)
      size++;  // to include the type
    if (allowValue)
      return readRawString(variant, header, uint8_t(1 + sizeBytes), size);
    else
      return skipBytes(size);
  }
  DeserializationError::Code readByte(uint8_t& value) {
    int c = reader_.read();
    if (c < 0)
      return DeserializationError::IncompleteInput;
    value = static_cast<uint8_t>(c);
    return DeserializationError::Ok;
  }
  DeserializationError::Code readBytes(void* p, size_t n) {
    if (reader_.readBytes(reinterpret_cast<char*>(p), n) == n)
      return DeserializationError::Ok;
    return DeserializationError::IncompleteInput;
  }
  template <typename T>
  DeserializationError::Code readBytes(T& value) {
    return readBytes(&value, sizeof(value));
  }
  DeserializationError::Code skipBytes(size_t n) {
    for (; n; --n) {
      if (reader_.read() < 0)
        return DeserializationError::IncompleteInput;
    }
    return DeserializationError::Ok;
  }
  DeserializationError::Code readInteger(VariantData* variant, uint8_t width,
                                         bool isSigned) {
    uint8_t buffer[8];
    auto err = readBytes(buffer, width);
    if (err)
      return err;
    union {
      int64_t signedValue;
      uint64_t unsignedValue;
    };
    if (isSigned)
      signedValue = static_cast<int8_t>(buffer[0]);  // propagate sign bit
    else
      unsignedValue = static_cast<uint8_t>(buffer[0]);
    for (uint8_t i = 1; i < width; i++)
      unsignedValue = (unsignedValue << 8) | buffer[i];
    if (isSigned) {
      auto truncatedValue = static_cast<JsonInteger>(signedValue);
      if (truncatedValue == signedValue) {
        if (!variant->setInteger(truncatedValue, resources_))
          return DeserializationError::NoMemory;
      }
    } else {
      auto truncatedValue = static_cast<JsonUInt>(unsignedValue);
      if (truncatedValue == unsignedValue)
        if (!variant->setInteger(truncatedValue, resources_))
          return DeserializationError::NoMemory;
    }
    return DeserializationError::Ok;
  }
  template <typename T>
  enable_if_t<sizeof(T) == 4, DeserializationError::Code> readFloat(
      VariantData* variant) {
    DeserializationError::Code err;
    T value;
    err = readBytes(value);
    if (err)
      return err;
    fixEndianness(value);
    variant->setFloat(value, resources_);
    return DeserializationError::Ok;
  }
  template <typename T>
  enable_if_t<sizeof(T) == 8, DeserializationError::Code> readDouble(
      VariantData* variant) {
    DeserializationError::Code err;
    T value;
    err = readBytes(value);
    if (err)
      return err;
    fixEndianness(value);
    if (variant->setFloat(value, resources_))
      return DeserializationError::Ok;
    else
      return DeserializationError::NoMemory;
  }
  template <typename T>
  enable_if_t<sizeof(T) == 4, DeserializationError::Code> readDouble(
      VariantData* variant) {
    DeserializationError::Code err;
    uint8_t i[8];  // input is 8 bytes
    T value;       // output is 4 bytes
    uint8_t* o = reinterpret_cast<uint8_t*>(&value);
    err = readBytes(i, 8);
    if (err)
      return err;
    doubleToFloat(i, o);
    fixEndianness(value);
    variant->setFloat(value, resources_);
    return DeserializationError::Ok;
  }
  DeserializationError::Code readString(VariantData* variant, size_t n) {
    DeserializationError::Code err;
    err = readString(n);
    if (err)
      return err;
    variant->setOwnedString(stringBuffer_.save());
    return DeserializationError::Ok;
  }
  DeserializationError::Code readString(size_t n) {
    char* p = stringBuffer_.reserve(n);
    if (!p)
      return DeserializationError::NoMemory;
    return readBytes(p, n);
  }
  DeserializationError::Code readRawString(VariantData* variant,
                                           const void* header,
                                           uint8_t headerSize, size_t n) {
    auto totalSize = size_t(headerSize + n);
    if (totalSize < n)                        // integer overflow
      return DeserializationError::NoMemory;  // (not testable on 64-bit)
    char* p = stringBuffer_.reserve(totalSize);
    if (!p)
      return DeserializationError::NoMemory;
    memcpy(p, header, headerSize);
    auto err = readBytes(p + headerSize, n);
    if (err)
      return err;
    variant->setRawString(stringBuffer_.save());
    return DeserializationError::Ok;
  }
  template <typename TFilter>
  DeserializationError::Code readArray(
      VariantData* variant, size_t n, TFilter filter,
      DeserializationOption::NestingLimit nestingLimit) {
    DeserializationError::Code err;
    if (nestingLimit.reached())
      return DeserializationError::TooDeep;
    bool allowArray = filter.allowArray();
    ArrayData* array;
    if (allowArray) {
      ARDUINOJSON_ASSERT(variant != 0);
      array = &variant->toArray();
    } else {
      array = 0;
    }
    TFilter elementFilter = filter[0U];
    for (; n; --n) {
      VariantData* value;
      if (elementFilter.allow()) {
        ARDUINOJSON_ASSERT(array != 0);
        value = array->addElement(resources_);
        if (!value)
          return DeserializationError::NoMemory;
      } else {
        value = 0;
      }
      err = parseVariant(value, elementFilter, nestingLimit.decrement());
      if (err)
        return err;
    }
    return DeserializationError::Ok;
  }
  template <typename TFilter>
  DeserializationError::Code readObject(
      VariantData* variant, size_t n, TFilter filter,
      DeserializationOption::NestingLimit nestingLimit) {
    DeserializationError::Code err;
    if (nestingLimit.reached())
      return DeserializationError::TooDeep;
    ObjectData* object;
    if (filter.allowObject()) {
      ARDUINOJSON_ASSERT(variant != 0);
      object = &variant->toObject();
    } else {
      object = 0;
    }
    for (; n; --n) {
      err = readKey();
      if (err)
        return err;
      JsonString key = stringBuffer_.str();
      TFilter memberFilter = filter[key.c_str()];
      VariantData* member;
      if (memberFilter.allow()) {
        ARDUINOJSON_ASSERT(object != 0);
        auto savedKey = stringBuffer_.save();
        member = object->addMember(savedKey, resources_);
        if (!member)
          return DeserializationError::NoMemory;
      } else {
        member = 0;
      }
      err = parseVariant(member, memberFilter, nestingLimit.decrement());
      if (err)
        return err;
    }
    return DeserializationError::Ok;
  }
  DeserializationError::Code readKey() {
    DeserializationError::Code err;
    uint8_t code;
    err = readByte(code);
    if (err)
      return err;
    if ((code & 0xe0) == 0xa0)
      return readString(code & 0x1f);
    if (code >= 0xd9 && code <= 0xdb) {
      uint8_t sizeBytes = uint8_t(1U << (code - 0xd9));
      uint32_t size = 0;
      for (uint8_t i = 0; i < sizeBytes; i++) {
        err = readByte(code);
        if (err)
          return err;
        size = (size << 8) | code;
      }
      return readString(size);
    }
    return DeserializationError::InvalidInput;
  }
  ResourceManager* resources_;
  TReader reader_;
  StringBuffer stringBuffer_;
  bool foundSomething_;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
template <typename TDestination, typename... Args>
detail::enable_if_t<detail::is_deserialize_destination<TDestination>::value,
                    DeserializationError>
deserializeMsgPack(TDestination&& dst, Args&&... args) {
  using namespace detail;
  return deserialize<MsgPackDeserializer>(detail::forward<TDestination>(dst),
                                          detail::forward<Args>(args)...);
}
template <typename TDestination, typename TChar, typename... Args>
detail::enable_if_t<detail::is_deserialize_destination<TDestination>::value,
                    DeserializationError>
deserializeMsgPack(TDestination&& dst, TChar* input, Args&&... args) {
  using namespace detail;
  return deserialize<MsgPackDeserializer>(detail::forward<TDestination>(dst),
                                          input,
                                          detail::forward<Args>(args)...);
}
class MsgPackExtension {
 public:
  MsgPackExtension() : data_(nullptr), size_(0), type_(0) {}
  explicit MsgPackExtension(int8_t type, const void* data, size_t size)
      : data_(data), size_(size), type_(type) {}
  int8_t type() const {
    return type_;
  }
  const void* data() const {
    return data_;
  }
  size_t size() const {
    return size_;
  }
 private:
  const void* data_;
  size_t size_;
  int8_t type_;
};
template <>
struct Converter<MsgPackExtension> : private detail::VariantAttorney {
  static void toJson(MsgPackExtension src, JsonVariant dst) {
    auto data = VariantAttorney::getData(dst);
    if (!data)
      return;
    auto resources = getResourceManager(dst);
    data->clear(resources);
    if (src.data()) {
      uint8_t format, sizeBytes;
      if (src.size() >= 0x10000) {
        format = 0xc9;  // ext 32
        sizeBytes = 4;
      } else if (src.size() >= 0x100) {
        format = 0xc8;  // ext 16
        sizeBytes = 2;
      } else if (src.size() == 16) {
        format = 0xd8;  // fixext 16
        sizeBytes = 0;
      } else if (src.size() == 8) {
        format = 0xd7;  // fixext 8
        sizeBytes = 0;
      } else if (src.size() == 4) {
        format = 0xd6;  // fixext 4
        sizeBytes = 0;
      } else if (src.size() == 2) {
        format = 0xd5;  // fixext 2
        sizeBytes = 0;
      } else if (src.size() == 1) {
        format = 0xd4;  // fixext 1
        sizeBytes = 0;
      } else {
        format = 0xc7;  // ext 8
        sizeBytes = 1;
      }
      auto str = resources->createString(src.size() + 2 + sizeBytes);
      if (str) {
        resources->saveString(str);
        auto ptr = reinterpret_cast<uint8_t*>(str->data);
        *ptr++ = uint8_t(format);
        for (uint8_t i = 0; i < sizeBytes; i++)
          *ptr++ = uint8_t(src.size() >> (sizeBytes - i - 1) * 8 & 0xff);
        *ptr++ = uint8_t(src.type());
        memcpy(ptr, src.data(), src.size());
        data->setRawString(str);
        return;
      }
    }
  }
  static MsgPackExtension fromJson(JsonVariantConst src) {
    auto data = getData(src);
    if (!data)
      return {};
    auto rawstr = data->asRawString();
    if (rawstr.size() == 0)
      return {};
    auto p = reinterpret_cast<const uint8_t*>(rawstr.c_str());
    size_t payloadSize = 0;
    uint8_t headerSize = 0;
    const uint8_t& code = p[0];
    if (code >= 0xd4 && code <= 0xd8) {  // fixext 1
      headerSize = 2;
      payloadSize = size_t(1) << (code - 0xd4);
    }
    if (code >= 0xc7 && code <= 0xc9) {
      uint8_t sizeBytes = uint8_t(1 << (code - 0xc7));
      for (uint8_t i = 0; i < sizeBytes; i++)
        payloadSize = (payloadSize << 8) | p[1 + i];
      headerSize = uint8_t(2 + sizeBytes);
    }
    if (rawstr.size() == headerSize + payloadSize)
      return MsgPackExtension(int8_t(p[headerSize - 1]), p + headerSize,
                              payloadSize);
    return {};
  }
  static bool checkJson(JsonVariantConst src) {
    return fromJson(src).data() != nullptr;
  }
};
ARDUINOJSON_END_PUBLIC_NAMESPACE
ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE
template <typename TWriter>
class MsgPackSerializer : public VariantDataVisitor<size_t> {
 public:
  static const bool producesText = false;
  MsgPackSerializer(TWriter writer, const ResourceManager* resources)
      : writer_(writer), resources_(resources) {}
  template <typename T>
  enable_if_t<is_floating_point<T>::value && sizeof(T) == 4, size_t> visit(
      T value32) {
    if (canConvertNumber<JsonInteger>(value32)) {
      JsonInteger truncatedValue = JsonInteger(value32);
      if (value32 == T(truncatedValue))
        return visit(truncatedValue);
    }
    writeByte(0xCA);
    writeInteger(value32);
    return bytesWritten();
  }
  template <typename T>
  ARDUINOJSON_NO_SANITIZE("float-cast-overflow")
  enable_if_t<is_floating_point<T>::value && sizeof(T) == 8, size_t> visit(
      T value64) {
    float value32 = float(value64);
    if (value32 == value64)
      return visit(value32);
    writeByte(0xCB);
    writeInteger(value64);
    return bytesWritten();
  }
  size_t visit(const ArrayData& array) {
    size_t n = array.size(resources_);
    if (n < 0x10) {
      writeByte(uint8_t(0x90 + n));
    } else if (n < 0x10000) {
      writeByte(0xDC);
      writeInteger(uint16_t(n));
    } else {
      writeByte(0xDD);
      writeInteger(uint32_t(n));
    }
    auto slotId = array.head();
    while (slotId != NULL_SLOT) {
      auto slot = resources_->getVariant(slotId);
      slot->accept(*this, resources_);
      slotId = slot->next();
    }
    return bytesWritten();
  }
  size_t visit(const ObjectData& object) {
    size_t n = object.size(resources_);
    if (n < 0x10) {
      writeByte(uint8_t(0x80 + n));
    } else if (n < 0x10000) {
      writeByte(0xDE);
      writeInteger(uint16_t(n));
    } else {
      writeByte(0xDF);
      writeInteger(uint32_t(n));
    }
    auto slotId = object.head();
    while (slotId != NULL_SLOT) {
      auto slot = resources_->getVariant(slotId);
      slot->accept(*this, resources_);
      slotId = slot->next();
    }
    return bytesWritten();
  }
  size_t visit(const char* value) {
    return visit(JsonString(value));
  }
  size_t visit(JsonString value) {
    ARDUINOJSON_ASSERT(value != NULL);
    auto n = value.size();
    if (n < 0x20) {
      writeByte(uint8_t(0xA0 + n));
    } else if (n < 0x100) {
      writeByte(0xD9);
      writeInteger(uint8_t(n));
    } else if (n < 0x10000) {
      writeByte(0xDA);
      writeInteger(uint16_t(n));
    } else {
      writeByte(0xDB);
      writeInteger(uint32_t(n));
    }
    writeBytes(reinterpret_cast<const uint8_t*>(value.c_str()), n);
    return bytesWritten();
  }
  size_t visit(RawString value) {
    writeBytes(reinterpret_cast<const uint8_t*>(value.data()), value.size());
    return bytesWritten();
  }
  size_t visit(JsonInteger value) {
    if (value > 0) {
      visit(static_cast<JsonUInt>(value));
    } else if (value >= -0x20) {
      writeInteger(int8_t(value));
    } else if (value >= -0x80) {
      writeByte(0xD0);
      writeInteger(int8_t(value));
    } else if (value >= -0x8000) {
      writeByte(0xD1);
      writeInteger(int16_t(value));
    }
#if ARDUINOJSON_USE_LONG_LONG
    else if (value >= -0x80000000LL)
#else
    else
#endif
    {
      writeByte(0xD2);
      writeInteger(int32_t(value));
    }
#if ARDUINOJSON_USE_LONG_LONG
    else {
      writeByte(0xD3);
      writeInteger(int64_t(value));
    }
#endif
    return bytesWritten();
  }
  size_t visit(JsonUInt value) {
    if (value <= 0x7F) {
      writeInteger(uint8_t(value));
    } else if (value <= 0xFF) {
      writeByte(0xCC);
      writeInteger(uint8_t(value));
    } else if (value <= 0xFFFF) {
      writeByte(0xCD);
      writeInteger(uint16_t(value));
    }
#if ARDUINOJSON_USE_LONG_LONG
    else if (value <= 0xFFFFFFFF)
#else
    else
#endif
    {
      writeByte(0xCE);
      writeInteger(uint32_t(value));
    }
#if ARDUINOJSON_USE_LONG_LONG
    else {
      writeByte(0xCF);
      writeInteger(uint64_t(value));
    }
#endif
    return bytesWritten();
  }
  size_t visit(bool value) {
    writeByte(value ? 0xC3 : 0xC2);
    return bytesWritten();
  }
  size_t visit(nullptr_t) {
    writeByte(0xC0);
    return bytesWritten();
  }
 private:
  size_t bytesWritten() const {
    return writer_.count();
  }
  void writeByte(uint8_t c) {
    writer_.write(c);
  }
  void writeBytes(const uint8_t* p, size_t n) {
    writer_.write(p, n);
  }
  template <typename T>
  void writeInteger(T value) {
    fixEndianness(value);
    writeBytes(reinterpret_cast<uint8_t*>(&value), sizeof(value));
  }
  CountingDecorator<TWriter> writer_;
  const ResourceManager* resources_;
};
ARDUINOJSON_END_PRIVATE_NAMESPACE
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
template <typename TDestination>
detail::enable_if_t<!detail::is_pointer<TDestination>::value, size_t>
serializeMsgPack(JsonVariantConst source, TDestination& output) {
  using namespace FLArduinoJson::detail;
  return serialize<MsgPackSerializer>(source, output);
}
inline size_t serializeMsgPack(JsonVariantConst source, void* output,
                               size_t size) {
  using namespace FLArduinoJson::detail;
  return serialize<MsgPackSerializer>(source, output, size);
}
inline size_t measureMsgPack(JsonVariantConst source) {
  using namespace FLArduinoJson::detail;
  return measure<MsgPackSerializer>(source);
}
ARDUINOJSON_END_PUBLIC_NAMESPACE
#ifdef ARDUINOJSON_SLOT_OFFSET_SIZE
#error ARDUINOJSON_SLOT_OFFSET_SIZE has been removed, use ARDUINOJSON_SLOT_ID_SIZE instead
#endif
#ifdef ARDUINOJSON_ENABLE_STRING_DEDUPLICATION
#warning "ARDUINOJSON_ENABLE_STRING_DEDUPLICATION has been removed, string deduplication is now always enabled"
#endif
#ifdef __GNUC__
#define ARDUINOJSON_PRAGMA(x) _Pragma(#x)
#define ARDUINOJSON_COMPILE_ERROR(msg) ARDUINOJSON_PRAGMA(GCC error msg)
#define ARDUINOJSON_STRINGIFY(S) #S
#define ARDUINOJSON_DEPRECATION_ERROR(X, Y) \
  ARDUINOJSON_COMPILE_ERROR(ARDUINOJSON_STRINGIFY(X is a Y from FLArduinoJson 5. Please see https:/\/arduinojson.org/v7/upgrade-from-v5/ to learn how to upgrade to FLArduinoJson 7))
#define StaticJsonBuffer ARDUINOJSON_DEPRECATION_ERROR(StaticJsonBuffer, class)
#define DynamicJsonBuffer ARDUINOJSON_DEPRECATION_ERROR(DynamicJsonBuffer, class)
#define JsonBuffer ARDUINOJSON_DEPRECATION_ERROR(JsonBuffer, class)
#define RawJson ARDUINOJSON_DEPRECATION_ERROR(RawJson, function)
#define ARDUINOJSON_NAMESPACE _Pragma ("GCC warning \"ARDUINOJSON_NAMESPACE is deprecated, use ArduinoJson instead\"") FLArduinoJson
#define JSON_ARRAY_SIZE(N) _Pragma ("GCC warning \"JSON_ARRAY_SIZE is deprecated, you don't need to compute the size anymore\"") (FLArduinoJson::detail::sizeofArray(N))
#define JSON_OBJECT_SIZE(N) _Pragma ("GCC warning \"JSON_OBJECT_SIZE is deprecated, you don't need to compute the size anymore\"") (FLArduinoJson::detail::sizeofObject(N))
#define JSON_STRING_SIZE(N) _Pragma ("GCC warning \"JSON_STRING_SIZE is deprecated, you don't need to compute the size anymore\"") (N+1)
#else
#define JSON_ARRAY_SIZE(N) (FLArduinoJson::detail::sizeofArray(N))
#define JSON_OBJECT_SIZE(N) (FLArduinoJson::detail::sizeofObject(N))
#define JSON_STRING_SIZE(N) (N+1)
#endif
ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE
template <size_t N>
class ARDUINOJSON_DEPRECATED("use JsonDocument instead") StaticJsonDocument
    : public JsonDocument {
 public:
  using JsonDocument::JsonDocument;
  size_t capacity() const {
    return N;
  }
};
namespace detail {
template <typename TAllocator>
class AllocatorAdapter : public Allocator {
 public:
  AllocatorAdapter(const AllocatorAdapter&) = delete;
  AllocatorAdapter& operator=(const AllocatorAdapter&) = delete;
  void* allocate(size_t size) override {
    return _allocator.allocate(size);
  }
  void deallocate(void* ptr) override {
    _allocator.deallocate(ptr);
  }
  void* reallocate(void* ptr, size_t new_size) override {
    return _allocator.reallocate(ptr, new_size);
  }
  static Allocator* instance() {
    static AllocatorAdapter instance;
    return &instance;
  }
 private:
  AllocatorAdapter() = default;
  ~AllocatorAdapter() = default;
  TAllocator _allocator;
};
}  // namespace detail
template <typename TAllocator>
class ARDUINOJSON_DEPRECATED("use JsonDocument instead") BasicJsonDocument
    : public JsonDocument {
 public:
  BasicJsonDocument(size_t capacity)
      : JsonDocument(detail::AllocatorAdapter<TAllocator>::instance()),
        _capacity(capacity) {}
  size_t capacity() const {
    return _capacity;
  }
  void garbageCollect() {}
 private:
  size_t _capacity;
};
class ARDUINOJSON_DEPRECATED("use JsonDocument instead") DynamicJsonDocument
    : public JsonDocument {
 public:
  DynamicJsonDocument(size_t capacity) : _capacity(capacity) {}
  size_t capacity() const {
    return _capacity;
  }
  void garbageCollect() {}
 private:
  size_t _capacity;
};
inline JsonObject JsonArray::createNestedObject() const {
  return add<JsonObject>();
}
ARDUINOJSON_END_PUBLIC_NAMESPACE

// using namespace FLArduinoJson;

#else

#error ArduinoJson requires a C++ compiler, please change file extension to .cc or .cpp

#endif
