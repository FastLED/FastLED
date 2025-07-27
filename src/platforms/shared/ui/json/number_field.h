#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/math_macros.h"
#include "fl/json.h"
#include "fl/ptr.h" // For fl::shared_ptr

namespace fl {

// Forward declaration of the internal class
class JsonUiNumberFieldInternal;

class JsonNumberFieldImpl {
  public:
    JsonNumberFieldImpl(const fl::string &name, double value, double min, double max);
    ~JsonNumberFieldImpl();
    JsonNumberFieldImpl &Group(const fl::string &name);

    const fl::string &name() const;
    void toJson(fl::Json &json) const;
    double value() const;
    void setValue(double value);
    const fl::string &groupName() const;
    
    // Method to allow parent UIElement class to set the group
    void setGroup(const fl::string &groupName);

    int id() const;

    JsonNumberFieldImpl &operator=(double value);
    JsonNumberFieldImpl &operator=(int value);
    // Use ALMOST_EQUAL_FLOAT for floating-point comparison
    bool operator==(double v) const;
    bool operator==(int v) const;
    bool operator!=(double v) const;
    bool operator!=(int v) const;

  private:
    // Change to use the specific internal implementation
    fl::shared_ptr<JsonUiNumberFieldInternal> mInternal;
};

} // namespace fl
