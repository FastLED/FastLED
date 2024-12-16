#pragma once

#include "fl/namespace.h"

namespace fl {

/// @brief A void returning callback that can be used to call a function with a variable number of arguments. If this binds
///        to a non-static member function then self must be none null. If it's a free function then set self to
///        nullptr.
/// @tparam ...Args The types of the arguments to the function.
/// @example Callback<const char*> callback(this, [](void* self, const char* str) { static_cast<MyClass*>(self)->myFunction(str); });
template<typename ...Args>
class Callback {
public:
    Callback() = default;
    // Member function constructor.
    Callback(void* self, void (*callback)(void* self, Args... args)) : self(self), callback(callback) {}
    // Free function constructor.
    explicit Callback(void* (*callback)(Args... args)) : self(nullptr), callback((void (*)(void*, Args...))callback) {}
    Callback(const Callback&) = default;
    operator bool() const { return callback != nullptr; }
    void operator()(Args... args) const { if (callback) callback(self, args...); }
    void clear() { callback = nullptr; self = nullptr; }
    bool operator==(const Callback& other) const { return self == other.self && callback == other.callback; }
    bool operator!=(const Callback& other) const { return !(*this == other); }
    Callback& operator=(const Callback& other) { self = other.self; callback = other.callback; return *this; }
    Callback& operator=(void* (*other)(Args... args)) { self = nullptr; callback = (void (*)(void*, Args...))other; return *this; }
    Callback& operator=(void (*other)(void* self, Args... args)) { self = nullptr; callback = other; return *this; }
    // operator < for set/map
    bool operator<(const Callback& other) const { return self < other.self || (self == other.self && callback < other.callback); }
private:
    void* self = nullptr;
    void (*callback)(void* self, Args... args) = nullptr;
};

}  // namespace fl
