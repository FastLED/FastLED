// Provides coroutine implementations to all test DLLs via libtest_shared_objs.a.
// Separated from doctest_main.cpp so that .cpp files don't include .impl.cpp.hpp files.
// This file is excluded from test discovery via EXCLUDED_TEST_FILES in test_config.py.
#include "platforms/coroutine.impl.cpp.hpp"
