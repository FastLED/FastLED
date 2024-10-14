#pragma once

// A templated singleton class, parameterized by the type of the singleton and an optional integer.
template<typename T, int N = 0>
class Singleton {
public:
    static T& instance() {
        static char storage[sizeof(T)];
        static bool initialized = false;
        if (!initialized) {
            // Construct the object in-place using placement new
            new (storage) T();
            initialized = true;
        }
        return *reinterpret_cast<T*>(storage);
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
