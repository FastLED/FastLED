#pragma once

#include "ptr.h"
#include "ui_internal.h"
#include <memory>
#include <mutex>

FASTLED_NAMESPACE_BEGIN

class jsUI {
public:
    jsUI(std::weak_ptr<jsUiInternal> internal);
    virtual ~jsUI();

    std::string type() const;
    std::string name() const;
    void update(const char* jsonStr);
    std::string toJsonStr() const;
    int id() const;
    void releaseInternal();
    std::shared_ptr<jsUiInternal> getInternal();

protected:
    virtual void updateInternal(const char* jsonStr) = 0;
    std::weak_ptr<jsUiInternal> mInternal;
    mutable std::mutex mMutex;
};

FASTLED_NAMESPACE_END
