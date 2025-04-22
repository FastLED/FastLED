#pragma once
#include "fl/function.h"
#include "fl/vector.h"

namespace fl {

template<typename T>
class FunctionList {
private:
    fl::vector<Function<T>> mFunctions;

public:
    FunctionList() = default;
    ~FunctionList() = default;

    void add(Function<T> function) {
        mFunctions.push_back(function);
    }

    void remove(Function<T> function) {
        mFunctions.erase(function);
    }

    void clear() {
        mFunctions.clear();
    }

    template<typename... Args>
    void invoke(Args&&... args) {
        for (const auto &function : mFunctions) {
            // function(std::forward<Args>(args)...);
            function(args...);
        }
    }
};

} // namespace fl
