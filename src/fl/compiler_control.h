#pragma once

// Stringify helper
#define FL_STRINGIFY(x) #x
#define FL_DO_PRAGMA(x) _Pragma(#x)

#if defined(__clang__)
  #define FL_DISABLE_WARNING_PUSH         FL_DO_PRAGMA(clang diagnostic push)
  #define FL_DISABLE_WARNING_POP          FL_DO_PRAGMA(clang diagnostic pop)
  #define FL_DISABLE_WARNING(warning)     _Pragma(FL_STRINGIFY(clang diagnostic ignored warning))
#elif defined(__GNUC__)
  #define FL_DISABLE_WARNING_PUSH         FL_DO_PRAGMA(GCC diagnostic push)
  #define FL_DISABLE_WARNING_POP          FL_DO_PRAGMA(GCC diagnostic pop)
  #define FL_DISABLE_WARNING(warning)     _Pragma(FL_STRINGIFY(GCC diagnostic ignored warning))
#else
  #define FL_DISABLE_WARNING_PUSH
  #define FL_DISABLE_WARNING_POP
  #define FL_DISABLE_WARNING(warning)
#endif
