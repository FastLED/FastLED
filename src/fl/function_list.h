#pragma once
#include "fl/function.h"
#include "fl/pair.h"
#include "fl/vector.h"

namespace fl {

template <typename FunctionType> class FunctionListBase {
  protected:
    fl::vector<pair<int, FunctionType>> mFunctions;
    int mCounter = 0;

  public:
    FunctionListBase() = default;
    ~FunctionListBase() = default;

    int add(FunctionType function) {
        int id = mCounter++;
        pair<int, FunctionType> entry(id, function);
        mFunctions.push_back(entry);
        return id;
    }

    void remove(int id) {
        for (int i = mFunctions.size() - 1; i >= 0; --i) {
            if (mFunctions[i].first == id) {
                mFunctions.erase(mFunctions.begin() + i);
            }
        }
    }

    void clear() { mFunctions.clear(); }
};

template <typename... Args>
class FunctionList : public FunctionListBase<function<void(Args...)>> {
  public:
    void invoke(Args... args) {
        for (const auto &pair : this->mFunctions) {
            auto &function = pair.second;
            function(args...);
        }
    }
};

template <>
class FunctionList<void> : public FunctionListBase<function<void()>> {
  public:
    void invoke() {
        for (const auto &pair : this->mFunctions) {
            auto &function = pair.second;
            function();
        }
    }
};

} // namespace fl
