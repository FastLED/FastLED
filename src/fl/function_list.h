#pragma once
#include "ftl/function.h"
#include "ftl/pair.h"
#include "ftl/vector.h"
#include "ftl/type_traits.h"

/* TODO: embed this in ftl/function.h */

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

    // Boolean conversion for if (callback) checks
    explicit operator bool() const { return !empty(); }
};

// Primary template declaration (no definition - only specializations exist)
template <typename> class FunctionList;

// Partial specialization for function signature syntax: FunctionList<void(Args...)>
// Supports: FunctionList<void()>, FunctionList<void(float)>, FunctionList<void(u8, float, float)>, etc.
template <typename... Args>
class FunctionList<void(Args...)> : public FunctionListBase<function<void(Args...)>> {
  public:
    void invoke(Args... args) {
        for (const auto &pair : this->mFunctions) {
            auto &function = pair.second;
            function(args...);
        }
    }

    // Call operator - syntactic sugar for invoke()
    void operator()(Args... args) {
        invoke(args...);
    }
};

// Partial specialization for non-void return types: FunctionList<R(Args...)> where R != void
// Triggers a compile-time error when attempting to use non-void return types
template <typename R, typename... Args>
class FunctionList<R(Args...)> {
    static_assert(fl::is_same<R, void>::value,
                  "FunctionList only supports void return type. "
                  "Use FunctionList<void(Args...)> instead of FunctionList<ReturnType(Args...)>.");
};

} // namespace fl
