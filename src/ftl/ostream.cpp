#include "ftl/ostream.h"

namespace fl {

// Global cout instance for immediate output
ostream cout;

// endl manipulator instance
const endl_t endl;

// Numeric output operators with formatting support
ostream& ostream::operator<<(fl::i8 n) {
    string temp;
    if (mBase == 16) {
        StringFormatter::appendHex(fl::i16(n), &temp);
    } else if (mBase == 8) {
        StringFormatter::appendOct(fl::i16(n), &temp);
    } else {
        temp.append(fl::i16(n));
    }
    print(temp.c_str());
    return *this;
}

ostream& ostream::operator<<(fl::u8 n) {
    string temp;
    if (mBase == 16) {
        StringFormatter::appendHex(fl::u16(n), &temp);
    } else if (mBase == 8) {
        StringFormatter::appendOct(fl::u16(n), &temp);
    } else {
        temp.append(fl::u16(n));
    }
    print(temp.c_str());
    return *this;
}

ostream& ostream::operator<<(fl::i16 n) {
    string temp;
    if (mBase == 16) {
        StringFormatter::appendHex(n, &temp);
    } else if (mBase == 8) {
        StringFormatter::appendOct(n, &temp);
    } else {
        temp.append(n);
    }
    print(temp.c_str());
    return *this;
}

ostream& ostream::operator<<(fl::i32 n) {
    string temp;
    if (mBase == 16) {
        StringFormatter::appendHex(n, &temp);
    } else if (mBase == 8) {
        StringFormatter::appendOct(n, &temp);
    } else {
        temp.append(n);
    }
    print(temp.c_str());
    return *this;
}

ostream& ostream::operator<<(fl::u32 n) {
    string temp;
    if (mBase == 16) {
        StringFormatter::appendHex(n, &temp);
    } else if (mBase == 8) {
        StringFormatter::appendOct(n, &temp);
    } else {
        temp.append(n);
    }
    print(temp.c_str());
    return *this;
}

// Manipulator operator implementations (declared as friends in ostream)
ostream& operator<<(ostream& os, const hex_t&) {
    os.mBase = 16;
    return os;
}

ostream& operator<<(ostream& os, const dec_t&) {
    os.mBase = 10;
    return os;
}

ostream& operator<<(ostream& os, const oct_t&) {
    os.mBase = 8;
    return os;
}

} // namespace fl
