#pragma once

#include "fl/function.h"
#include "fl/json.h"
#include "fl/namespace.h"
#include "fl/memory.h"
#include "fl/str.h"
#include "fl/mutex.h"
#include "fl/unused.h"


namespace fl {

// JSON UI components migrated to shared_ptr system
class JsonUiInternal;
using JsonUiInternalPtr = fl::shared_ptr<JsonUiInternal>;

class JsonUiInternal {
  public:
    // Constructor: Initializes the base JsonUiInternal with name.
    // This constructor is protected because JsonUiInternal is now an abstract base class.
    public:
    JsonUiInternal(const fl::string &name) : mName(name), mId(nextId()) {}

  public:
    virtual ~JsonUiInternal() = default;

    const fl::string &name() const;
    virtual void updateInternal(const fl::Json &json) { FL_UNUSED(json); }
    virtual void toJson(fl::Json &json) const { FL_UNUSED(json); }
    int id() const;

    // Group functionality
    void setGroup(const fl::string &groupName);
    const fl::string &groupName() const;

    // Change tracking for polling (eliminates need for manual notifications)
    bool hasChanged() const;
    void markChanged();
    void clearChanged();

  private:
    static int nextId();
    fl::string mName;
    int mId;
    fl::string mGroup;
    mutable fl::mutex mMutex;
    mutable bool mHasChanged = false; // Track if component has changed since last poll
};

} // namespace fl
