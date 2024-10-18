#include "ptr.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

Referent::Referent() : mRefCount(0) {}
Referent::~Referent() = default;
void Referent::ref() { mRefCount++; }

int Referent::ref_count() const { return mRefCount; }

void Referent::unref() {
    if (--mRefCount == 0) {
        if (mWeakPtr) {
            mWeakPtr->setReferent(nullptr);
            mWeakPtr.reset();
        }
        destroy();
    }
}

void Referent::destroy() { delete this; }

Referent::Referent(const Referent &) = default;
Referent &Referent::operator=(const Referent &) = default;
Referent::Referent(Referent &&) = default;
Referent &Referent::operator=(Referent &&) = default;

FASTLED_NAMESPACE_END
