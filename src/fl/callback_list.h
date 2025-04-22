#pragma once

#include "fl/callback.h"
#include "fl/vector.h"

namespace fl {

// Simple Add-only callback list.
template <typename CallbackT> class CallbackList {
  public:
    CallbackList() = default;
    ~CallbackList() = default;

    void add(CallbackT callback) { mCallbacks.push_back(callback); }
    void clear() { mCallbacks.clear(); }

    void invoke() {
        for (const auto &callback : mCallbacks) {
            callback();
        }
    }

    void invoke(float arg) {
        for (const auto &callback : mCallbacks) {
            callback(arg);
        }
    }

  private:
    fl::vector_inlined<CallbackT, 1> mCallbacks;
};

} // namespace fl
