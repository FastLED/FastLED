#pragma once

#include "ptr.h"
#include "string.h"


#ifndef FASTLED_STR_INLINED_SIZE
#define FASTLED_STR_INLINED_SIZE 64
#endif

template<size_t SIZE> class StrN;

// A copy on write string class. Copy operations are
// fast unless the string overflows the inlined buffer.
// Whenever possible the heap memory will be shared.
typedef StrN<FASTLED_STR_INLINED_SIZE> Str;


///////////////////////////////////////////////////////
// Implementation details.

DECLARE_SMART_PTR(StringHolder);

class StringHolder : public Referent {
  public:
    StringHolder(const char *str);
    ~StringHolder();
    bool isShared() const { return ref_count() > 1; }
    void grow(size_t newLength);
    const char *data() const { return mData; }
    char *data() { return mData; }
    size_t length() const { return mLength; }

  private:
    char *mData;
    size_t mLength;
};

template <size_t SIZE = 64> class StrN {
  private:
    size_t mLength = 0;

    char mInlineData[SIZE];
    StringHolderPtr mHeapData;

  public:
    // Constructors
    StrN() { mInlineData[0] = '\0'; }

    template <size_t M> StrN(const StrN<M> &other) { copy(other); }

    StrN(const char *str) {
        size_t len = strlen(str);
        if (len + 1 <= SIZE) {
            memcpy(mInlineData, str, len + 1);
            mHeapData.reset();
        } else {
            mHeapData = StringHolderPtr::New(str);
        }
        mLength = len;
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

    StrN &operator=(const StrN &other) { copy(other); }

    template <size_t M> StrN &operator=(const StrN<M> &other) { copy(other); }

    // Destructor
    ~StrN() {}

    // Accessors
    size_t size() const { return mLength; }
    const char *c_str() const {
        return mHeapData ? mHeapData->data() : mInlineData;
    }

    char &operator[](size_t index) {
        if (index >= mLength) {
            static char dummy = '\0';
            return dummy;
        }
        return c_str()[index];
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



