#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "fl/singleton.h"
#include "fl/namespace.h"
#include "fl/map.h"

// Define a reasonable maximum number of strips

FASTLED_NAMESPACE_BEGIN

class CLEDController;
extern uint16_t cled_contoller_size();

class StripIdMap {
public:
    static const int MAX_STRIPS = 64;
    static int addOrGetId(CLEDController* owner) {
        StripIdMap& instance = Instance();
        int id;
        if (instance.mStripMap.get(owner, &id)) {
            return id;
        }
        id = instance.mCounter++;
        printf("Adding strip id: %d\n", id);
        instance.mStripMap.update(owner, id);
        instance.mOwnerMap.update(id, owner);
        return id;
    }

    // 0 if not found
    static CLEDController* getOwner(int id) {
        StripIdMap& instance = Instance();
        CLEDController* owner;
        if (instance.mOwnerMap.get(id, &owner)) {
            return owner;
        }
        return nullptr;
    }

    /// -1 if not found
    static int getId(CLEDController* owner) {
        StripIdMap& instance = Instance();
        int id;
        if (instance.mStripMap.get(owner, &id)) {
            return id;
        }
        return -1;
    }

    static int getOrFindByAddress(uintptr_t address) {
        if (address == 0) {
            return -1;
        }
        int id = getId(reinterpret_cast<CLEDController*>(address));
        if (id >= 0) {
            return id;
        }
        return spiFindIdOrMakeIt(address);
    }

    static CLEDController* getOwnerByAddress(uintptr_t spi_address) {
        // spiDevice is going to be a member of the subclass of CLEDController. So
        // to find the device we need to iterate over the map and compare the spiDevice pointer
        // to the pointer address of all the CLedController objects.
        // Note that the device should already have been added by the time this function is called.
        StripIdMap& instance = Instance();
        uint16_t controller_size = cled_contoller_size();
        uint16_t smallest_diff = 0xFFFF;
        CLEDController* closest_controller = nullptr;
        
        for (auto it = instance.mStripMap.begin(); it != instance.mStripMap.end(); ++it) {
            CLEDController* controller = it->first;
            uintptr_t address_subclass = reinterpret_cast<uintptr_t>(controller) + controller_size;
            // if below, then the spiDevice is NOT a member of the subclass of CLEDController
            if (spi_address < address_subclass) {
                continue;
            }
            uintptr_t diff = spi_address - address_subclass;
            if (diff < smallest_diff) {
                smallest_diff = diff;
                closest_controller = controller;
            }
        }
        if (closest_controller && smallest_diff < controller_size) {
            return closest_controller;
        }
        return nullptr;
    }

    static int spiFindIdOrMakeIt(uintptr_t spi_address) {
        CLEDController* closest_controller = getOwnerByAddress(spi_address);
        if (closest_controller) {
            int id = addOrGetId(closest_controller);
            //if (instance.mStripMap.get(closest_controller, &id)) {
            //    return id;
            //}
            return id;
        }
        return -1;
    }

    #ifdef FASTLED_TESTING
    static void test_clear() {
        Instance().mStripMap.clear();
        Instance().mOwnerMap.clear();
        Instance().mCounter = 0;
    }
    #endif

private:
    static StripIdMap& Instance() {
        return fl::Singleton<StripIdMap>::instance();
    }
    fl::FixedMap<CLEDController*, int, MAX_STRIPS> mStripMap;
    fl::FixedMap<int, CLEDController*, MAX_STRIPS> mOwnerMap;
    int mCounter = 0;
};

FASTLED_NAMESPACE_END
