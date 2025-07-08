#pragma once
#include "fl/function.h"
#include "fl/pair.h"
#include "fl/vector.h"
#include "fl/type_traits.h"

namespace fl {

template <typename FunctionType> class FunctionListBase {
  protected:
    fl::vector<pair<int, FunctionType>> mFunctions;
    int mCounter = 0;

  public:
    // Iterator types
    using iterator = typename fl::vector<pair<int, FunctionType>>::iterator;
    using const_iterator = typename fl::vector<pair<int, FunctionType>>::const_iterator;
    
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
    
    // Iterator methods
    iterator begin() { return mFunctions.begin(); }
    iterator end() { return mFunctions.end(); }
    const_iterator begin() const { return mFunctions.begin(); }
    const_iterator end() const { return mFunctions.end(); }
    const_iterator cbegin() const { return mFunctions.cbegin(); }
    const_iterator cend() const { return mFunctions.cend(); }
    
    // Size information
    fl::size size() const { return mFunctions.size(); }
    bool empty() const { return mFunctions.empty(); }
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

template <>
class FunctionList<void()> : public FunctionListBase<function<void()>> {
  public:
    void invoke() {
        for (const auto &pair : this->mFunctions) {
            auto &function = pair.second;
            function();
        }
    }
};

} // namespace fl
