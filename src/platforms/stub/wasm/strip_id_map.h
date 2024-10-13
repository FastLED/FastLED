#pragma once

#include <map>
#include <stdint.h>
#include <stddef.h>

#include "singleton.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

class CLEDController;

class StripIdMap {
public:
    static int add(CLEDController* owner) {
        StripIdMap& instance = Instance();
        auto find = instance.mStripMap.find(owner);
        if (find != instance.mStripMap.end()) {
            return find->second;
        }
        int id = instance.mCounter++;
        instance.mStripMap[owner] = id;
        instance.mOwnerMap[id] = owner;
        return id;
    }

    // 0 if not found
    static CLEDController* getOwner(int id) {
        StripIdMap& instance = Instance();
        auto find = instance.mOwnerMap.find(id);
        if (find != instance.mOwnerMap.end()) {
            return find->second;
        }
        return 0;
    }

    /// -1 if not found
    static int getId(CLEDController* owner) {
        StripIdMap& instance = Instance();
        auto find = instance.mStripMap.find(owner);
        if (find != instance.mStripMap.end()) {
            return find->second;
        }
        return -1;
    }

private:
    static StripIdMap& Instance() {
        return Singleton<StripIdMap>::instance();
    }
    std::map<CLEDController*, int> mStripMap;
    std::map<int, CLEDController*> mOwnerMap;
    int mCounter = 0;
};

FASTLED_NAMESPACE_END