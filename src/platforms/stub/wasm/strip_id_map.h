#pragma once

#include <map>
#include <stdint.h>
#include <stddef.h>

#include "singleton.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN


class StripIdMap {
public:
    static int add(uintptr_t owner) {
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
    static uintptr_t getOwner(int id) {
        StripIdMap& instance = Instance();
        auto find = instance.mOwnerMap.find(id);
        if (find != instance.mOwnerMap.end()) {
            return find->second;
        }
        return 0;
    }

    /// -1 if not found
    static int getId(uintptr_t owner) {
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
    std::map<uintptr_t, int> mStripMap;
    std::map<int, uintptr_t> mOwnerMap;
    int mCounter = 0;
};

FASTLED_NAMESPACE_END