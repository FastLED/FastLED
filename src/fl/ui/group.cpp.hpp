#include "fl/ui/group.h"
#include "fl/stl/noexcept.h"

namespace fl {

UIGroup::UIGroup(const fl::string& groupName) FL_NO_EXCEPT : mImpl(groupName.c_str()) {}
UIGroup::~UIGroup() FL_NO_EXCEPT {}

} // namespace fl
