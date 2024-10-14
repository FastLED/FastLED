#pragma once
#include <new>

// A templated singleton class, parameterized by the type of the singleton and an optional integer.
template<typename T, int N = 0>
class Singleton {
public:
    static T& instance() {
        static char buffer[sizeof(T)];
        static bool initialized = false;
        if (!initialized) {
            // Use inplace new so that the destructor doesn't get called when the program exits.
            new (buffer) T();
            initialized = true;
        }
        return *reinterpret_cast<T*>(buffer);
    }
    static T* instancePtr() {
        return &instance();
    }
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
private:
    Singleton() = default;
    ~Singleton() = default;
};
