#pragma once

// IWYU pragma: private

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP32) && defined(FL_ESP_USE_TINYUSB_CDC) && FL_ESP_USE_TINYUSB_CDC

#include "sdkconfig.h"
#include "platforms/esp/32/detail/console_config.h"

#if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "Native TinyUSB CDC requires an ESP32-S2/S3/P4 OTG controller"
#endif

#include "platforms/esp/32/drivers/usb_cdc_esp32.h"
#include "fl/stl/scope_exit.h"
#include "fl/stl/cstring.h"
// IWYU pragma: begin_keep
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_private/usb_phy.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <tusb.h>
// IWYU pragma: end_keep

#if !CFG_TUD_CDC
#error "Native TinyUSB CDC requires a TinyUSB SDK built with CDC enabled"
#endif

// Board variant metadata owns the registered device identity. This header
// supplies pin/identity constants without importing the Arduino API.
#if (!defined(USB_VID) || !defined(USB_PID)) && FL_HAS_INCLUDE(<pins_arduino.h>)
#include <pins_arduino.h> // IWYU pragma: keep
#endif

#if defined(USB_VID) && defined(USB_PID)
#define FL_NATIVE_CDC_VID USB_VID
#define FL_NATIVE_CDC_PID USB_PID
#elif defined(CONFIG_TINYUSB_DESC_VID) && defined(CONFIG_TINYUSB_DESC_PID)
#define FL_NATIVE_CDC_VID CONFIG_TINYUSB_DESC_VID
#define FL_NATIVE_CDC_PID CONFIG_TINYUSB_DESC_PID
#else
#error "Native TinyUSB requires USB_VID/USB_PID from the board build configuration"
#endif

namespace fl {
namespace native_cdc_detail {

static constexpr u8 kInterface = 0;
#if defined(CONFIG_IDF_TARGET_ESP32P4)
static constexpr u8 kRhport = 1;
static constexpr usb_phy_target_t kPhyTarget = USB_PHY_TARGET_UTMI;
static constexpr usb_phy_speed_t kPhySpeed = USB_PHY_SPEED_HIGH;
static constexpr tusb_speed_t kUsbSpeed = TUSB_SPEED_HIGH;
#else
static constexpr u8 kRhport = 0;
static constexpr usb_phy_target_t kPhyTarget = USB_PHY_TARGET_INT;
static constexpr usb_phy_speed_t kPhySpeed = USB_PHY_SPEED_FULL;
static constexpr tusb_speed_t kUsbSpeed = TUSB_SPEED_FULL;
#endif
static constexpr u16 kConfigurationLength = TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN;

static constexpr tusb_desc_device_t kDevice = {
    sizeof(tusb_desc_device_t), TUSB_DESC_DEVICE, 0x0200,
    TUSB_CLASS_MISC, MISC_SUBCLASS_COMMON, MISC_PROTOCOL_IAD,
    CFG_TUD_ENDPOINT0_SIZE, FL_NATIVE_CDC_VID, FL_NATIVE_CDC_PID,
    0x0100, 1, 2, 3, 1
};

static constexpr u8 kConfiguration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, kConfigurationLength, 0, 100),
    TUD_CDC_DESCRIPTOR(kInterface, 4, 0x81, 8, 0x02, 0x82, 64)
};

#if defined(CONFIG_IDF_TARGET_ESP32P4)
static constexpr u8 kHighSpeedConfiguration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, kConfigurationLength, 0, 100),
    TUD_CDC_DESCRIPTOR(kInterface, 4, 0x81, 8, 0x02, 0x82, 512)
};

static constexpr tusb_desc_device_qualifier_t kQualifier = {
    sizeof(tusb_desc_device_qualifier_t), TUSB_DESC_DEVICE_QUALIFIER, 0x0200,
    TUSB_CLASS_MISC, MISC_SUBCLASS_COMMON, MISC_PROTOCOL_IAD,
    CFG_TUD_ENDPOINT0_SIZE, 1, 0
};
#endif

static const u8* configuration(bool highSpeed) FL_NO_EXCEPT {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    if (highSpeed) return kHighSpeedConfiguration;
#else
    (void)highSpeed;
#endif
    return kConfiguration;
}

static void usbTask(void*) FL_NO_EXCEPT {
    // The task exists before stack initialization so allocation failure cannot
    // leave an initialized legacy stack without its required service task.
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    for (;;) {
        tud_task();
    }
}

} // namespace native_cdc_detail
} // namespace fl

// TinyUSB requires these C callbacks to have firmware lifetime.
// The packaged SDK enables more class drivers than our CDC-only descriptor
// advertises. Supply their mandatory callbacks here so archive resolution does
// not pull in Arduino's composite USB owners. No storage or DFU interface is
// advertised; unexpected requests must not read storage, write flash or reboot.
#if CFG_TUD_HID
extern "C" const uint8_t* tud_hid_descriptor_report_cb(uint8_t instance) { // ok no noexcept - TinyUSB C callback contract
    (void)instance;
    return nullptr;
}

extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report, hid_report_type_t type, uint8_t* data, uint16_t length) { // ok no noexcept - TinyUSB C callback contract
    (void)instance; (void)report; (void)type; (void)data; (void)length;
    return 0;
}

extern "C" void tud_hid_set_report_cb(uint8_t instance, uint8_t report, hid_report_type_t type, const uint8_t* data, uint16_t length) { // ok no noexcept - TinyUSB C callback contract
    (void)instance; (void)report; (void)type; (void)data; (void)length;
}
#endif

#if CFG_TUD_ECM_RNDIS || CFG_TUD_NCM
extern "C" bool tud_network_recv_cb(const uint8_t* data, uint16_t length) { // ok no noexcept - TinyUSB C callback contract
    (void)data; (void)length;
    return false;
}

extern "C" uint16_t tud_network_xmit_cb(uint8_t* data, void* reference, uint16_t argument) { // ok no noexcept - TinyUSB C callback contract
    (void)data; (void)reference; (void)argument;
    return 0;
}

extern "C" void tud_network_init_cb(void) { // ok no noexcept - TinyUSB C callback contract
    // No network interface is advertised by this CDC-only device.
}
#endif

#if CFG_TUD_DFU_RUNTIME
extern "C" void tud_dfu_runtime_reboot_to_dfu_cb(void) { // ok no noexcept - TinyUSB C callback contract
    // This device never advertises a runtime DFU interface.
}
#endif

#if CFG_TUD_DFU
extern "C" uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state) { // ok no noexcept - TinyUSB C callback contract
    (void)alt; (void)state;
    return 0;
}

extern "C" void tud_dfu_download_cb(uint8_t alt, uint16_t block, const uint8_t* data, uint16_t length) { // ok no noexcept - TinyUSB C callback contract
    (void)alt; (void)block; (void)data; (void)length;
    tud_dfu_finish_flashing(DFU_STATUS_ERR_TARGET);
}

extern "C" void tud_dfu_manifest_cb(uint8_t alt) { // ok no noexcept - TinyUSB C callback contract
    (void)alt;
    tud_dfu_finish_flashing(DFU_STATUS_ERR_TARGET);
}

extern "C" uint16_t tud_dfu_upload_cb(uint8_t alt, uint16_t block, uint8_t* data, uint16_t length) { // ok no noexcept - TinyUSB C callback contract
    (void)alt; (void)block; (void)data; (void)length;
    return 0;
}

extern "C" void tud_dfu_detach_cb(void) { // ok no noexcept - TinyUSB C callback contract
    // No advertised DFU interface owns a detach or reboot operation.
}

extern "C" void tud_dfu_abort_cb(uint8_t alt) { // ok no noexcept - TinyUSB C callback contract
    (void)alt;
}
#endif

#if CFG_TUD_MSC
extern "C" void tud_msc_inquiry_cb(uint8_t lun, uint8_t* vendor, uint8_t* product, uint8_t* revision) { // ok no noexcept - TinyUSB C callback contract
    (void)lun;
    fl::memset(vendor, 0, 8);
    fl::memset(product, 0, 16);
    fl::memset(revision, 0, 4);
}

extern "C" bool tud_msc_test_unit_ready_cb(uint8_t lun) { // ok no noexcept - TinyUSB C callback contract
    (void)lun;
    return false;
}

extern "C" void tud_msc_capacity_cb(uint8_t lun, uint32_t* count, uint16_t* size) { // ok no noexcept - TinyUSB C callback contract
    (void)lun;
    *count = 0;
    *size = 0;
}

extern "C" int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* data, uint32_t length) { // ok no noexcept - TinyUSB C callback contract
    (void)lun; (void)lba; (void)offset; (void)data; (void)length;
    return -1;
}

extern "C" int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* data, uint32_t length) { // ok no noexcept - TinyUSB C callback contract
    (void)lun; (void)lba; (void)offset; (void)data; (void)length;
    return -1;
}

extern "C" int32_t tud_msc_scsi_cb(uint8_t lun, const uint8_t* command, void* data, uint16_t length) { // ok no noexcept - TinyUSB C callback contract
    (void)command; (void)data; (void)length;
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0);
    return -1;
}
#endif

extern "C" const uint8_t* tud_descriptor_device_cb(void) { // ok no noexcept - TinyUSB declares this C callback without noexcept
    return reinterpret_cast<const uint8_t*>(&fl::native_cdc_detail::kDevice); // ok reinterpret cast
}

extern "C" const uint8_t* tud_descriptor_configuration_cb(uint8_t index) { // ok no noexcept - TinyUSB declares this C callback without noexcept
    return index == 0 ? fl::native_cdc_detail::configuration(tud_speed_get() == TUSB_SPEED_HIGH) : nullptr;
}

#if defined(CONFIG_IDF_TARGET_ESP32P4)
extern "C" const uint8_t* tud_descriptor_device_qualifier_cb(void) { // ok no noexcept - TinyUSB declares this C callback without noexcept
    return reinterpret_cast<const uint8_t*>(&fl::native_cdc_detail::kQualifier); // ok reinterpret cast
}

extern "C" const uint8_t* tud_descriptor_other_speed_configuration_cb(uint8_t index) { // ok no noexcept - TinyUSB declares this C callback without noexcept
    if (index != 0) return nullptr;
    static fl::u8 descriptor[fl::native_cdc_detail::kConfigurationLength] = {};
    const fl::u8* source = fl::native_cdc_detail::configuration(tud_speed_get() != TUSB_SPEED_HIGH);
    for (fl::size i = 0; i < sizeof(descriptor); ++i) descriptor[i] = source[i];
    descriptor[1] = TUSB_DESC_OTHER_SPEED_CONFIG;
    return descriptor;
}
#endif

extern "C" const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) { // ok no noexcept - TinyUSB declares this C callback without noexcept
    (void)langid;
    // TinyUSB copies this descriptor synchronously before another request.
    static fl::u16 descriptor[64] = {};
    if (index == 0) {
        descriptor[0] = static_cast<fl::u16>((TUSB_DESC_STRING << 8) | 4);
        descriptor[1] = 0x0409;
        return descriptor;
    }
    const char* text = nullptr;
    char serial[13] = {};
    switch (index) {
    case 1:
#ifdef USB_MANUFACTURER
        text = USB_MANUFACTURER;
#else
        text = "FastLED";
#endif
        break;
    case 2:
#ifdef USB_PRODUCT
        text = USB_PRODUCT;
#else
        text = "FastLED console";
#endif
        break;
    case 3: {
        fl::u8 mac[6] = {};
        if (esp_efuse_mac_get_default(mac) != ESP_OK) return nullptr;
        static constexpr char kHex[] = "0123456789ABCDEF";
        for (fl::size i = 0; i < 6; ++i) {
            serial[i * 2] = kHex[mac[i] >> 4];
            serial[i * 2 + 1] = kHex[mac[i] & 15];
        }
        text = serial;
        break;
    }
    case 4:
        text = "Console";
        break;
    default:
        return nullptr;
    }
    fl::size length = 0;
    while (text[length] && length < 63) {
        descriptor[length + 1] = static_cast<fl::u8>(text[length]);
        ++length;
    }
    descriptor[0] = static_cast<fl::u16>((TUSB_DESC_STRING << 8) | (2 * length + 2));
    return descriptor;
}

namespace fl {

UsbCdcEsp32::UsbCdcEsp32() FL_NO_EXCEPT {
    // Do not adopt another owner's device stack or consume its CDC interface.
    if (tud_inited()) return;
    mTxMutex = xSemaphoreCreateMutex();
    if (!mTxMutex) return;
    usb_phy_config_t config = {};
    config.controller = USB_PHY_CTRL_OTG;
    config.target = native_cdc_detail::kPhyTarget;
    config.otg_mode = USB_OTG_MODE_DEVICE;
    config.otg_speed = native_cdc_detail::kPhySpeed;
    usb_phy_handle_t phy = nullptr;
    if (usb_new_phy(&config, &phy) != ESP_OK) {
        vSemaphoreDelete(static_cast<SemaphoreHandle_t>(mTxMutex));
        mTxMutex = nullptr;
        return;
    }
    TaskHandle_t task = nullptr;
    if (xTaskCreate(native_cdc_detail::usbTask, "fl-usb", 4096, nullptr,
                    configMAX_PRIORITIES - 1, &task) != pdPASS) {
        usb_del_phy(phy);
        vSemaphoreDelete(static_cast<SemaphoreHandle_t>(mTxMutex));
        mTxMutex = nullptr;
        return;
    }
#if TUSB_VERSION_MAJOR > 0 || TUSB_VERSION_MINOR >= 20
    tusb_rhport_init_t init = {};
    init.role = TUSB_ROLE_DEVICE;
    init.speed = native_cdc_detail::kUsbSpeed;
    const bool initialized = tusb_init(native_cdc_detail::kRhport, &init);
#else
    const bool initialized = tud_init(native_cdc_detail::kRhport);
#endif
    if (!initialized) {
        vTaskDelete(task);
        usb_del_phy(phy);
        vSemaphoreDelete(static_cast<SemaphoreHandle_t>(mTxMutex));
        mTxMutex = nullptr;
        return;
    }
    mInitialized = true;
    xTaskNotifyGive(task);
}

bool UsbCdcEsp32::isConnected() const FL_NO_EXCEPT {
    return mInitialized && tud_cdc_n_connected(native_cdc_detail::kInterface);
}

int UsbCdcEsp32::available() FL_NO_EXCEPT {
    return mInitialized ? static_cast<i32>(tud_cdc_n_available(native_cdc_detail::kInterface)) : 0;
}

int UsbCdcEsp32::peek() FL_NO_EXCEPT {
    u8 value = 0;
    return mInitialized && tud_cdc_n_peek(native_cdc_detail::kInterface, &value) ? value : -1;
}

int UsbCdcEsp32::read() FL_NO_EXCEPT {
    u8 value = 0;
    return mInitialized && tud_cdc_n_read(native_cdc_detail::kInterface, &value, 1) == 1 ? value : -1;
}

size_t UsbCdcEsp32::write(const u8* buffer, size_t size) FL_NO_EXCEPT {
    if (!buffer || !size || !isConnected()) return 0;
    const TickType_t start = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(1000);
    auto mutex = static_cast<SemaphoreHandle_t>(mTxMutex);
    if (xSemaphoreTake(mutex, timeout) != pdTRUE) return 0;
    auto unlock = fl::make_scope_exit([mutex]() { xSemaphoreGive(mutex); });
    size_t written = 0;
    while (written < size && isConnected()) {
        const u32 available = tud_cdc_n_write_available(native_cdc_detail::kInterface);
        const size_t remaining = size - written;
        const u32 length = remaining < available ? static_cast<u32>(remaining) : available;
        if (length) {
            written += tud_cdc_n_write(native_cdc_detail::kInterface, buffer + written, length);
        }
        tud_cdc_n_write_flush(native_cdc_detail::kInterface);
        if (written == size || xTaskGetTickCount() - start >= timeout) break;
        vTaskDelay(1);
    }
    return written;
}

void UsbCdcEsp32::write(const char* str) FL_NO_EXCEPT {
    if (!str) return;
    size_t size = 0;
    while (str[size]) ++size;
    write(reinterpret_cast<const u8*>(str), size); // ok reinterpret cast
}

void UsbCdcEsp32::writeln(const char* str) FL_NO_EXCEPT {
    write(str);
    write("\n");
}

bool UsbCdcEsp32::flush(u32 timeoutMs) FL_NO_EXCEPT {
    if (!isConnected()) return false;
    const TickType_t start = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(timeoutMs);
    auto mutex = static_cast<SemaphoreHandle_t>(mTxMutex);
    if (xSemaphoreTake(mutex, timeout) != pdTRUE) return false;
    auto unlock = fl::make_scope_exit([mutex]() { xSemaphoreGive(mutex); });
    while (tud_cdc_n_write_available(native_cdc_detail::kInterface) < CFG_TUD_CDC_TX_BUFSIZE) {
        tud_cdc_n_write_flush(native_cdc_detail::kInterface);
        if (!isConnected() || xTaskGetTickCount() - start >= timeout) return false;
        vTaskDelay(1);
    }
    return isConnected();
}

} // namespace fl

#undef FL_NATIVE_CDC_VID
#undef FL_NATIVE_CDC_PID

#endif
