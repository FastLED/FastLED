

#include "fl/scoped_ptr.h"
#include "fl/str.h"
#include "fl/warn.h"



namespace fl {

namespace scoped_ptr_detail {
void LOG_POINTER_DELETE(void* ptr) {
    if (!ptr) {
        return;
    }
    uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
    FASTLED_WARN("Deleting ptr: " << address);
}

void LOG_POINTER_NEW(void* ptr) {
    if (!ptr) {
        return;
    }
    uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
    FASTLED_WARN("Newed ptr: " << address);
}

}  // namespace scoped_ptr_detail
}  // namespace fl
