#include "title.h"
#include "platforms/shared/ui/json/ui.h"
#include "fl/json.h"

#if FASTLED_ENABLE_JSON

namespace fl {

// Constructor implementation
JsonTitleImpl::JsonTitleImpl(const fl::string& name, const fl::string& text) {
    // Create an instance of the new internal class
    mInternal = fl::make_shared<JsonUiTitleInternal>(name, text);

    // Register the component with the JsonUiManager
    addJsonUiComponent(mInternal);
}

// Destructor implementation
JsonTitleImpl::~JsonTitleImpl() {
    // Ensure the component is removed from the global registry
    removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
    
    // No need to clear functions anymore as there are no lambdas capturing 'this'
    // if (mInternal) {
    //     mInternal->clearFunctions();
    // }
}

JsonTitleImpl &JsonTitleImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const string &JsonTitleImpl::name() const { return mInternal->name(); }

const fl::string &JsonTitleImpl::groupName() const { return mInternal->groupName(); }

// Delegate text accessors to the internal object
const fl::string& JsonTitleImpl::text() const {
    return mInternal->text();
}

void JsonTitleImpl::setText(const fl::string& text) {
    mInternal->setText(text);
}

void JsonTitleImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

} // namespace fl

#endif
