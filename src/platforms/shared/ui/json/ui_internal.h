#pragma once

// IWYU pragma: private

#include "fl/stl/function.h"
#include "fl/stl/json.h"
#include "fl/stl/memory.h"
#include "fl/stl/string.h"
#include "fl/stl/mutex.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"


namespace fl {

// JSON UI components migrated to shared_ptr system
class JsonUiInternal;
using JsonUiInternalPtr = fl::shared_ptr<JsonUiInternal>;

class JsonUiInternal {
  public:
    // Constructor: Initializes the base JsonUiInternal with name.
    // This constructor is protected because JsonUiInternal is now an abstract base class.
    public:
    JsonUiInternal(const fl::string &name) FL_NOEXCEPT : mName(name), mId(nextId()) {}

  public:
    virtual ~JsonUiInternal() = default;

    const fl::string &name() const FL_NOEXCEPT;
    virtual void updateInternal(const fl::json &json) FL_NOEXCEPT { FL_UNUSED(json); }
    virtual void toJson(fl::json &json) const FL_NOEXCEPT { FL_UNUSED(json); }
    int id() const FL_NOEXCEPT;

    // Group functionality
    void setGroup(const fl::string &groupName) FL_NOEXCEPT;
    fl::string groupName() const FL_NOEXCEPT;

    // Change tracking for polling (eliminates need for manual notifications)
    bool hasChanged() const FL_NOEXCEPT;
    void markChanged() FL_NOEXCEPT;
    void clearChanged() FL_NOEXCEPT;

  private:
    static int nextId() FL_NOEXCEPT;
    fl::string mName;
    int mId;
    fl::string mGroup;
    mutable fl::mutex mMutex;
    mutable bool mHasChanged = false; // Track if component has changed since last poll
};

} // namespace fl
