#pragma once
#include "fl/function.h"

namespace fl {

template<typename T>
class FunctionList {
public:
    FunctionList() = default;
    ~FunctionList() = default;

    void add(Function<T> function) {
        mFunctions.push_back(function);
    }

    void remove(Function<T> function) {
        auto it = std::remove(mFunctions.begin(), mFunctions.end(), function);
        if (it != mFunctions.end()) {
            mFunctions.erase(it, mFunctions.end());
        }
    }

    void clear() {
        mFunctions.clear();
    }

    void invoke() {
        for (const auto &function : mFunctions) {
            function();
        }
    }


} // namespace fl
