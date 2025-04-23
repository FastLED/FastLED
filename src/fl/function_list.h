#pragma once
#include "fl/function.h"
#include "fl/vector.h"

namespace fl {


template<typename FunctionType>
class FunctionListBase {
protected:
    fl::vector<FunctionType> mFunctions;

public:
    FunctionListBase() = default;
    ~FunctionListBase() = default;

    void add(FunctionType function) {
        mFunctions.push_back(function);
    }

    void remove(FunctionType function) {
        mFunctions.erase(function);
    }

    void clear() {
        mFunctions.clear();
    }
};


template<typename... Args>
class FunctionList : public FunctionListBase<Function<void(Args...)>> {
    public:
    void invoke(Args... args) {
        for (const auto &function : this->mFunctions) {
            // function(std::forward<Args>(args)...);
            function(args...);
        }
    }

};

template<>
class FunctionList<void> : public FunctionListBase<Function<void()>> {
    public:
    void invoke() {
        for (const auto &function : this->mFunctions) {
            function();
        }
    }
};

} // namespace fl
