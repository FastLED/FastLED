#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/math_macros.h"
#include "fl/json.h"
#include "fl/ptr.h" // For fl::shared_ptr

namespace fl {

// Forward declaration of the internal class
class JsonUiSliderInternal;

class JsonSliderImpl {
  public:
    JsonSliderImpl(const fl::string &name, float value, float min, float max,
                 float step = -1);
    ~JsonSliderImpl();
    JsonSliderImpl &Group(const fl::string &name);

    const fl::string &name() const;
    void toJson(fl::Json &json) const;
    float value() const;
    float value_normalized() const;
    float getMax() const;
    float getMin() const;
    void setValue(float value);
    const fl::string &groupName() const;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

    int id() const;

    int as_int() const;
    JsonSliderImpl &operator=(float value);
    JsonSliderImpl &operator=(int value);

  private:
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiSliderInternal> mInternal;
};

} // namespace fl
