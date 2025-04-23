#pragma once
#include "fl/function.h"
#include "fl/vector.h"

namespace fl {

template<typename... Args>
class FunctionList {
private:
    using FunctionType = Function<void(Args...)>;
    fl::vector<FunctionType> mFunctions;

public:
    FunctionList() = default;
    ~FunctionList() = default;

    void add(FunctionType function) {
        mFunctions.push_back(function);
    }

    void remove(FunctionType function) {
        mFunctions.erase(function);
    }

    void clear() {
        mFunctions.clear();
    }
    void invoke(Args... args) {
        for (const auto &function : mFunctions) {
            // function(std::forward<Args>(args)...);
            function(args...);
        }
    }
};


class FunctionListVoid {
private:
    using FunctionType = Function<void()>;
    fl::vector<FunctionType> mFunctions;

public:
    FunctionListVoid() = default;
    ~FunctionListVoid() = default;

    void add(FunctionType function) {
        mFunctions.push_back(function);
    }

    void remove(FunctionType function) {
        mFunctions.erase(function);
    }

    void clear() {
        mFunctions.clear();
    }

    void invoke() {
        for (const auto &function : mFunctions) {
            // function(std::forward<Args>(args)...);
            function();
        }
    }
};

} // namespace fl
