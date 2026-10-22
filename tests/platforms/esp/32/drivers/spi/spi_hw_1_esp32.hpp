#pragma once
#include "FastLED.h"
#include "fl/stl/json.h"
#include "platforms/shared/spi_hw_1.h"
#include "driver/spi_master.h"
#include "SpiProviderFaults.hpp"
namespace spi_provider_probe {
inline fl::json probe(const fl::json&) {
    const auto heapStart = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    auto& faults = spi_provider_faults::state();
    faults.spiTask = xTaskGetCurrentTaskHandle();
    fl::shared_ptr<fl::SpiHw1> controller;
    for (const auto& candidate : fl::SpiHw1::getAll()) if (candidate->getBusId() == 2) controller = candidate;
    bool began = false; bool queued = false; bool timedOut = false; bool retained = false; bool reaped = false; bool recoveredByPoll = false; bool allocationRejected = false;
    if (controller) {
        fl::SpiHw1::Config config; config.bus_num=2;config.data_pin=4;config.clock_pin=18;config.clock_speed_hz=1000000;config.max_transfer_sz=4096;
        began=controller->begin(config);
        if (began) {
            faults.heapTask = xTaskGetCurrentTaskHandle();
            faults.heapSize = 4096;
            const auto failuresBefore = faults.heapFailures;
            auto failedBuffer = controller->acquireDMABuffer(4096);
            faults.heapTask = nullptr;
            allocationRejected = faults.heapFailures > failuresBefore && !failedBuffer.ok() && !controller->transmit();
            failedBuffer.reset();
            auto buffer=controller->acquireDMABuffer(257);
            if(buffer.ok() && buffer.size()==257) {
                for(unsigned i=0;i<257;++i) buffer.data()[i]=static_cast<unsigned char>(i^0xa5u);
                queued=controller->transmit();
                buffer.reset(); // Driver is the only remaining owner of the DMA storage.
                if(queued) {
                    faults.spiTimeouts=2;
                    timedOut=!controller->waitComplete(0);
                    retained=controller->isBusy();
                    reaped=controller->waitComplete(2000) && !controller->isBusy();
                }
            }
            if (retained && reaped) {
                auto next = controller->acquireDMABuffer(4096);
                if (next.ok() && next.size()==4096) {
                    fl::memset(next.data().data(),0xa5,4096);
                    const bool started=controller->transmit();
                    next.reset();
                    faults.spiTimeouts=2;
                    const bool retryTimeout=started && !controller->waitComplete(0);
                    const auto start=millis();
                    while (controller->isBusy() && millis()-start<1000) delay(1);
                    recoveredByPoll=retryTimeout && !controller->isBusy();
                }
                controller->end();
            }
        }
    }
    faults.spiTask = nullptr;
    const auto heapEnd = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    fl::json result = fl::json::object();
    result.set("provider",false);
    result.set("heapStart",static_cast<unsigned>(heapStart));
    result.set("heapEnd",static_cast<unsigned>(heapEnd));
    result.set("began",began);result.set("queued",queued);result.set("timedOut",timedOut);result.set("retained",retained);result.set("reaped",reaped);
    result.set("allocationFaults",faults.heapFailures);
    result.set("allocationRejected",allocationRejected);
    result.set("pollRecovered",recoveredByPoll);
    result.set("success",began&&queued&&timedOut&&retained&&reaped&&recoveredByPoll&&allocationRejected);
    return result;
}
}
