#pragma once



#include "fl/singleton.h"
#include "platforms/wasm/engine_listener.h"

#include "fl/map.h"
#include "fl/ptr.h"
#include "fl/set.h"

#include "fl/json.h"
#include "platforms/wasm/js.h"

namespace fl {
namespace ui_detail {

#ifdef __EMSCRIPTEN__
// use std::string when in emscripten, else the js engine can't bind to this class.
#include <string>
using string = std::string;
#else
using string = fl::string;
#endif

} // namespace detail


class jsUiInternal;

class jsUiManager : fl::EngineEvents::Listener {
  public:
    void addComponent(fl::WeakPtr<jsUiInternal> component);
    void removeComponent(fl::WeakPtr<jsUiInternal> component);

    // Called from the JS engine.
    static void jsUpdateUiComponents(const fl::ui_detail::string &jsonStr) {
        updateUiComponents(jsonStr.c_str());
    }
    // Internal representation.
    static void updateUiComponents(const char *jsonStr);

    static jsUiManager &instance();

  private:
    static void executeUiUpdates(const FLArduinoJson::JsonDocument &doc);
    typedef fl::FixedSet<fl::WeakPtr<jsUiInternal>, 64> jsUIRefSet;
    friend class fl::Singleton<jsUiManager>;
    jsUiManager() { fl::EngineEvents::addListener(this); }
    ~jsUiManager() { fl::EngineEvents::removeListener(this); }

    void onPlatformPreLoop() override {
        if (!mHasPendingUpdate) {
            return;
        }
        jsUiManager::executeUiUpdates(mPendingJsonUpdate);
        mPendingJsonUpdate.clear();
        mHasPendingUpdate = false;
    }

    void onEndShowLeds() override {
        if (mItemsAdded) {
            FLArduinoJson::JsonDocument doc;
            FLArduinoJson::JsonArray jarray =
                doc.to<FLArduinoJson::JsonArray>();
            toJson(jarray);
            fl::string buff;
            FLArduinoJson::serializeJson(doc, buff);
            fl::updateJs(buff.c_str());
            mItemsAdded = false;
        }
    }

    fl::vector<jsUiInternalPtr> getComponents();
    void toJson(FLArduinoJson::JsonArray &json);

    jsUIRefSet mComponents;
    fl::mutex mMutex;


    bool mItemsAdded = false;
    FLArduinoJson::JsonDocument mPendingJsonUpdate;
    bool mHasPendingUpdate = false;
};

} // namespace fl
