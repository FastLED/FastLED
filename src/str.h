#pragma once

#include "ptr.h"
#include "string.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN


#ifndef FASTLED_STR_INLINED_SIZE
#define FASTLED_STR_INLINED_SIZE 64
#endif

// A copy on write string class. Fast to copy from another
// Str object as read only pointers are shared.
class Str;

///////////////////////////////////////////////////////
// Implementation details.

DECLARE_SMART_PTR(StringHolder);

class StringHolder : public Referent {
  public:
    StringHolder(const char *str);
    StringHolder(size_t length);
    ~StringHolder();
    bool isShared() const { return ref_count() > 1; }
    void grow(size_t newLength);
    const char *data() const { return mData; }
    char *data() { return mData; }
    size_t length() const { return mLength; }
    size_t capacity() const { return mCapacity; }

  private:
    char *mData = nullptr;
    size_t mLength = 0;
    size_t mCapacity = 0;
};

template <size_t SIZE = 64> class StrN {
  private:
    size_t mLength = 0;
    char mInlineData[SIZE] = {0};
    StringHolderPtr mHeapData;

  public:
    // Constructors
    StrN() = default;

    template <size_t M> StrN(const StrN<M> &other) { copy(other); }

    StrN(const char *str) {
        size_t len = strlen(str);
        mLength = len;
        if (len + 1 <= SIZE) {
            memcpy(mInlineData, str, len + 1);
            mHeapData.reset();
        } else {
            mHeapData = StringHolderPtr::New(str);
        }
    }

    StrN(const StrN &other) { copy(other); }

    bool operator==(const StrN &other) const {
        return strcmp(c_str(), other.c_str()) == 0;
    }

    bool operator!=(const StrN &other) const {
        return strcmp(c_str(), other.c_str()) != 0;
    }

    template <size_t M> void copy(const StrN<M> &other) {
        size_t len = other.size();
        if (len + 1 <= SIZE) {
            memcpy(mInlineData, other.c_str(), len + 1);
            mHeapData.reset();
        } else {
            if (other.mHeapData) {
                mHeapData = other.mHeapData;
            } else {
                mHeapData = StringHolderPtr::New(other.c_str());
            }
        }
        mLength = len;
    }

    void copy(const char *str) {
        size_t len = strlen(str);
        mLength = len;
        if (len + 1 <= SIZE) {
            memcpy(mInlineData, str, len + 1);
            mHeapData.reset();
        } else {
            if (mHeapData && !mHeapData->isShared()) {
                // We are the sole owners of this data so we can modify it
                mHeapData->grow(len);
                memcpy(mHeapData->data(), str, len + 1);
                return;
            }
            mHeapData.reset();
            mHeapData = StringHolderPtr::New(str);
        }
    }

    StrN &operator=(const StrN &other) { copy(other); return *this; }

    template <size_t M> StrN &operator=(const StrN<M> &other) { copy(other); return *this; }

    // Destructor
    ~StrN() {}

    // Accessors
    size_t size() const { return mLength; }
    const char *c_str() const {
        return mHeapData ? mHeapData->data() : mInlineData;
    }

    char *c_str_mutable() {
        return mHeapData ? mHeapData->data() : mInlineData;
    }

    char &operator[](size_t index) {
        if (index >= mLength) {
            static char dummy = '\0';
            return dummy;
        }
        return c_str_mutable()[index];
    }

    const char &operator[](size_t index) const {
        if (index >= mLength) {
            static char dummy = '\0';
            return dummy;
        }
        return c_str()[index];
    }

    // Append method
    void append(const char *str) {
        size_t len = strlen(str);
        size_t newLen = mLength + len;
        if (newLen + 1 <= SIZE) {
            memcpy(mInlineData + mLength, str, len + 1);
            mLength = newLen;
            return;
        }
        if (mHeapData) {
            if (!mHeapData->isShared()) {
                mHeapData->grow(newLen);
                memcpy(mHeapData->data() + mLength, str, len + 1);
                mLength = newLen;
                return;
            }
        }
        mHeapData.reset();
        StringHolderPtr newData = StringHolderPtr::New(mLength + len);
        if (newData) {
            memcpy(newData->data(), c_str(), mLength);
            memcpy(newData->data() + mLength, str, len + 1);
            mHeapData = newData;
            mLength = newLen;
        }
        mHeapData = newData;
    }

  private:
    StringHolderPtr mData;
};



class Str : public StrN<FASTLED_STR_INLINED_SIZE> {
  public:
    Str() : StrN<FASTLED_STR_INLINED_SIZE>() {}
    Str(const char *str) : StrN<FASTLED_STR_INLINED_SIZE>(str) {}
    Str(const Str &other) : StrN<FASTLED_STR_INLINED_SIZE>(other) {}
    template <size_t M> Str(const StrN<M> &other) : StrN<FASTLED_STR_INLINED_SIZE>(other) {}
    Str &operator=(const Str &other) { copy(other); return *this; }
};

FASTLED_NAMESPACE_END
