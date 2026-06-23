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
                 float step = -1) FL_NOEXCEPT;
    ~JsonSliderImpl();
    JsonSliderImpl &Group(const fl::string &name) FL_NOEXCEPT;

    const fl::string &name() const FL_NOEXCEPT;
    void toJson(fl::json &json) const FL_NOEXCEPT;
    float value() const FL_NOEXCEPT;
    float value_normalized() const FL_NOEXCEPT;
    float getMax() const FL_NOEXCEPT;
    float getMin() const FL_NOEXCEPT;
    void setValue(float value) FL_NOEXCEPT;
    fl::string groupName() const FL_NOEXCEPT;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName) FL_NOEXCEPT;

    int id() const FL_NOEXCEPT;

    int as_int() const FL_NOEXCEPT;
    JsonSliderImpl &operator=(float value) FL_NOEXCEPT;
    JsonSliderImpl &operator=(int value) FL_NOEXCEPT;

  private:
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiSliderInternal> mInternal;
};

} // namespace fl
