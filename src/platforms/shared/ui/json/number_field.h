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
class JsonUiNumberFieldInternal;

class JsonNumberFieldImpl {
  public:
    JsonNumberFieldImpl(const fl::string &name, float value, float min, float max) FL_NO_EXCEPT;
    ~JsonNumberFieldImpl();
    JsonNumberFieldImpl &Group(const fl::string &name) FL_NO_EXCEPT;

    const fl::string &name() const FL_NO_EXCEPT;
    void toJson(fl::json &json) const FL_NO_EXCEPT;
    float value() const FL_NO_EXCEPT;
    void setValue(float value) FL_NO_EXCEPT;
    fl::string groupName() const FL_NO_EXCEPT;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName) FL_NO_EXCEPT;

    int id() const FL_NO_EXCEPT;

    JsonNumberFieldImpl &operator=(float value) FL_NO_EXCEPT;
    JsonNumberFieldImpl &operator=(int value) FL_NO_EXCEPT;
    // Use ALMOST_EQUAL_FLOAT for floating-point comparison
    bool operator==(float v) const FL_NO_EXCEPT;
    bool operator==(int v) const FL_NO_EXCEPT;
    bool operator!=(float v) const FL_NO_EXCEPT;
    bool operator!=(int v) const FL_NO_EXCEPT;

  private:
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiNumberFieldInternal> mInternal = nullptr;
};

} // namespace fl
