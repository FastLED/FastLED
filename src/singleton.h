#pragma once

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

// A templated singleton class, parameterized by the type of the singleton and an optional integer.
template<typename T, int N = 0>
class Singleton {
public:
    static T& instance() {
        static T instance;
        return instance;
    }
    static T* instanceRef() {
        return &instance();
    }
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
private:
    Singleton() = default;
    ~Singleton() = default;
};

FASTLED_NAMESPACE_END