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
    JsonNumberFieldImpl(const fl::string &name, float value, float min, float max) FL_NOEXCEPT;
    ~JsonNumberFieldImpl();
    JsonNumberFieldImpl &Group(const fl::string &name) FL_NOEXCEPT;

    const fl::string &name() const FL_NOEXCEPT;
    void toJson(fl::json &json) const FL_NOEXCEPT;
    float value() const FL_NOEXCEPT;
    void setValue(float value) FL_NOEXCEPT;
    fl::string groupName() const FL_NOEXCEPT;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName) FL_NOEXCEPT;

    int id() const FL_NOEXCEPT;

    JsonNumberFieldImpl &operator=(float value) FL_NOEXCEPT;
    JsonNumberFieldImpl &operator=(int value) FL_NOEXCEPT;
    // Use ALMOST_EQUAL_FLOAT for floating-point comparison
    bool operator==(float v) const FL_NOEXCEPT;
    bool operator==(int v) const FL_NOEXCEPT;
    bool operator!=(float v) const FL_NOEXCEPT;
    bool operator!=(int v) const FL_NOEXCEPT;

  private:
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiNumberFieldInternal> mInternal = nullptr;
};

} // namespace fl
