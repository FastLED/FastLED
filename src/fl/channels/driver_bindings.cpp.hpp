// IWYU pragma: private

#include "fl/channels/driver_bindings.h"

#include "fl/stl/singleton.h"

namespace fl {

DriverBindings& DriverBindings::instance() FL_NO_EXCEPT {
    return Singleton<DriverBindings>::instance();
}

void DriverBindings::push(DriverBinding& node) FL_NO_EXCEPT {
    if (node.listed) {
        return;
    }
    node.listed = true;
    node.next = mHead;
    mHead = &node;
}

bool DriverBindings::bind(Bus bus, u8 which) FL_NO_EXCEPT {
    bool matched = false;
    for (DriverBinding* node = mHead; node != nullptr; node = node->next) {
        if (bus != Bus::AUTO && (node->bus != bus || node->which != which)) {
            continue;
        }
        matched = true;
        // Registration deduplicates by name, so binding again is cheap and
        // re-registers a driver a cleared manager dropped.
        node->bind();
    }
    return matched;
}

}  // namespace fl
