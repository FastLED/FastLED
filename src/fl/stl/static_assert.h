#pragma once

/// @file static_assert.h
/// Portable compile-time assertion wrapper.

#if __cplusplus <= 199711L

namespace fl {
template <bool> struct static_assert_failure;
template <> struct static_assert_failure<true> {};
} // namespace fl

#define FL_STATIC_ASSERT_JOIN_IMPL(a, b) a##b
#define FL_STATIC_ASSERT_JOIN(a, b) FL_STATIC_ASSERT_JOIN_IMPL(a, b)
#ifdef __COUNTER__
#define FL_STATIC_ASSERT_NAME FL_STATIC_ASSERT_JOIN(fl_static_assert_, __COUNTER__)
#else
#define FL_STATIC_ASSERT_NAME FL_STATIC_ASSERT_JOIN(fl_static_assert_, __LINE__)
#endif
#define FL_STATIC_ASSERT_IMPL(expression)                                      \
    typedef char FL_STATIC_ASSERT_NAME[sizeof(                                \
        ::fl::static_assert_failure<(bool)(expression)>)]
#define FL_STATIC_ASSERT_SELECT(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10,     \
                                _11, _12, _13, _14, _15, _16, NAME, ...)     \
    NAME
#define FL_STATIC_ASSERT_EXPR_1(expression) expression
#define FL_STATIC_ASSERT_EXPR_2(expression, message) expression
#define FL_STATIC_ASSERT_EXPR_3(a, b, message) a, b
#define FL_STATIC_ASSERT_EXPR_4(a, b, c, message) a, b, c
#define FL_STATIC_ASSERT_EXPR_5(a, b, c, d, message) a, b, c, d
#define FL_STATIC_ASSERT_EXPR_6(a, b, c, d, e, message) a, b, c, d, e
#define FL_STATIC_ASSERT_EXPR_7(a, b, c, d, e, f, message) a, b, c, d, e, f
#define FL_STATIC_ASSERT_EXPR_8(a, b, c, d, e, f, g, message) a, b, c, d, e, \
    f, g
#define FL_STATIC_ASSERT_EXPR_9(a, b, c, d, e, f, g, h, message) a, b, c, d, \
    e, f, g, h
#define FL_STATIC_ASSERT_EXPR_10(a, b, c, d, e, f, g, h, i, message) a, b, c, \
    d, e, f, g, h, i
#define FL_STATIC_ASSERT_EXPR_11(a, b, c, d, e, f, g, h, i, j, message) a, b, \
    c, d, e, f, g, h, i, j
#define FL_STATIC_ASSERT_EXPR_12(a, b, c, d, e, f, g, h, i, j, k, message) a, \
    b, c, d, e, f, g, h, i, j, k
#define FL_STATIC_ASSERT_EXPR_13(a, b, c, d, e, f, g, h, i, j, k, l,         \
                                 message)                                     \
    a, b, c, d, e, f, g, h, i, j, k, l
#define FL_STATIC_ASSERT_EXPR_14(a, b, c, d, e, f, g, h, i, j, k, l, m,      \
                                 message)                                     \
    a, b, c, d, e, f, g, h, i, j, k, l, m
#define FL_STATIC_ASSERT_EXPR_15(a, b, c, d, e, f, g, h, i, j, k, l, m, n,   \
                                 message)                                     \
    a, b, c, d, e, f, g, h, i, j, k, l, m, n
#define FL_STATIC_ASSERT_EXPR_16(a, b, c, d, e, f, g, h, i, j, k, l, m, n,   \
                                 o, message)                                  \
    a, b, c, d, e, f, g, h, i, j, k, l, m, n, o
#define FL_STATIC_ASSERT_EXPR(...)                                            \
    FL_STATIC_ASSERT_SELECT(__VA_ARGS__, FL_STATIC_ASSERT_EXPR_16,            \
                            FL_STATIC_ASSERT_EXPR_15,                         \
                            FL_STATIC_ASSERT_EXPR_14,                         \
                            FL_STATIC_ASSERT_EXPR_13,                         \
                            FL_STATIC_ASSERT_EXPR_12,                         \
                            FL_STATIC_ASSERT_EXPR_11,                         \
                            FL_STATIC_ASSERT_EXPR_10,                         \
                            FL_STATIC_ASSERT_EXPR_9, FL_STATIC_ASSERT_EXPR_8, \
                            FL_STATIC_ASSERT_EXPR_7, FL_STATIC_ASSERT_EXPR_6, \
                            FL_STATIC_ASSERT_EXPR_5, FL_STATIC_ASSERT_EXPR_4, \
                            FL_STATIC_ASSERT_EXPR_3, FL_STATIC_ASSERT_EXPR_2, \
                            FL_STATIC_ASSERT_EXPR_1)(__VA_ARGS__)

// Template commas split macro arguments on old preprocessors. Drop the message
// argument, then rejoin the remaining tokens as the assertion expression.
#define FL_STATIC_ASSERT(...)                                                 \
    FL_STATIC_ASSERT_IMPL((FL_STATIC_ASSERT_EXPR(__VA_ARGS__)))

#else

#define FL_STATIC_ASSERT(...) static_assert(__VA_ARGS__)

#endif
