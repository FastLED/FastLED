#pragma once
#include <ostream>

namespace fl {

#ifndef FASTLED_HAS_DPRINT
// make a dummy class for DBG. It will be used like this:
// example: FASTLED_DPRINT() << "Hello World" << endl;


class DBG {
public:
    // Constructor does nothing
    DBG() {}
    
    // Dummy stream operator that returns *this to allow chaining
    template<typename T>
    DBG& operator<<(const T&) { return *this; }
    
    // Special handling for endl-like manipulators
    DBG& operator<<(std::ostream& (*manip)(std::ostream&)) { return *this; }
};

// Define the print macro
#define FASTLED_DPRINT() fl::DBG()

#endif


}  // namespace fl

