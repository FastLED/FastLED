#pragma once
#include "FastLED.h"
#include "fl/stl/json.h"
#include "fl/stl/memory.h"
#include "fl/channels/rx/channel.h"
#include "platforms/arduino/spi/spi_arduino.hpp"
#include "platforms/esp/32/drivers/spi/spi_provider.h"
#include "SpiProviderFaults.hpp"
#include "driver/spi_master.h"
#include "driver/spi_slave.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"
namespace spi_provider_probe {
inline bool& registered() { static bool value = false; return value; }
inline unsigned& calls() { static unsigned count = 0; return count; }
inline fl::shared_ptr<fl::platforms::ArduinoSpiBus>& busOwner() { static fl::shared_ptr<fl::platforms::ArduinoSpiBus> owner; return owner; }
inline fl::shared_ptr<SPIClass>& rawOwner() { static fl::shared_ptr<SPIClass> owner; return owner; }
class Witness : public fl::Selectable {
    int mPin; unsigned mHz; unsigned mMode; unsigned mOrder;
public:
    unsigned selects = 0; unsigned releases = 0; bool settings = true; bool pin = true;
    Witness(int cs, unsigned hz, unsigned mode, unsigned order) : mPin(cs),mHz(hz),mMode(mode),mOrder(order) {
        gpio_set_direction(static_cast<gpio_num_t>(mPin), GPIO_MODE_INPUT_OUTPUT);
        gpio_set_level(static_cast<gpio_num_t>(mPin), 1);
    }
    void select() noexcept override {
        ++selects;
        auto handle = rawOwner()->bus();
        settings &= spiGetDataMode(handle) == mMode && spiGetBitOrder(handle) == mOrder && spiClockDivToFrequency(spiGetClockDiv(handle)) == mHz;
        gpio_set_level(static_cast<gpio_num_t>(mPin), 0);
        pin &= gpio_get_level(static_cast<gpio_num_t>(mPin)) == 0;
    }
    void release() noexcept override {
        ++releases;
        gpio_set_level(static_cast<gpio_num_t>(mPin), 1);
        pin &= gpio_get_level(static_cast<gpio_num_t>(mPin)) == 1;
    }
    bool isSelected() noexcept override { return gpio_get_level(static_cast<gpio_num_t>(mPin)) == 0; }
};
inline fl::shared_ptr<Witness>& ledSelect() { static fl::shared_ptr<Witness> select; return select; }
struct PeerOperation {
    fl::shared_ptr<fl::platforms::ArduinoSpiBus> owner;
    fl::shared_ptr<Witness> select;
    SemaphoreHandle_t entered;
    SemaphoreHandle_t finished;
    bool success = false;
    bool attemptedWhileLocked = false;
};
inline void runPeer(void* argument) {
    auto& peer = *static_cast<PeerOperation*>(argument);
    auto& fault = spi_provider_faults::state();
    fault.guardOwner = xTaskGetCurrentTaskHandle();
    fault.guardObserved.store(false);
    peer.success = peer.owner->withBus([&](SPIClass& bus) -> bool {
        bus.beginTransaction(SPISettings(1000000, LSBFIRST, SPI_MODE3));
        peer.select->select();
        xSemaphoreGive(peer.entered);
        // Keep the complete transaction locked while the LED caller starts.
        const auto started = xTaskGetTickCount();
        while (!fault.guardObserved.load() && xTaskGetTickCount() - started < pdMS_TO_TICKS(1000))
            vTaskDelay(1);
        peer.attemptedWhileLocked = fault.guardObserved.load();
        unsigned char bytes[257];
        for (unsigned i = 0; i < sizeof(bytes); ++i) bytes[i] = i ^ 0xa5u;
        bus.writeBytes(bytes, sizeof(bytes));
        peer.select->release();
        bus.endTransaction();
        return true;
    });
    fault.guardOwner = nullptr;
    fault.guardQueue = nullptr;
    xSemaphoreGive(peer.finished);
    vTaskDelete(nullptr);
}
inline fl::json probe(const fl::json&) {
    if (!busOwner()) {
        rawOwner() = fl::make_shared<SPIClass>(HSPI);
        rawOwner()->begin(18, -1, 4, -1);
        rawOwner()->setHwCs(false);
        busOwner() = fl::make_shared<fl::platforms::ArduinoSpiBus>(rawOwner(), 2, 4, 18);
        ledSelect() = fl::make_shared<Witness>(19, 500000, SPI_MODE0, MSBFIRST);
    }
    const bool ownerReady = busOwner() && busOwner()->ready();
    const auto heapStart = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    bool contracts = true;
    {
        auto& fault = spi_provider_faults::state();
        fault.queueTask = xTaskGetCurrentTaskHandle();
        const auto mutexFailures = fault.queueFailures;
        auto noMutex = fl::make_shared<fl::platforms::ArduinoSpiBus>(rawOwner(), 2, 4, 18);
        fault.queueTask = nullptr;
        contracts &= fault.queueFailures == mutexFailures + 1 && !noMutex->valid();
        auto invalid = fl::make_shared<fl::platforms::ArduinoSpiBus>(rawOwner(), 1, 4, 18);
        contracts &= !invalid->valid() && !fl::platforms::makeArduinoSpiHw1(invalid);
        contracts &= !busOwner()->withBus([](SPIClass&) -> bool { return false; });
        contracts &= busOwner()->ready(); // Failure must release the shared guard.
        const auto selections = ledSelect()->selects;
        contracts &= busOwner()->write({}, 500000, MSBFIRST, SPI_MODE0, ledSelect());
        contracts &= !busOwner()->write({}, 0, MSBFIRST, SPI_MODE0, ledSelect());
        contracts &= ledSelect()->selects == selections;
        auto temporary = fl::platforms::makeArduinoSpiHw1(busOwner());
        fl::SpiHw1::Config config;
        config.bus_num = 2; config.data_pin = 4; config.clock_pin = 18;
        config.clock_speed_hz = 500000; config.max_transfer_sz = 4096;
        contracts &= temporary && temporary->begin(config) && !temporary->transmit();
        auto tooLarge = temporary->acquireDMABuffer(4097);
        contracts &= !tooLarge.ok();
        fault.heapTask = xTaskGetCurrentTaskHandle(); fault.heapSize = 4096;
        const auto allocationFailures = fault.heapFailures;
        auto failed = temporary->acquireDMABuffer(4096);
        fault.heapTask = nullptr;
        contracts &= fault.heapFailures > allocationFailures && !failed.ok() && !temporary->transmit();
        temporary->end();
        config.clock_speed_hz = 1000000;
        contracts &= temporary->begin(config);
        temporary->end();
        contracts &= busOwner()->ready(); // Adapter release must not end the app bus.
        auto borrowed = fl::make_shared<fl::platforms::ArduinoSpiBus>(rawOwner(), 2, 4, 18);
        auto retained = fl::platforms::makeArduinoSpiHw1(borrowed);
        borrowed.reset();
        contracts &= retained && retained->begin(config);
        retained.reset();
        contracts &= busOwner()->ready();

    }

    auto clientSelect = fl::make_shared<Witness>(21, 1000000, SPI_MODE3, LSBFIRST);
    unsigned char bytes[257];
    for (unsigned i=0;i<sizeof(bytes);++i) bytes[i]=static_cast<unsigned char>(i^0xa5u);
    const bool secondBefore = busOwner()->write(bytes, 1000000, LSBFIRST, SPI_MODE3, clientSelect);
    static CRGB leds[32];
    for(unsigned i=0;i<32;++i) leds[i]=CRGB(i*7u,i*3u,i*11u);
    if(!registered()) {
        auto& controller = FastLED.addLeds<APA102,4,18,RGB,DATA_RATE_KHZ(500)>(leds,32,fl::Esp32SpiBus::HOST2);
        controller.setCorrection(UncorrectedColor).setTemperature(UncorrectedTemperature).setDither(0);
        registered() = true;
    }
    FastLED.setBrightness(255);
    FastLED.show();
    bool completed = FastLED.wait(1000);
    const auto dataRoute = GPIO.func_out_sel_cfg[4].val;
    const auto clockRoute = GPIO.func_out_sel_cfg[18].val;
    const auto csRoute = GPIO.func_out_sel_cfg[19].val;
    spi_bus_config_t captureBus = {}; captureBus.mosi_io_num=4;captureBus.miso_io_num=-1;captureBus.sclk_io_num=18;captureBus.quadwp_io_num=-1;captureBus.quadhd_io_num=-1;captureBus.max_transfer_sz=256;captureBus.flags=SPICOMMON_BUSFLAG_GPIO_PINS;
    spi_slave_interface_config_t captureConfig={};captureConfig.spics_io_num=19;captureConfig.queue_size=1;captureConfig.mode=0;
    const auto captureStatus=spi_slave_initialize(SPI3_HOST,&captureBus,&captureConfig,2);
    const bool armed=captureStatus==ESP_OK;
    auto actual=static_cast<unsigned char*>(heap_caps_malloc(256,MALLOC_CAP_DMA));
    if(actual) fl::memset(actual,0,256);
    spi_slave_transaction_t transaction={};transaction.length=256*8;transaction.rx_buffer=actual;
    bool queued=armed&&actual&&spi_slave_queue_trans(SPI3_HOST,&transaction,portMAX_DELAY)==ESP_OK;
    GPIO.func_out_sel_cfg[4].val=dataRoute;GPIO.func_out_sel_cfg[18].val=clockRoute;GPIO.func_out_sel_cfg[19].val=csRoute;
    gpio_ll_output_enable(&GPIO,GPIO_NUM_4);gpio_ll_output_enable(&GPIO,GPIO_NUM_18);gpio_ll_output_enable(&GPIO,GPIO_NUM_19);
    PeerOperation peer;
    peer.owner = busOwner(); peer.select = clientSelect;
    peer.entered = xSemaphoreCreateBinary(); peer.finished = xSemaphoreCreateBinary();
    const bool peerStarted = peer.entered && peer.finished &&
        xTaskCreate(runPeer, "spi-peer", 3072, &peer, 2, nullptr) == pdPASS;
    const bool contended = peerStarted && xSemaphoreTake(peer.entered, pdMS_TO_TICKS(1000)) == pdTRUE;
    const auto frameStart = esp_timer_get_time();
    FastLED.show();completed &= FastLED.wait(1000);
    const auto frameMicros = esp_timer_get_time() - frameStart;
    if(peerStarted) xSemaphoreTake(peer.finished, portMAX_DELAY);
    const bool peerPassed = contended && peer.success && peer.attemptedWhileLocked;
    if(peer.entered) vSemaphoreDelete(peer.entered);
    if(peer.finished) vSemaphoreDelete(peer.finished);
    // Let the idle task reclaim the completed peer's FreeRTOS task storage.
    vTaskDelay(pdMS_TO_TICKS(20));
    spi_slave_transaction_t* received=nullptr;
    const bool captured=queued&&spi_slave_get_trans_result(SPI3_HOST,&received,pdMS_TO_TICKS(1000))==ESP_OK;
    unsigned bits=captured?transaction.trans_len:0;
    unsigned mismatch=bits==140*8?0:1;
    if(actual) {
        for(unsigned i=0;i<4;++i) mismatch += actual[i]!=0;
        for(unsigned i=0;i<32;++i) {
            mismatch += actual[4+4*i] != 0xff;
            mismatch += actual[5+4*i] != leds[i].r;
            mismatch += actual[6+4*i] != leds[i].g;
            mismatch += actual[7+4*i] != leds[i].b;
        }
        for(unsigned i=132;i<140;++i) mismatch += actual[i]!=0xff;
    } else ++mismatch;
    if(armed) spi_slave_free(SPI3_HOST);
    GPIO.func_out_sel_cfg[4].val=dataRoute;GPIO.func_out_sel_cfg[18].val=clockRoute;GPIO.func_out_sel_cfg[19].val=csRoute;
    gpio_ll_output_enable(&GPIO,GPIO_NUM_4);gpio_ll_output_enable(&GPIO,GPIO_NUM_18);gpio_ll_output_enable(&GPIO,GPIO_NUM_19);
    const bool secondAfter=busOwner()->write(bytes,1000000,LSBFIRST,SPI_MODE3,clientSelect);
    spi_device_interface_config_t dc={};dc.clock_speed_hz=500000;dc.mode=0;dc.spics_io_num=-1;dc.queue_size=1;
    spi_device_handle_t handle=nullptr;
    const auto status=spi_bus_add_device(SPI2_HOST,&dc,&handle);
    if(status==ESP_OK) spi_bus_remove_device(handle);
    const bool nativeUnclaimed=status==ESP_ERR_INVALID_STATE;
    const bool settings=ledSelect()->settings && clientSelect->settings && ledSelect()->pin && clientSelect->pin && ledSelect()->selects==ledSelect()->releases && clientSelect->selects==clientSelect->releases;
    const bool clientSettings = clientSelect->settings;
    const bool clientPin = clientSelect->pin;
    peer.select.reset();
    clientSelect.reset();
    if(actual) heap_caps_free(actual);
    const auto heapEnd = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    fl::json result = fl::json::object();
    result.set("ownerReady",ownerReady);result.set("completed",completed);result.set("providerCalls",calls());
    result.set("nativeUnclaimed",nativeUnclaimed);result.set("secondBefore",secondBefore);result.set("secondAfter",secondAfter);
    result.set("settings",settings);result.set("armed",armed);result.set("captureStatus",static_cast<int>(captureStatus));result.set("captured",captured);result.set("bits",bits);result.set("mismatches",mismatch);

    result.set("ledSelects",ledSelect()->selects);result.set("ledSettings",ledSelect()->settings);result.set("ledPin",ledSelect()->pin);result.set("clientSettings",clientSettings);result.set("clientPin",clientPin);
    result.set("frameMicros",static_cast<unsigned>(frameMicros));
    result.set("contracts",contracts);
    result.set("contention",peerPassed);
    result.set("provider",true);
    result.set("heapStart",static_cast<unsigned>(heapStart));
    result.set("heapEnd",static_cast<unsigned>(heapEnd));
    result.set("success",ownerReady && completed && calls()>0 && nativeUnclaimed && secondBefore && secondAfter && settings && armed && mismatch==0 && peerPassed && contracts);
    return result;
}
}
namespace fl { namespace platforms {
fl::shared_ptr<SpiHw1> fastled_spi_provider(fl::u8 host) FL_NO_EXCEPT {
    ++spi_provider_probe::calls();
    if(host!=2) return {};
    return makeArduinoSpiHw1(spi_provider_probe::busOwner(),spi_provider_probe::ledSelect());
}
}}
