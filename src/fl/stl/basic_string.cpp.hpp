#include "fl/stl/basic_string.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/span.h"
#include "fl/stl/string_view.h"
#include "fl/system/sketch_macros.h"  // FL_PLATFORM_HAS_LARGE_MEMORY -- gates int64 itoa path

namespace fl {

// ODR-use definition
const fl::size basic_string::npos;

// ======= PUBLIC CONSTRUCTORS (span delegate) =======

basic_string::basic_string(fl::span<char, static_cast<fl::size>(-1)> storage) FL_NO_EXCEPT
    : basic_string(storage.data(), storage.size()) {}

// ======= DESTRUCTOR =======

basic_string::~basic_string() FL_DTOR_NOEXCEPT {}

// ======= string_view CONSTRUCTOR FROM basic_string =======
// Defined here (in the same TU as basic_string itself) so
// string_view.h can stay light: it forward-declares basic_string
// and declares this ctor without needing basic_string's complete
// type.

string_view::string_view(const basic_string& str) FL_NO_EXCEPT
    : mData(str.c_str()), mSize(str.size()) {}

// ======= ACCESSORS =======

const char* basic_string::c_str() const {
    if (mStorage.is<ConstView>()) {
        const ConstView& view = mStorage.get<ConstView>();
        if (view.data && view.length > 0 && view.data[view.length] != '\0') {
            const_cast<basic_string*>(this)->materialize();
        }
    }
    return constData();
}

char* basic_string::c_str_mutable() {
    // Inline mode — already mutable
    if (mStorage.empty()) {
        return inlineBufferPtr();
    }
    struct Visitor {
        basic_string* self;
        char* result;
        void accept(NotNullStringHolderPtr& heap) {
            if (heap.get().use_count() > 1) {
                // COW: detach from shared data before returning mutable pointer
                self->mStorage = NotNullStringHolderPtr(
                    fl::make_shared<StringHolder>(heap->data(), self->mLength));
                result = self->heapData()->data();
            } else {
                result = heap->data();
            }
        }
        void accept(ConstLiteral&) {
            self->materialize();
            result = self->hasHeapData() ? self->heapData()->data()
                                         : self->inlineBufferPtr();
        }
        void accept(ConstView&) {
            self->materialize();
            result = self->hasHeapData() ? self->heapData()->data()
                                         : self->inlineBufferPtr();
        }
    };
    Visitor v{this, nullptr};
    mStorage.visit(v);
    return v.result;
}

fl::size basic_string::capacity() const {
    if (hasHeapData()) {
        return heapData()->capacity();
    } else if (isNonOwning()) {
        return 0;
    }
    return mInlineCapacity;
}

// ======= ELEMENT ACCESS =======

char basic_string::operator[](fl::size index) const {
    if (index >= mLength) {
        return '\0';
    }
    return c_str()[index];
}

char& basic_string::operator[](fl::size index) {
    static char dummy = '\0';
    if (index >= mLength) {
        dummy = '\0';
        return dummy;
    }
    return c_str_mutable()[index];
}

char& basic_string::at(fl::size pos) {
    static char dummy = '\0';
    if (pos >= mLength) {
        dummy = '\0';
        return dummy;
    }
    return c_str_mutable()[pos];
}

const char& basic_string::at(fl::size pos) const {
    static char dummy = '\0';
    if (pos >= mLength) {
        dummy = '\0';
        return dummy;
    }
    return c_str()[pos];
}

char basic_string::front() const {
    if (empty()) return '\0';
    return c_str()[0];
}

char basic_string::back() const {
    if (empty()) return '\0';
    return c_str()[mLength - 1];
}

char basic_string::charAt(fl::size index) const {
    if (index >= mLength) return '\0';
    return c_str()[index];
}

// ======= COMPARISON OPERATORS =======

bool basic_string::operator==(const basic_string& other) const { return fl::strcmp(c_str(), other.c_str()) == 0; }
bool basic_string::operator!=(const basic_string& other) const { return fl::strcmp(c_str(), other.c_str()) != 0; }
bool basic_string::operator<(const basic_string& other) const { return fl::strcmp(c_str(), other.c_str()) < 0; }
bool basic_string::operator>(const basic_string& other) const { return fl::strcmp(c_str(), other.c_str()) > 0; }
bool basic_string::operator<=(const basic_string& other) const { return fl::strcmp(c_str(), other.c_str()) <= 0; }
bool basic_string::operator>=(const basic_string& other) const { return fl::strcmp(c_str(), other.c_str()) >= 0; }

bool basic_string::operator==(const char* other) const { return fl::strcmp(c_str(), other ? other : "") == 0; }
bool basic_string::operator!=(const char* other) const { return fl::strcmp(c_str(), other ? other : "") != 0; }

// ======= HELPER METHODS =======

const char* basic_string::constData() const {
    if (isInline()) {
        return inlineBufferPtr();
    } else if (mStorage.is<NotNullStringHolderPtr>()) {
        return mStorage.get<NotNullStringHolderPtr>()->data();
    } else if (mStorage.is<ConstLiteral>()) {
        return mStorage.get<ConstLiteral>().data;
    } else if (mStorage.is<ConstView>()) {
        return mStorage.get<ConstView>().data;
    }
    return "";
}

void basic_string::materialize() {
    if (mStorage.is<ConstLiteral>()) {
        const char* data = mStorage.get<ConstLiteral>().data;
        if (!data) {
            mLength = 0;
            mStorage.reset();
            inlineBufferPtr()[0] = '\0';
            return;
        }
        fl::size len = mLength;
        if (len + 1 <= mInlineCapacity) {
            fl::memcpy(inlineBufferPtr(), data, len);
            inlineBufferPtr()[len] = '\0';
            mStorage.reset();
        } else {
            mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(data, len));
        }
    } else if (mStorage.is<ConstView>()) {
        const ConstView& view = mStorage.get<ConstView>();
        if (!view.data) {
            mLength = 0;
            mStorage.reset();
            inlineBufferPtr()[0] = '\0';
            return;
        }
        fl::size len = view.length;
        mLength = len;
        if (len + 1 <= mInlineCapacity) {
            fl::memcpy(inlineBufferPtr(), view.data, len);
            inlineBufferPtr()[len] = '\0';
            mStorage.reset();
        } else {
            mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(view.data, len));
        }
    }
}

NotNullStringHolderPtr& basic_string::heapData() {
    if (!mStorage.is<NotNullStringHolderPtr>()) {
        mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(0));
    }
    return mStorage.get<NotNullStringHolderPtr>();
}

const NotNullStringHolderPtr& basic_string::heapData() const {
    return mStorage.get<NotNullStringHolderPtr>();
}

// ======= WRITE =======

// Trivial delegates to the workhorse write(const char*, fl::size) below.
// Folding away the address-of-byte indirection that write(u8) previously
// did, and dropping the verbose static_cast<void*> intermediate from
// write(u8*) — shrinks each call site by a few bytes (#3122 B5 /
// #2886 Stage 5).
fl::size basic_string::write(const fl::u8* data, fl::size n) {
    return write(fl::bit_cast_ptr<const char>(data), n);
}

fl::size basic_string::write(char c) { return write(&c, 1); }

fl::size basic_string::write(fl::u8 c) {
    return write(static_cast<char>(c));
}

fl::size basic_string::write(const char* str, fl::size n) {
    fl::size newLen = mLength + n;

    // Handle non-owning storage
    if (isNonOwning()) {
        const char* existingData = constData();
        fl::size existingLen = mLength;
        if (newLen + 1 <= mInlineCapacity) {
            if (existingLen > 0 && existingData) {
                fl::memcpy(inlineBufferPtr(), existingData, existingLen);
            }
            fl::memcpy(inlineBufferPtr() + existingLen, str, n);
            inlineBufferPtr()[newLen] = '\0';
            mStorage.reset();
            mLength = newLen;
            return mLength;
        }
    } else if (hasHeapData() && heapData().get().use_count() <= 1) {
        NotNullStringHolderPtr& heap = heapData();
        if (!heap->hasCapacity(newLen)) {
            // Check if str points into our buffer (self-referential write).
            // grow() uses realloc which can relocate the buffer.
            const char* bufStart = heap->data();
            fl::size grow_length = fl::max(3, newLen * 3 / 2);
            if (str >= bufStart && str < bufStart + mLength + 1) {
                fl::size offset = static_cast<fl::size>(str - bufStart);
                heap->grow(grow_length);
                str = heap->data() + offset; // update to new location
            } else {
                heap->grow(grow_length);
            }
        }
        fl::memcpy(heap->data() + mLength, str, n);
        mLength = newLen;
        heap->data()[mLength] = '\0';
        return mLength;
    } else if (!hasHeapData() && newLen + 1 <= mInlineCapacity) {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING(array-bounds)
        fl::memcpy(inlineBufferPtr() + mLength, str, n);
        FL_DISABLE_WARNING_POP
        mLength = newLen;
        inlineBufferPtr()[mLength] = '\0';
        return mLength;
    }
    // Materialize non-owning storage, detach shared storage, or grow inline
    // storage through one allocation path. Keep the old storage alive until
    // both copies finish so appending from our own data remains valid.
    const char* existingData = constData();
    NotNullStringHolderPtr newData = NotNullStringHolderPtr(fl::make_shared<StringHolder>(newLen));
    {
        if (mLength > 0 && existingData) {
            fl::memcpy(newData->data(), existingData, mLength);
        }
        fl::memcpy(newData->data() + mLength, str, n);
        newData->data()[newLen] = '\0';
        mStorage = newData;
        mLength = newLen;
    }
    return mLength;
}

fl::size basic_string::write(const fl::u16& n) {
    char buf[64] = {0};
    int len = fl::utoa32(static_cast<fl::u32>(n), buf, 10);
    return write(buf, len);
}

fl::size basic_string::write(const fl::u32& val) {
    char buf[64] = {0};
    int len = fl::utoa32(val, buf, 10);
    return write(buf, len);
}

fl::size basic_string::write(const u64& val) {
    char buf[64] = {0};
#if FL_PLATFORM_HAS_LARGE_MEMORY
    int len = fl::utoa64(val, buf, 10);
#else
    // Low-memory gate per #3224 Tier 3G: route through the 32-bit utoa to
    // avoid `__udivmoddi4` + `__clzdi2` (libgcc-nofp's 64-bit divmod cascade,
    // ~1.5 KB on Cortex-M0+). Values larger than UINT32_MAX saturate.
    int len = fl::utoa32(val > 0xFFFFFFFFull ? 0xFFFFFFFFu
                                              : static_cast<fl::u32>(val),
                         buf, 10);
#endif
    return write(buf, len);
}

fl::size basic_string::write(const i64& val) {
    char buf[64] = {0};
#if FL_PLATFORM_HAS_LARGE_MEMORY
    int len = fl::itoa64(val, buf, 10);
#else
    // Low-memory gate per #3224 Tier 3G: route through the 32-bit itoa to
    // avoid `__udivmoddi4` + `__clzdi2`. Values outside INT32 range saturate
    // to INT32_MIN / INT32_MAX. Sketches that need full int64 formatting on
    // Low-memory can opt in by defining FL_PLATFORM_HAS_LARGE_MEMORY=1.
    fl::i32 narrow = val >  2147483647LL ?  2147483647
                   : val < -2147483648LL ? -2147483648
                                          : static_cast<fl::i32>(val);
    int len = fl::itoa(narrow, buf, 10);
#endif
    return write(buf, len);
}

fl::size basic_string::write(const fl::i32& val) {
    char buf[64] = {0};
    int len = fl::itoa(val, buf, 10);
    return write(buf, len);
}

fl::size basic_string::write(const fl::i8 val) {
    char buf[64] = {0};
    int len = fl::itoa(static_cast<fl::i32>(val), buf, 10);
    return write(buf, len);
}

// ======= COPY =======

void basic_string::copy(const char* str) {
    fl::size len = fl::strlen(str);
    mLength = len;
    if (len + 1 <= mInlineCapacity) {
        if (!isInline()) {
            mStorage.reset();
        }
        fl::memcpy(inlineBufferPtr(), str, len + 1);
    } else {
        if (hasHeapData() && heapData().get().use_count() <= 1) {
            heapData()->copy(str, len);
            return;
        }
        mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(str, len));
    }
}

void basic_string::copy(const char* str, fl::size len) {
    mLength = len;
    if (len + 1 <= mInlineCapacity) {
        if (!isInline()) {
            mStorage.reset();
        }
        fl::memcpy(inlineBufferPtr(), str, len);
        inlineBufferPtr()[len] = '\0';
    } else {
        if (hasHeapData() && heapData().get().use_count() <= 1) {
            heapData()->copy(str, len);
            return;
        }
        mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(str, len));
    }
}

void basic_string::copy(const basic_string& other) {
    fl::size len = other.size();
    if (other.hasHeapData()) {
        // Share the heap pointer
        const auto& otherHeap = other.mStorage.get<NotNullStringHolderPtr>();
        mStorage = otherHeap;
    } else if (len + 1 <= mInlineCapacity) {
        if (!isInline()) {
            mStorage.reset();
        }
        const char* src = other.c_str();
        char* dst = inlineBufferPtr();
        fl::memcpy(dst, src, len + 1);
    } else {
        mStorage = NotNullStringHolderPtr(fl::make_shared<StringHolder>(other.c_str(), len));
    }
    mLength = len;
}

fl::size basic_string::copy(char* dest, fl::size count, fl::size pos) const {
    if (!dest) return 0;
    if (pos >= mLength) return 0;
    fl::size actualCount = count;
    if (actualCount > mLength - pos) {
        actualCount = mLength - pos;
    }
    if (actualCount > 0) {
        fl::memcpy(dest, c_str() + pos, actualCount);
    }
    return actualCount;
}

// ======= ASSIGN =======

void basic_string::assign(const char* str, fl::size len) {
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

basic_string& basic_string::assign(const basic_string& str) {
    copy(str);
    return *this;
}

basic_string& basic_string::assign(const basic_string& str, fl::size pos, fl::size count) {
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

basic_string& basic_string::assign(fl::size count, char c) {
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

// ======= MEMORY MANAGEMENT =======

void basic_string::clear(bool freeMemory) {
    mLength = 0;
    if (isNonOwning() || (freeMemory && hasHeapData())) {
        mStorage.reset();
        inlineBufferPtr()[0] = '\0';
    } else {
        c_str_mutable()[0] = '\0';
    }
}

// ======= STACK OPERATIONS =======

void basic_string::push_back(char c) { write(c); }
void basic_string::push_ascii(char c) { write(c); }

// ======= PROTECTED: MOVE / FACTORY HELPERS =======

void basic_string::moveFrom(basic_string&& other) FL_NO_EXCEPT {
    if (other.isInline()) {
        mLength = other.mLength;
        fl::memcpy(inlineBufferPtr(), other.inlineBufferPtr(), other.mLength + 1);
        // mStorage is already empty (inline mode) from constructor
    } else {
        mLength = other.mLength;
        mStorage = fl::move(other.mStorage);
    }
    other.mLength = 0;
    other.mStorage.reset();
    other.inlineBufferPtr()[0] = '\0';
}

void basic_string::moveAssign(basic_string&& other) FL_NO_EXCEPT {
    if (this == &other) return;
    if (other.isInline()) {
        mLength = other.mLength;
        mStorage.reset();
        fl::memcpy(inlineBufferPtr(), other.inlineBufferPtr(), other.mLength + 1);
    } else {
        mLength = other.mLength;
        mStorage = fl::move(other.mStorage);
    }
    other.mLength = 0;
    other.mStorage.reset();
    other.inlineBufferPtr()[0] = '\0';
}

void basic_string::setLiteral(const char* literal) {
    if (literal) {
        mLength = fl::strlen(literal);
        mStorage = ConstLiteral(literal);
    }
}

void basic_string::setView(const char* data, fl::size len) {
    if (data && len > 0) {
        mLength = len;
        mStorage = ConstView(data, len);
    }
}

void basic_string::setSharedHolder(const fl::shared_ptr<StringHolder>& holder) {
    if (!holder || holder->length() == 0) return;
    mLength = holder->length();
    mStorage = NotNullStringHolderPtr(holder);
}

// ======= APPEND =======

basic_string& basic_string::append(const char* str) {
    write(str, fl::strlen(str));
    return *this;
}

basic_string& basic_string::append(const char* str, fl::size len) {
    write(str, len);
    return *this;
}

basic_string& basic_string::append(char c) {
    write(&c, 1);
    return *this;
}

basic_string& basic_string::append(const i8& val) {
    write(val);
    return *this;
}

basic_string& basic_string::append(const u8& val) {
    write(static_cast<fl::u16>(val));
    return *this;
}

basic_string& basic_string::append(const bool& val) {
    if (val) {
        write("true", 4);
    } else {
        write("false", 5);
    }
    return *this;
}

basic_string& basic_string::append(const i16& val) {
    write(static_cast<fl::i32>(val));
    return *this;
}

basic_string& basic_string::append(const u16& val) {
    write(val);
    return *this;
}

basic_string& basic_string::append(const i32& val) {
    write(val);
    return *this;
}

basic_string& basic_string::append(const u32& val) {
    write(val);
    return *this;
}

basic_string& basic_string::append(const i64& val) {
    write(val);
    return *this;
}

basic_string& basic_string::append(const u64& val) {
    write(val);
    return *this;
}

basic_string& basic_string::append(const basic_string& str) {
    write(str.c_str(), str.size());
    return *this;
}

// ======= HEX/OCT APPEND =======

basic_string& basic_string::appendHex(i32 val) { char b[64]={0}; int l=fl::itoa(val,b,16); write(b,l); return *this; }
basic_string& basic_string::appendHex(u32 val) { char b[64]={0}; int l=fl::utoa32(val,b,16); write(b,l); return *this; }
basic_string& basic_string::appendHex(i64 val) { char b[64]={0}; int l=fl::itoa64(val,b,16); write(b,l); return *this; }
basic_string& basic_string::appendHex(u64 val) { char b[64]={0}; int l=fl::utoa64(val,b,16); write(b,l); return *this; }
basic_string& basic_string::appendHex(i16 val) { return appendHex(static_cast<i32>(val)); }
basic_string& basic_string::appendHex(u16 val) { return appendHex(static_cast<u32>(val)); }
basic_string& basic_string::appendHex(i8 val) { return appendHex(static_cast<i32>(val)); }
basic_string& basic_string::appendHex(u8 val) { return appendHex(static_cast<u32>(val)); }

basic_string& basic_string::appendOct(i32 val) { char b[64]={0}; int l=fl::itoa(val,b,8); write(b,l); return *this; }
basic_string& basic_string::appendOct(u32 val) { char b[64]={0}; int l=fl::utoa32(val,b,8); write(b,l); return *this; }
basic_string& basic_string::appendOct(i64 val) { char b[64]={0}; int l=fl::itoa64(val,b,8); write(b,l); return *this; }
basic_string& basic_string::appendOct(u64 val) { char b[64]={0}; int l=fl::utoa64(val,b,8); write(b,l); return *this; }
basic_string& basic_string::appendOct(i16 val) { return appendOct(static_cast<i32>(val)); }
basic_string& basic_string::appendOct(u16 val) { return appendOct(static_cast<u32>(val)); }
basic_string& basic_string::appendOct(i8 val) { return appendOct(static_cast<i32>(val)); }
basic_string& basic_string::appendOct(u8 val) { return appendOct(static_cast<u32>(val)); }

// ======= OTHER =======

float basic_string::toFloat() const { return fl::parseFloat(c_str(), mLength); }

} // namespace fl
