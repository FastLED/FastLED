#include "fl/stl/basic_string.h" // ok no header - public declaration remains in parent directory.
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"

namespace fl {

NotNullStringHolderPtr& basic_string::heapData() FL_NO_EXCEPT {
    if (!mStorage.is<NotNullStringHolderPtr>()) {
        mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(0));
    }
    return mStorage.get<NotNullStringHolderPtr>();
}

// ======= FIND =======

fl::size basic_string::find(const char& value) const FL_NO_EXCEPT {
    for (fl::size i = 0; i < mLength; ++i) {
        if (c_str()[i] == value) return i;
    }
    return npos;
}

fl::size basic_string::find(const char* substr) const FL_NO_EXCEPT {
    if (!substr) return npos;
    auto begin_ptr = c_str();
    const char* found = fl::strstr(begin_ptr, substr);
    if (found) return found - begin_ptr;
    return npos;
}

fl::size basic_string::find(const char& value, fl::size start_pos) const FL_NO_EXCEPT {
    if (start_pos >= mLength) return npos;
    for (fl::size i = start_pos; i < mLength; ++i) {
        if (c_str()[i] == value) return i;
    }
    return npos;
}

fl::size basic_string::find(const char* substr, fl::size start_pos) const FL_NO_EXCEPT {
    if (!substr || start_pos >= mLength) return npos;
    auto begin_ptr = c_str() + start_pos;
    const char* found = fl::strstr(begin_ptr, substr);
    if (found) return found - c_str();
    return npos;
}

fl::size basic_string::find(const basic_string& other) const FL_NO_EXCEPT { return find(other.c_str()); }
fl::size basic_string::find(const basic_string& other, fl::size start_pos) const FL_NO_EXCEPT { return find(other.c_str(), start_pos); }

// ======= RFIND =======

fl::size basic_string::rfind(char c, fl::size pos) const FL_NO_EXCEPT {
    if (mLength == 0) return npos;
    fl::size searchPos = (pos >= mLength || pos == npos) ? (mLength - 1) : pos;
    const char* str = c_str();
    for (fl::size i = searchPos + 1; i > 0; --i) {
        if (str[i - 1] == c) return i - 1;
    }
    return npos;
}

fl::size basic_string::rfind(const char* s, fl::size pos, fl::size count) const FL_NO_EXCEPT {
    if (!s || count == 0) {
        if (count == 0) return (pos > mLength) ? mLength : pos;
        return npos;
    }
    if (count > mLength) return npos;
    fl::size maxStart = mLength - count;
    fl::size searchStart = (pos >= mLength || pos == npos) ? maxStart : pos;
    if (searchStart + count > mLength) searchStart = maxStart;
    const char* str = c_str();
    for (fl::size i = searchStart + 1; i > 0; --i) {
        fl::size idx = i - 1;
        if (idx + count > mLength) continue;
        bool match = true;
        for (fl::size j = 0; j < count; ++j) {
            if (str[idx + j] != s[j]) { match = false; break; }
        }
        if (match) return idx;
    }
    return npos;
}

fl::size basic_string::rfind(const char* s, fl::size pos) const FL_NO_EXCEPT {
    if (!s) return npos;
    return rfind(s, pos, fl::strlen(s));
}

fl::size basic_string::rfind(const basic_string& str, fl::size pos) const FL_NO_EXCEPT { return rfind(str.c_str(), pos, str.size()); }

// ======= FIND_FIRST_OF =======

fl::size basic_string::find_first_of(const char* s, fl::size pos, fl::size count) const FL_NO_EXCEPT {
    if (!s || count == 0) return npos;
    if (pos >= mLength) return npos;
    const char* str = c_str();
    for (fl::size i = pos; i < mLength; ++i) {
        for (fl::size j = 0; j < count; ++j) {
            if (str[i] == s[j]) return i;
        }
    }
    return npos;
}

fl::size basic_string::find_first_of(const char* s, fl::size pos) const FL_NO_EXCEPT {
    if (!s) return npos;
    return find_first_of(s, pos, fl::strlen(s));
}

fl::size basic_string::find_first_of(char c, fl::size pos) const FL_NO_EXCEPT { return find(c, pos); }
fl::size basic_string::find_first_of(const basic_string& str, fl::size pos) const FL_NO_EXCEPT { return find_first_of(str.c_str(), pos, str.size()); }

// ======= FIND_LAST_OF =======

fl::size basic_string::find_last_of(const char* s, fl::size pos, fl::size count) const FL_NO_EXCEPT {
    if (!s || count == 0) return npos;
    if (mLength == 0) return npos;
    fl::size searchPos = (pos >= mLength || pos == npos) ? (mLength - 1) : pos;
    const char* str = c_str();
    for (fl::size i = searchPos + 1; i > 0; --i) {
        for (fl::size j = 0; j < count; ++j) {
            if (str[i - 1] == s[j]) return i - 1;
        }
    }
    return npos;
}

fl::size basic_string::find_last_of(const char* s, fl::size pos) const FL_NO_EXCEPT {
    if (!s) return npos;
    return find_last_of(s, pos, fl::strlen(s));
}

fl::size basic_string::find_last_of(char c, fl::size pos) const FL_NO_EXCEPT { return rfind(c, pos); }
fl::size basic_string::find_last_of(const basic_string& str, fl::size pos) const FL_NO_EXCEPT { return find_last_of(str.c_str(), pos, str.size()); }

// ======= FIND_FIRST_NOT_OF =======

fl::size basic_string::find_first_not_of(char c, fl::size pos) const FL_NO_EXCEPT {
    if (pos >= mLength) return npos;
    const char* str = c_str();
    for (fl::size i = pos; i < mLength; ++i) {
        if (str[i] != c) return i;
    }
    return npos;
}

fl::size basic_string::find_first_not_of(const char* s, fl::size pos, fl::size count) const FL_NO_EXCEPT {
    if (!s || count == 0) return (pos < mLength) ? pos : npos;
    if (pos >= mLength) return npos;
    const char* str = c_str();
    for (fl::size i = pos; i < mLength; ++i) {
        bool found_in_set = false;
        for (fl::size j = 0; j < count; ++j) {
            if (str[i] == s[j]) { found_in_set = true; break; }
        }
        if (!found_in_set) return i;
    }
    return npos;
}

fl::size basic_string::find_first_not_of(const char* s, fl::size pos) const FL_NO_EXCEPT {
    if (!s) return (pos < mLength) ? pos : npos;
    return find_first_not_of(s, pos, fl::strlen(s));
}

fl::size basic_string::find_first_not_of(const basic_string& str, fl::size pos) const FL_NO_EXCEPT { return find_first_not_of(str.c_str(), pos, str.size()); }

// ======= FIND_LAST_NOT_OF =======

fl::size basic_string::find_last_not_of(char c, fl::size pos) const FL_NO_EXCEPT {
    if (mLength == 0) return npos;
    fl::size searchPos = (pos >= mLength || pos == npos) ? (mLength - 1) : pos;
    const char* str = c_str();
    for (fl::size i = searchPos + 1; i > 0; --i) {
        if (str[i - 1] != c) return i - 1;
    }
    return npos;
}

fl::size basic_string::find_last_not_of(const char* s, fl::size pos, fl::size count) const FL_NO_EXCEPT {
    if (!s || count == 0) {
        if (mLength == 0) return npos;
        fl::size searchPos = (pos >= mLength || pos == npos) ? (mLength - 1) : pos;
        return searchPos;
    }
    if (mLength == 0) return npos;
    fl::size searchPos = (pos >= mLength || pos == npos) ? (mLength - 1) : pos;
    const char* str = c_str();
    for (fl::size i = searchPos + 1; i > 0; --i) {
        bool found_in_set = false;
        for (fl::size j = 0; j < count; ++j) {
            if (str[i - 1] == s[j]) { found_in_set = true; break; }
        }
        if (!found_in_set) return i - 1;
    }
    return npos;
}

fl::size basic_string::find_last_not_of(const char* s, fl::size pos) const FL_NO_EXCEPT {
    if (!s) {
        if (mLength == 0) return npos;
        fl::size searchPos = (pos >= mLength || pos == npos) ? (mLength - 1) : pos;
        return searchPos;
    }
    return find_last_not_of(s, pos, fl::strlen(s));
}

fl::size basic_string::find_last_not_of(const basic_string& str, fl::size pos) const FL_NO_EXCEPT { return find_last_not_of(str.c_str(), pos, str.size()); }

// ======= CONTAINS =======

bool basic_string::contains(const char* substr) const FL_NO_EXCEPT { return find(substr) != npos; }
bool basic_string::contains(char c) const FL_NO_EXCEPT { return find(c) != npos; }
bool basic_string::contains(const basic_string& other) const FL_NO_EXCEPT { return find(other.c_str()) != npos; }

// ======= STARTS_WITH / ENDS_WITH =======

bool basic_string::starts_with(const char* prefix) const FL_NO_EXCEPT {
    if (!prefix) return true;
    fl::size prefix_len = fl::strlen(prefix);
    if (prefix_len > mLength) return false;
    return fl::strncmp(c_str(), prefix, prefix_len) == 0;
}

bool basic_string::ends_with(const char* suffix) const FL_NO_EXCEPT {
    if (!suffix) return true;
    fl::size suffix_len = fl::strlen(suffix);
    if (suffix_len > mLength) return false;
    return fl::strncmp(c_str() + mLength - suffix_len, suffix, suffix_len) == 0;
}

bool basic_string::starts_with(char c) const FL_NO_EXCEPT { return mLength > 0 && c_str()[0] == c; }
bool basic_string::starts_with(const basic_string& prefix) const FL_NO_EXCEPT { return starts_with(prefix.c_str()); }
bool basic_string::ends_with(char c) const FL_NO_EXCEPT { return mLength > 0 && c_str()[mLength - 1] == c; }
bool basic_string::ends_with(const basic_string& suffix) const FL_NO_EXCEPT { return ends_with(suffix.c_str()); }

// ======= POP_BACK =======

void basic_string::pop_back() FL_NO_EXCEPT {
    if (mLength > 0) {
        mLength--;
        c_str_mutable()[mLength] = '\0';
    }
}

// ======= INSERT =======

basic_string& basic_string::insert(fl::size pos, fl::size count, char ch) FL_NO_EXCEPT {
    if (pos > mLength) pos = mLength;
    if (count == 0) return *this;

    // Materialize non-owning storage before modifying
    if (isNonOwning()) {
        materialize();
    }

    fl::size newLen = mLength + count;

    // Handle COW
    if (hasHeapData() && heapData().get().use_count() > 1) {
        const NotNullStringHolderPtr& heap = heapData();
        NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
        if (pos > 0) fl::memcpy(newData->data(), heap->data(), pos);
        for (fl::size i = 0; i < count; ++i) newData->data()[pos + i] = ch;
        if (pos < mLength) fl::memcpy(newData->data() + pos + count, heap->data() + pos, mLength - pos);
        newData->data()[newLen] = '\0';
        mStorage = newData;
        mLength = newLen;
        return *this;
    }

    // Inline buffer
    if (newLen + 1 <= mInlineCapacity && !hasHeapData()) {
        if (pos < mLength) {
            fl::memmove(inlineBufferPtr() + pos + count, inlineBufferPtr() + pos, mLength - pos);
        }
        for (fl::size i = 0; i < count; ++i) inlineBufferPtr()[pos + i] = ch;
        mLength = newLen;
        inlineBufferPtr()[mLength] = '\0';
        return *this;
    }

    // Heap in-place or new allocation
    bool canInsertInPlace = hasHeapData();
    if (canInsertInPlace) {
        NotNullStringHolderPtr& heap = heapData();
        canInsertInPlace = heap.get().use_count() <= 1 && heap->hasCapacity(newLen);
        if (canInsertInPlace) {
            char* data = heap->data();
            if (pos < mLength) fl::memmove(data + pos + count, data + pos, mLength - pos);
            for (fl::size i = 0; i < count; ++i) data[pos + i] = ch;
            mLength = newLen;
            data[mLength] = '\0';
        }
    }
    if (!canInsertInPlace) {
        NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
        const char* src = c_str();
        if (pos > 0) fl::memcpy(newData->data(), src, pos);
        for (fl::size i = 0; i < count; ++i) newData->data()[pos + i] = ch;
        if (pos < mLength) fl::memcpy(newData->data() + pos + count, src + pos, mLength - pos);
        newData->data()[newLen] = '\0';
        mStorage = newData;
        mLength = newLen;
    }
    return *this;
}

basic_string& basic_string::insert(fl::size pos, const char* s) FL_NO_EXCEPT {
    if (!s) return *this;
    return insert(pos, s, fl::strlen(s));
}

basic_string& basic_string::insert(fl::size pos, const basic_string& str) FL_NO_EXCEPT {
    return insert(pos, str.c_str(), str.size());
}

basic_string& basic_string::insert(fl::size pos, const basic_string& str, fl::size pos2, fl::size count) FL_NO_EXCEPT {
    if (pos2 >= str.size()) return *this;
    fl::size actualCount = count;
    if (actualCount == npos || pos2 + actualCount > str.size()) {
        actualCount = str.size() - pos2;
    }
    return insert(pos, str.c_str() + pos2, actualCount);
}

basic_string& basic_string::insert(fl::size pos, const char* s, fl::size count) FL_NO_EXCEPT {
    if (pos > mLength) pos = mLength;
    if (!s || count == 0) return *this;

    // Materialize non-owning storage before modifying
    if (isNonOwning()) {
        materialize();
    }

    fl::size newLen = mLength + count;

    // Handle COW
    if (hasHeapData() && heapData().get().use_count() > 1) {
        const NotNullStringHolderPtr& heap = heapData();
        NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
        if (pos > 0) fl::memcpy(newData->data(), heap->data(), pos);
        fl::memcpy(newData->data() + pos, s, count);
        if (pos < mLength) fl::memcpy(newData->data() + pos + count, heap->data() + pos, mLength - pos);
        newData->data()[newLen] = '\0';
        mStorage = newData;
        mLength = newLen;
        return *this;
    }

    // Inline buffer
    if (newLen + 1 <= mInlineCapacity && !hasHeapData()) {
        if (pos < mLength) {
            fl::memmove(inlineBufferPtr() + pos + count, inlineBufferPtr() + pos, mLength - pos);
        }
        fl::memcpy(inlineBufferPtr() + pos, s, count);
        mLength = newLen;
        inlineBufferPtr()[mLength] = '\0';
        return *this;
    }

    // Heap in-place or new allocation
    bool canInsertInPlace = hasHeapData();
    if (canInsertInPlace) {
        NotNullStringHolderPtr& heap = heapData();
        canInsertInPlace = heap.get().use_count() <= 1 && heap->hasCapacity(newLen);
        if (canInsertInPlace) {
            char* data = heap->data();
            if (pos < mLength) fl::memmove(data + pos + count, data + pos, mLength - pos);
            fl::memcpy(data + pos, s, count);
            mLength = newLen;
            data[mLength] = '\0';
        }
    }
    if (!canInsertInPlace) {
        NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
        const char* src = c_str();
        if (pos > 0) fl::memcpy(newData->data(), src, pos);
        fl::memcpy(newData->data() + pos, s, count);
        if (pos < mLength) fl::memcpy(newData->data() + pos + count, src + pos, mLength - pos);
        newData->data()[newLen] = '\0';
        mStorage = newData;
        mLength = newLen;
    }
    return *this;
}

// ======= ERASE =======

basic_string& basic_string::erase(fl::size pos, fl::size count) FL_NO_EXCEPT {
    if (pos >= mLength) return *this;

    fl::size actualCount = count;
    if (actualCount == npos || pos + actualCount > mLength) {
        actualCount = mLength - pos;
    }
    if (actualCount == 0) return *this;

    // Handle COW
    if (hasHeapData() && heapData().get().use_count() > 1) {
        const NotNullStringHolderPtr& heap = heapData();
        NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(mLength - actualCount));
        if (pos > 0) fl::memcpy(newData->data(), heap->data(), pos);
        fl::size remainingLen = mLength - pos - actualCount;
        if (remainingLen > 0) {
            fl::memcpy(newData->data() + pos, heap->data() + pos + actualCount, remainingLen);
        }
        mLength = mLength - actualCount;
        newData->data()[mLength] = '\0';
        mStorage = newData;
        return *this;
    }

    fl::size remainingLen = mLength - pos - actualCount;
    if (remainingLen > 0) {
        char* data = c_str_mutable();
        fl::memmove(data + pos, data + pos + actualCount, remainingLen);
    }
    mLength -= actualCount;
    c_str_mutable()[mLength] = '\0';
    return *this;
}

// ======= REPLACE =======

basic_string& basic_string::replace(fl::size pos, fl::size count, const basic_string& str) FL_NO_EXCEPT {
    return replace(pos, count, str.c_str(), str.size());
}

basic_string& basic_string::replace(fl::size pos, fl::size count, const basic_string& str,
                     fl::size pos2, fl::size count2) FL_NO_EXCEPT {
    if (pos2 >= str.size()) return erase(pos, count);
    fl::size actualCount2 = count2;
    if (actualCount2 == npos || pos2 + actualCount2 > str.size()) {
        actualCount2 = str.size() - pos2;
    }
    return replace(pos, count, str.c_str() + pos2, actualCount2);
}

basic_string& basic_string::replace(fl::size pos, fl::size count, const char* s, fl::size count2) FL_NO_EXCEPT {
    if (pos > mLength) return *this;
    if (!s) return erase(pos, count);

    // Materialize non-owning storage before modifying
    if (isNonOwning()) {
        materialize();
    }

    fl::size actualCount = count;
    if (actualCount == npos || pos + actualCount > mLength) {
        actualCount = mLength - pos;
    }
    fl::size newLen = mLength - actualCount + count2;

    // Handle COW
    if (hasHeapData() && heapData().get().use_count() > 1) {
        const NotNullStringHolderPtr& heap = heapData();
        NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
        if (pos > 0) fl::memcpy(newData->data(), heap->data(), pos);
        fl::memcpy(newData->data() + pos, s, count2);
        fl::size remainingLen = mLength - pos - actualCount;
        if (remainingLen > 0) {
            fl::memcpy(newData->data() + pos + count2, heap->data() + pos + actualCount, remainingLen);
        }
        newData->data()[newLen] = '\0';
        mStorage = newData;
        mLength = newLen;
        return *this;
    }

    // Inline buffer
    if (newLen + 1 <= mInlineCapacity && !hasHeapData()) {
        if (count2 != actualCount) {
            fl::size remainingLen = mLength - pos - actualCount;
            if (remainingLen > 0) {
                fl::memmove(inlineBufferPtr() + pos + count2, inlineBufferPtr() + pos + actualCount, remainingLen);
            }
        }
        fl::memcpy(inlineBufferPtr() + pos, s, count2);
        mLength = newLen;
        inlineBufferPtr()[mLength] = '\0';
        return *this;
    }

    // Heap in-place or new allocation
    bool canReplaceInPlace = hasHeapData();
    if (canReplaceInPlace) {
        NotNullStringHolderPtr& heap = heapData();
        canReplaceInPlace = heap.get().use_count() <= 1 && heap->hasCapacity(newLen);
        if (canReplaceInPlace) {
            char* data = heap->data();
            if (count2 != actualCount) {
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memmove(data + pos + count2, data + pos + actualCount, remainingLen);
                }
            }
            fl::memcpy(data + pos, s, count2);
            mLength = newLen;
            data[mLength] = '\0';
        }
    }
    if (!canReplaceInPlace) {
        NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
        const char* src = c_str();
        if (pos > 0) fl::memcpy(newData->data(), src, pos);
        fl::memcpy(newData->data() + pos, s, count2);
        fl::size remainingLen = mLength - pos - actualCount;
        if (remainingLen > 0) {
            fl::memcpy(newData->data() + pos + count2, src + pos + actualCount, remainingLen);
        }
        newData->data()[newLen] = '\0';
        mStorage = newData;
        mLength = newLen;
    }
    return *this;
}

basic_string& basic_string::replace(fl::size pos, fl::size count, const char* s) FL_NO_EXCEPT {
    if (!s) return erase(pos, count);
    return replace(pos, count, s, fl::strlen(s));
}

basic_string& basic_string::replace(fl::size pos, fl::size count, fl::size count2, char ch) FL_NO_EXCEPT {
    if (pos > mLength) return *this;

    // Materialize non-owning storage before modifying
    if (isNonOwning()) {
        materialize();
    }

    fl::size actualCount = count;
    if (actualCount == npos || pos + actualCount > mLength) {
        actualCount = mLength - pos;
    }
    fl::size newLen = mLength - actualCount + count2;

    // Handle COW
    if (hasHeapData() && heapData().get().use_count() > 1) {
        const NotNullStringHolderPtr& heap = heapData();
        NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
        if (pos > 0) fl::memcpy(newData->data(), heap->data(), pos);
        for (fl::size i = 0; i < count2; ++i) newData->data()[pos + i] = ch;
        fl::size remainingLen = mLength - pos - actualCount;
        if (remainingLen > 0) {
            fl::memcpy(newData->data() + pos + count2, heap->data() + pos + actualCount, remainingLen);
        }
        newData->data()[newLen] = '\0';
        mStorage = newData;
        mLength = newLen;
        return *this;
    }

    // Inline buffer
    if (newLen + 1 <= mInlineCapacity && !hasHeapData()) {
        if (count2 != actualCount) {
            fl::size remainingLen = mLength - pos - actualCount;
            if (remainingLen > 0) {
                fl::memmove(inlineBufferPtr() + pos + count2, inlineBufferPtr() + pos + actualCount, remainingLen);
            }
        }
        for (fl::size i = 0; i < count2; ++i) inlineBufferPtr()[pos + i] = ch;
        mLength = newLen;
        inlineBufferPtr()[mLength] = '\0';
        return *this;
    }

    // Heap in-place or new allocation
    bool canReplaceInPlace = hasHeapData();
    if (canReplaceInPlace) {
        NotNullStringHolderPtr& heap = heapData();
        canReplaceInPlace = heap.get().use_count() <= 1 && heap->hasCapacity(newLen);
        if (canReplaceInPlace) {
            char* data = heap->data();
            if (count2 != actualCount) {
                fl::size remainingLen = mLength - pos - actualCount;
                if (remainingLen > 0) {
                    fl::memmove(data + pos + count2, data + pos + actualCount, remainingLen);
                }
            }
            for (fl::size i = 0; i < count2; ++i) data[pos + i] = ch;
            mLength = newLen;
            data[mLength] = '\0';
        }
    }
    if (!canReplaceInPlace) {
        NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
        const char* src = c_str();
        if (pos > 0) fl::memcpy(newData->data(), src, pos);
        for (fl::size i = 0; i < count2; ++i) newData->data()[pos + i] = ch;
        fl::size remainingLen = mLength - pos - actualCount;
        if (remainingLen > 0) {
            fl::memcpy(newData->data() + pos + count2, src + pos + actualCount, remainingLen);
        }
        newData->data()[newLen] = '\0';
        mStorage = newData;
        mLength = newLen;
    }
    return *this;
}

// ======= COMPARE =======

int basic_string::compare(const basic_string& str) const FL_NO_EXCEPT { return fl::strcmp(c_str(), str.c_str()); }

int basic_string::compare(fl::size pos1, fl::size count1, const basic_string& str) const FL_NO_EXCEPT {
    if (pos1 > mLength) {
        return str.empty() ? 0 : -1;
    }
    fl::size actualCount1 = count1;
    if (actualCount1 == npos || pos1 + actualCount1 > mLength) {
        actualCount1 = mLength - pos1;
    }
    fl::size minLen = (actualCount1 < str.size()) ? actualCount1 : str.size();
    int result = fl::strncmp(c_str() + pos1, str.c_str(), minLen);
    if (result != 0) return result;
    if (actualCount1 < str.size()) return -1;
    if (actualCount1 > str.size()) return 1;
    return 0;
}

int basic_string::compare(fl::size pos1, fl::size count1, const basic_string& str,
                     fl::size pos2, fl::size count2) const FL_NO_EXCEPT {
    if (pos1 > mLength || pos2 >= str.size()) {
        if (pos1 > mLength && pos2 >= str.size()) return 0;
        if (pos1 > mLength) return -1;
        return 1;
    }
    fl::size actualCount1 = count1;
    if (actualCount1 == npos || pos1 + actualCount1 > mLength) {
        actualCount1 = mLength - pos1;
    }
    fl::size actualCount2 = count2;
    if (actualCount2 == npos || pos2 + actualCount2 > str.size()) {
        actualCount2 = str.size() - pos2;
    }
    fl::size minLen = (actualCount1 < actualCount2) ? actualCount1 : actualCount2;
    int result = fl::strncmp(c_str() + pos1, str.c_str() + pos2, minLen);
    if (result != 0) return result;
    if (actualCount1 < actualCount2) return -1;
    if (actualCount1 > actualCount2) return 1;
    return 0;
}

int basic_string::compare(const char* s) const FL_NO_EXCEPT {
    if (!s) return mLength > 0 ? 1 : 0;
    return fl::strcmp(c_str(), s);
}

int basic_string::compare(fl::size pos1, fl::size count1, const char* s) const FL_NO_EXCEPT {
    if (!s) {
        if (pos1 >= mLength) return 0;
        fl::size actualCount1 = count1;
        if (actualCount1 == npos || pos1 + actualCount1 > mLength) {
            actualCount1 = mLength - pos1;
        }
        return (actualCount1 > 0) ? 1 : 0;
    }
    if (pos1 > mLength) return (s[0] == '\0') ? 0 : -1;
    fl::size actualCount1 = count1;
    if (actualCount1 == npos || pos1 + actualCount1 > mLength) {
        actualCount1 = mLength - pos1;
    }
    fl::size sLen = fl::strlen(s);
    fl::size minLen = (actualCount1 < sLen) ? actualCount1 : sLen;
    int result = fl::strncmp(c_str() + pos1, s, minLen);
    if (result != 0) return result;
    if (actualCount1 < sLen) return -1;
    if (actualCount1 > sLen) return 1;
    return 0;
}

int basic_string::compare(fl::size pos1, fl::size count1, const char* s, fl::size count2) const FL_NO_EXCEPT {
    if (!s) {
        if (pos1 >= mLength) return (count2 == 0) ? 0 : -1;
        fl::size actualCount1 = count1;
        if (actualCount1 == npos || pos1 + actualCount1 > mLength) {
            actualCount1 = mLength - pos1;
        }
        return (actualCount1 > 0) ? 1 : ((count2 == 0) ? 0 : -1);
    }
    if (pos1 > mLength) return (count2 == 0) ? 0 : -1;
    fl::size actualCount1 = count1;
    if (actualCount1 == npos || pos1 + actualCount1 > mLength) {
        actualCount1 = mLength - pos1;
    }
    fl::size minLen = (actualCount1 < count2) ? actualCount1 : count2;
    int result = fl::strncmp(c_str() + pos1, s, minLen);
    if (result != 0) return result;
    if (actualCount1 < count2) return -1;
    if (actualCount1 > count2) return 1;
    return 0;
}

// Optional capacity management, resizing, and swapping.

void basic_string::reserve(fl::size newCapacity) FL_NO_EXCEPT {
    if (newCapacity <= mLength) return;
    if (newCapacity + 1 <= mInlineCapacity) return;
    if (hasHeapData()) {
        const NotNullStringHolderPtr& heap = heapData();
        if (heap.get().use_count() <= 1 && heap->hasCapacity(newCapacity)) {
            return;
        }
    }
    NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newCapacity));
    fl::memcpy(newData->data(), c_str(), mLength);
    newData->data()[mLength] = '\0';
    mStorage = newData;
}

void basic_string::shrink_to_fit() FL_NO_EXCEPT {
    if (hasHeapData()) {
        NotNullStringHolderPtr& heap = heapData();
        if (heap.get().use_count() > 1) return;
        if (heap->capacity() <= mLength + 1) return;
        if (mLength + 1 <= mInlineCapacity) {
            fl::memcpy(inlineBufferPtr(), heap->data(), mLength + 1);
            mStorage.reset();
            return;
        }
        NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(mLength));
        fl::memcpy(newData->data(), heap->data(), mLength + 1);
        mStorage = newData;
    }
}

void basic_string::swapWith(basic_string& other) FL_NO_EXCEPT {
    if (this == &other) return;

    bool thisInline = isInline();
    bool otherInline = other.isInline();

    if (!thisInline && !otherInline) {
        // Both non-inline: swap variant + length
        fl::swap(mStorage, other.mStorage);
        fl::swap(mLength, other.mLength);
    } else if (thisInline && otherInline) {
        // Both inline: check capacity before swapping
        bool thisFits = other.mLength + 1 <= mInlineCapacity;
        bool otherFits = mLength + 1 <= other.mInlineCapacity;
        if (thisFits && otherFits) {
            // Both fit: swap buffer contents directly
            fl::size maxLen = fl::max(mLength, other.mLength);
            for (fl::size i = 0; i <= maxLen; ++i) {
                char tmp = inlineBufferPtr()[i];
                inlineBufferPtr()[i] = other.inlineBufferPtr()[i];
                other.inlineBufferPtr()[i] = tmp;
            }
            fl::swap(mLength, other.mLength);
        } else {
            // Capacity mismatch: promote to heap where needed
            NotNullStringHolderPtr thisData(
                fl::make_shared<StringHolder>(inlineBufferPtr(), mLength));
            NotNullStringHolderPtr otherData(
                fl::make_shared<StringHolder>(other.inlineBufferPtr(), other.mLength));
            fl::size thisLen = mLength;
            fl::size otherLen = other.mLength;
            if (thisFits) {
                mStorage.reset();
                fl::memcpy(inlineBufferPtr(), otherData->data(), otherLen + 1);
            } else {
                mStorage = otherData;
            }
            mLength = otherLen;
            if (otherFits) {
                other.mStorage.reset();
                fl::memcpy(other.inlineBufferPtr(), thisData->data(), thisLen + 1);
            } else {
                other.mStorage = thisData;
            }
            other.mLength = thisLen;
        }
    } else if (thisInline) {
        // this inline, other non-inline
        fl::size thisLen = mLength;
        // Take other's non-inline storage
        mStorage = fl::move(other.mStorage);
        mLength = other.mLength;
        // Put this's old inline data into other
        other.mStorage.reset();
        if (thisLen + 1 <= other.mInlineCapacity) {
            fl::memcpy(other.inlineBufferPtr(), inlineBufferPtr(), thisLen + 1);
        } else {
            other.mStorage = NotNullStringHolderPtr(
                fl::make_shared<StringHolder>(inlineBufferPtr(), thisLen));
        }
        other.mLength = thisLen;
    } else {
        // this non-inline, other inline — reverse
        other.swapWith(*this);
    }
}

void basic_string::resize(fl::size count) FL_NO_EXCEPT { resize(count, char()); }

void basic_string::resize(fl::size count, char ch) FL_NO_EXCEPT {
    if (count < mLength) {
        mLength = count;
        c_str_mutable()[mLength] = '\0';
    } else if (count > mLength) {
        fl::size additional_chars = count - mLength;
        reserve(count);
        char* data_ptr = c_str_mutable();
        for (fl::size i = 0; i < additional_chars; ++i) {
            data_ptr[mLength + i] = ch;
        }
        mLength = count;
        data_ptr[mLength] = '\0';
    }
}

// Optional assignment and shared-holder binding.

void basic_string::assign(const char* str, fl::size len) FL_NO_EXCEPT {
    mLength = len;
    if (len + 1 <= mInlineCapacity) {
        if (!isInline()) {
            mStorage.reset();
        }
        fl::memcpy(inlineBufferPtr(), str, len);
        inlineBufferPtr()[len] = '\0';
    } else {
        mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(str, len));
    }
}

basic_string& basic_string::assign(const basic_string& str) FL_NO_EXCEPT {
    copy(str);
    return *this;
}

basic_string& basic_string::assign(const basic_string& str, fl::size pos, fl::size count) FL_NO_EXCEPT {
    if (pos >= str.size()) {
        clear();
        return *this;
    }
    fl::size actualCount = count;
    if (actualCount == npos || pos + actualCount > str.size()) {
        actualCount = str.size() - pos;
    }
    copy(str.c_str() + pos, actualCount);
    return *this;
}

basic_string& basic_string::assign(fl::size count, char c) FL_NO_EXCEPT {
    if (count == 0) {
        clear();
        return *this;
    }
    mLength = count;
    if (count + 1 <= mInlineCapacity) {
        if (!isInline()) {
            mStorage.reset();
        }
        for (fl::size i = 0; i < count; ++i) {
            inlineBufferPtr()[i] = c;
        }
        inlineBufferPtr()[count] = '\0';
    } else {
        mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(count));
        NotNullStringHolderPtr& ptr = heapData();
        for (fl::size i = 0; i < count; ++i) {
            ptr->data()[i] = c;
        }
        ptr->data()[count] = '\0';
    }
    return *this;
}

basic_string& basic_string::assign(basic_string&& str) FL_NO_EXCEPT {
    moveAssign(fl::move(str));
    return *this;
}

void basic_string::setSharedHolder(const fl::shared_ptr<StringHolder>& holder) FL_NO_EXCEPT {
    if (!holder || holder->length() == 0) return;
    mLength = holder->length();
    mStorage = NotNullStringHolderPtr(holder);
}

// Optional hexadecimal and octal append operations.

basic_string& basic_string::appendHex(i32 val) FL_NO_EXCEPT { char b[64]={0}; int l=fl::itoa(val,b,16); write(b,l); return *this; }
basic_string& basic_string::appendHex(u32 val) FL_NO_EXCEPT { char b[64]={0}; int l=fl::utoa32(val,b,16); write(b,l); return *this; }
basic_string& basic_string::appendHex(i64 val) FL_NO_EXCEPT { char b[64]={0}; int l=fl::itoa64(val,b,16); write(b,l); return *this; }
basic_string& basic_string::appendHex(u64 val) FL_NO_EXCEPT { char b[64]={0}; int l=fl::utoa64(val,b,16); write(b,l); return *this; }
basic_string& basic_string::appendHex(i16 val) FL_NO_EXCEPT { return appendHex(static_cast<i32>(val)); }
basic_string& basic_string::appendHex(u16 val) FL_NO_EXCEPT { return appendHex(static_cast<u32>(val)); }
basic_string& basic_string::appendHex(i8 val) FL_NO_EXCEPT { return appendHex(static_cast<i32>(val)); }
basic_string& basic_string::appendHex(u8 val) FL_NO_EXCEPT { return appendHex(static_cast<u32>(val)); }
basic_string& basic_string::appendOct(i32 val) FL_NO_EXCEPT { char b[64]={0}; int l=fl::itoa(val,b,8); write(b,l); return *this; }
basic_string& basic_string::appendOct(u32 val) FL_NO_EXCEPT { char b[64]={0}; int l=fl::utoa32(val,b,8); write(b,l); return *this; }
basic_string& basic_string::appendOct(i64 val) FL_NO_EXCEPT { char b[64]={0}; int l=fl::itoa64(val,b,8); write(b,l); return *this; }
basic_string& basic_string::appendOct(u64 val) FL_NO_EXCEPT { char b[64]={0}; int l=fl::utoa64(val,b,8); write(b,l); return *this; }
basic_string& basic_string::appendOct(i16 val) FL_NO_EXCEPT { return appendOct(static_cast<i32>(val)); }
basic_string& basic_string::appendOct(u16 val) FL_NO_EXCEPT { return appendOct(static_cast<u32>(val)); }
basic_string& basic_string::appendOct(i8 val) FL_NO_EXCEPT { return appendOct(static_cast<i32>(val)); }
basic_string& basic_string::appendOct(u8 val) FL_NO_EXCEPT { return appendOct(static_cast<u32>(val)); }

} // namespace fl
