#pragma once

#include "fl/ptr.h"
#include "fl/referent.h"

namespace fl {

// Ptr template method implementations

template <typename T> 
template <typename... Args> 
inline Ptr<T> PtrTraits<T>::New(Args... args) {
    T *ptr = new T(args...);
    return Ptr<T>::TakeOwnership(ptr);
}

template <typename T> 
inline Ptr<T> PtrTraits<T>::New() {
    T *ptr = new T();
    return Ptr<T>::TakeOwnership(ptr);
}

template <typename T>
template <typename... Args> 
inline Ptr<T> Ptr<T>::New(Args... args) {
    return PtrTraits<T>::New(args...);
}

// Allow upcasting of Refs.
template <typename T>
template <typename U, typename>
inline Ptr<T>::Ptr(const Ptr<U> &refptr) : referent_(refptr.get()) {
    if (referent_ && isOwned()) {
        referent_->ref();
    }
}

template <typename T>
inline Ptr<T>::Ptr(const Ptr &other) : referent_(other.referent_) {
    if (referent_ && isOwned()) {
        referent_->ref();
    }
}

template <typename T>
inline Ptr<T>::Ptr(Ptr &&other) noexcept : referent_(other.referent_) {
    other.referent_ = nullptr;
}

template <typename T>
inline Ptr<T>::~Ptr() {
    if (referent_ && isOwned()) {
        referent_->unref();
    }
}

template <typename T>
inline Ptr<T> &Ptr<T>::operator=(const Ptr &other) {
    if (this != &other) {
        if (referent_ && isOwned()) {
            referent_->unref();
        }
        referent_ = other.referent_;
        if (referent_ && isOwned()) {
            referent_->ref();
        }
    }
    return *this;
}

template <typename T>
inline Ptr<T> &Ptr<T>::operator=(Ptr &&other) noexcept {
    if (this != &other) {
        if (referent_ && isOwned()) {
            referent_->unref();
        }
        referent_ = other.referent_;
        other.referent_ = nullptr;
    }
    return *this;
}

template <typename T>
inline void Ptr<T>::reset() {
    if (referent_ && isOwned()) {
        referent_->unref();
    }
    referent_ = nullptr;
}

template <typename T>
inline void Ptr<T>::reset(Ptr<T> &refptr) {
    if (refptr.referent_ != referent_) {
        if (refptr.referent_ && refptr.isOwned()) {
            refptr.referent_->ref();
        }
        if (referent_ && isOwned()) {
            referent_->unref();
        }
        referent_ = refptr.referent_;
    }
}

template <typename T>
inline T* Ptr<T>::release() {
    T *temp = referent_;
    referent_ = nullptr;
    return temp;
}

template <typename T>
inline void Ptr<T>::swap(Ptr &other) noexcept {
    T *temp = referent_;
    referent_ = other.referent_;
    other.referent_ = temp;
}

template <typename T>
inline Ptr<T>::Ptr(T *referent, bool from_heap) : referent_(referent) {
    if (referent_ && from_heap) {
        referent_->ref();
    }
}

// WeakPtr template method implementations

template <typename T>
inline WeakPtr<T>::WeakPtr(const Ptr<T> &ptr) : mWeakPtr(nullptr) {
    if (ptr) {
        mWeakPtr = ptr->getWeakPtr();
        if (!mWeakPtr) {
            // No weak reference exists yet, create one
            WeakReferent* weakRef = new WeakReferent();
            ptr->setWeakPtr(weakRef);
            weakRef->setReferent(ptr.get());
            mWeakPtr = weakRef;
        }
        if (mWeakPtr) {
            mWeakPtr->ref();
        }
    }
}

template <typename T>
template <typename U>
inline WeakPtr<T>::WeakPtr(const Ptr<U> &ptr) : mWeakPtr(nullptr) {
    if (ptr) {
        mWeakPtr = ptr->getWeakPtr();
        if (!mWeakPtr) {
            // No weak reference exists yet, create one
            WeakReferent* weakRef = new WeakReferent();
            ptr->setWeakPtr(weakRef);
            weakRef->setReferent(ptr.get());
            mWeakPtr = weakRef;
        }
        if (mWeakPtr) {
            mWeakPtr->ref();
        }
    }
}

template <typename T>
inline WeakPtr<T>::WeakPtr(const WeakPtr &other) : mWeakPtr(other.mWeakPtr) {
    if (mWeakPtr) {
        mWeakPtr->ref();
    }
}

template <typename T>
template <typename U>
inline WeakPtr<T>::WeakPtr(const WeakPtr<U> &other) : mWeakPtr(other.mWeakPtr) {
    if (mWeakPtr) {
        mWeakPtr->ref();
    }
}

template <typename T>
inline WeakPtr<T>::WeakPtr(WeakPtr &&other) noexcept : mWeakPtr(other.mWeakPtr) {
    other.mWeakPtr = nullptr;
}

template <typename T>
inline WeakPtr<T>::~WeakPtr() { 
    reset(); 
}

template <typename T>
inline WeakPtr<T> &WeakPtr<T>::operator=(const WeakPtr &other) {
    if (this != &other) {
        if (mWeakPtr) {
            mWeakPtr->unref();
        }
        mWeakPtr = other.mWeakPtr;
        if (mWeakPtr) {
            mWeakPtr->ref();
        }
    }
    return *this;
}

template <typename T>
inline Ptr<T> WeakPtr<T>::lock() const {
    if (!mWeakPtr) {
        return Ptr<T>();
    }
    T *out = static_cast<T *>(mWeakPtr->getReferent());
    if (!out) {
        // The referent has been destroyed
        return Ptr<T>();
    }
    if (out->ref_count() == 0) {
        // This is a static object, so the refcount is 0.
        return Ptr<T>::NoTracking(*out);
    }
    // This is a heap object, so we need to ref it.
    return Ptr<T>::TakeOwnership(static_cast<T *>(out));
}

template <typename T>
inline bool WeakPtr<T>::expired() const {
    if (!mWeakPtr) {
        return true;
    }
    if (!mWeakPtr->getReferent()) {
        return true;
    }
    return false;
}

template <typename T>
inline WeakPtr<T>::operator bool() const {
    return mWeakPtr && mWeakPtr->getReferent();
}

template <typename T>
inline bool WeakPtr<T>::operator!() const {
    bool ok = *this;
    return !ok;
}

template <typename T>
inline bool WeakPtr<T>::operator==(const WeakPtr &other) const {
    return mWeakPtr == other.mWeakPtr;
}

template <typename T>
inline bool WeakPtr<T>::operator!=(const WeakPtr &other) const {
    return !(mWeakPtr != other.mWeakPtr);
}

template <typename T>
inline bool WeakPtr<T>::operator==(const T *other) const {
    return lock().get() == other;
}

template <typename T>
inline bool WeakPtr<T>::operator==(T *other) const {
    if (!mWeakPtr) {
        return other == nullptr;
    }
    return mWeakPtr->getReferent() == other;
}

template <typename T>
inline bool WeakPtr<T>::operator==(const Ptr<T> &other) const {
    if (!mWeakPtr) {
        return !other;
    }
    return mWeakPtr->getReferent() == other.get();
}

template <typename T>
inline bool WeakPtr<T>::operator!=(const T *other) const {
    bool equal = *this == other;
    return !equal;
}

template <typename T>
inline void WeakPtr<T>::reset() {
    if (mWeakPtr) {
        mWeakPtr->unref();
        mWeakPtr = nullptr;
    }
}

template <typename T> 
inline WeakPtr<T> Ptr<T>::weakRefNoCreate() const {
    if (!referent_) {
        return WeakPtr<T>();
    }
    WeakReferent *tmp = get()->getWeakPtr();
    if (!tmp) {
        return WeakPtr<T>();
    }
    T *referent = static_cast<T *>(tmp->getReferent());
    if (!referent) {
        return WeakPtr<T>();
    }
    // At this point, we know that our weak referent is valid.
    // However, the template parameter ensures that either we have
    // an exact type, or are at least down-castable of it.
    WeakPtr<T> out;
    out.mWeakPtr = tmp;
    if (out.mWeakPtr) {
        out.mWeakPtr->ref();
    }
    return out;
}

// Free function templates

template <typename T, typename... Args> 
inline Ptr<T> NewPtr(Args... args) {
    return Ptr<T>::New(args...);
}

template <typename T> 
inline Ptr<T> NewPtrNoTracking(T &obj) {
    return Ptr<T>::NoTracking(obj);
}

} // namespace fl
