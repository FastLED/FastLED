#include "fl/referent.h"

#include "fl/namespace.h"

namespace fl {

Referent::Referent() : mRefCount(0), mWeakPtr(nullptr) {}

Referent::~Referent() {
    // The weak pointer cleanup should have been done in unref()
    // when the reference count reached 0
}

void Referent::ref() const { mRefCount++; }

int Referent::ref_count() const { return mRefCount; }

void Referent::unref() const {
    if (--mRefCount == 0) {
        if (mWeakPtr) {
            mWeakPtr->setReferent(nullptr);
            mWeakPtr->unref();
            mWeakPtr = nullptr;
        }
        destroy();
    }
}

void Referent::destroy() const { delete this; }

Referent::Referent(const Referent &) : mRefCount(0), mWeakPtr(nullptr) {}

Referent &Referent::operator=(const Referent &) {
    // Don't copy reference count or weak pointer
    return *this;
}

Referent::Referent(Referent &&) : mRefCount(0), mWeakPtr(nullptr) {}

Referent &Referent::operator=(Referent &&) {
    // Don't move reference count or weak pointer
    return *this;
}

} // namespace fl
