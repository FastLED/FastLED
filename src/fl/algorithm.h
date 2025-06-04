#pragma once

#include "fl/type_traits.h"

namespace fl {

template <typename Iterator>
void reverse(Iterator first, Iterator last) {
    while ((first != last) && (first != --last)) {
        swap(*first++, *last);
    }
}

} // namespace fl
