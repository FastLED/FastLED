#pragma once

// IWYU pragma: private

#include "fl/system/engine_events.h"
#include "fl/stl/string.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/math/math.h"
#include "fl/stl/json.h"
#include "fl/stl/shared_ptr.h" // For fl::shared_ptr
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declaration of the internal class
class JsonUiSliderInternal;

class JsonSliderImpl {
  public:
    JsonSliderImpl(const fl::string &name, float value, float min, float max,
                 float step = -1) FL_NO_EXCEPT;
    ~JsonSliderImpl();
    JsonSliderImpl &Group(const fl::string &name) FL_NO_EXCEPT;

    const fl::string &name() const FL_NO_EXCEPT;
    void toJson(fl::json &json) const FL_NO_EXCEPT;
    float value() const FL_NO_EXCEPT;
    float value_normalized() const FL_NO_EXCEPT;
    float getMax() const FL_NO_EXCEPT;
    float getMin() const FL_NO_EXCEPT;
    void setValue(float value) FL_NO_EXCEPT;
    fl::string groupName() const FL_NO_EXCEPT;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName) FL_NO_EXCEPT;

    int id() const FL_NO_EXCEPT;

    int as_int() const FL_NO_EXCEPT;
    JsonSliderImpl &operator=(float value) FL_NO_EXCEPT;
    JsonSliderImpl &operator=(int value) FL_NO_EXCEPT;

  private:
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiSliderInternal> mInternal;
};

} // namespace fl
