#pragma once

// base class for all ui components

#include "ptr.h"
#include <atomic>
#include <string>

FASTLED_NAMESPACE_BEGIN

class jsUI;
DECLARE_SMART_PTR(jsUI)

class jsUI : public Referent {
  public:
    jsUI();
    virtual ~jsUI();

    virtual std::string type() const = 0;
    virtual std::string name() const = 0;
    virtual void update(const char* jsonStr) = 0;
    virtual std::string toJsonStr() const = 0;
    int id() const;

  private:
    static int nextId();
    static std::atomic<uint32_t> sNextId;
    int mId;
};

FASTLED_NAMESPACE_END
