#include "devBLE.h"

#if defined(PLATFORM_ESP32)

#include "NimBLEDevice.h"
#include "common.h"
#include "logging.h"
#include "options.h"
#include "config.h"
#include "msp.h"
#include "msptypes.h"

NimBLEServer *pServer;
NimBLECharacteristic *rcCRSF;

// how often link stats packet is sent over BLE
#define BLE_LINKSTATS_PACKET_PERIOD_MS 200
#define BLE_CHANNELS_PACKET_PERIOD_MS 300

unsigned short const TELEMETRY_SVC_UUID = 0x1819;
unsigned short const TELEMETRY_CRSF_UUID = 0x2BBD;
const char *WRITE_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"; // 128-bit UUID

unsigned short const DEVICE_INFO_SVC_UUID = 0x180A;
unsigned short const MODEL_NUMBER_SVC_UUID = 0x2A24;
unsigned short const SERIAL_NUMBER_SVC_UUID = 0x2A25;
unsigned short const SOFTWARE_NUMBER_SVC_UUID = 0x2A28;
unsigned short const HARDWARE_NUMBER_SVC_UUID = 0x2A27;
unsigned short const MANUFACTURER_NAME_SVC_UUID = 0x2A29;

extern TxBackpackConfig config;

static void assignRandomAddress();

class BLEServerCB : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *param) override {
        if (pServer != nullptr) {
            assignRandomAddress();
            NimBLEDevice::startAdvertising();
        }
    }
};
static BLEServerCB bleServerCB;

static String getUIDString() {
    char muids[7] = {0};
    sprintf(muids, "%02X%02X%02X", firmwareOptions.uid[3], firmwareOptions.uid[4], firmwareOptions.uid[5]);
    return String(muids);
}

void BluetoothTelemetryShutdown() {
    if (pServer != nullptr) {
        DBGLN("Stopping BLE Telemetry!");
        pServer->setCallbacks(nullptr);
        pServer = nullptr;
        rcCRSF = nullptr;
        NimBLEDevice::stopAdvertising();
        NimBLEDevice::deinit(true);
    }
}

static void assignRandomAddress() {
    uint8_t ble_address[6];
    memcpy(ble_address, NimBLEDevice::getAddress().getNative(), 6);
    ble_address[5] |= 0xC0; // static random address
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
    ble_hs_id_set_rnd(ble_address);
}

static void onAdvertisingStopped(NimBLEAdvertising *pAdv) {
    if (pServer != nullptr) {
        assignRandomAddress();
        pAdv->start(0, &onAdvertisingStopped);
    }
}

// 写入回调处理
class WriteCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        // LOGI("BLE", "Received BLE WRITE: %s", value.c_str());
    }
};
static WriteCallback writeCallback;

static void BluetoothTelemetryUpdateDevice() {
    if (pServer != nullptr)
        return;

    // 使用 MAC 地址生成设备名 RunCam-XXXX
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    char devName[20];
    snprintf(devName, sizeof(devName), "RunCam-%02X%02X", mac[4], mac[5]);
    NimBLEDevice::init(devName);

    assignRandomAddress();
    NimBLEDevice::setMTU(60 + 3);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&bleServerCB, true);

    // === Telemetry Service ===
    NimBLEService *rcService = pServer->createService(TELEMETRY_SVC_UUID);

    // 通道值通知
    rcCRSF = rcService->createCharacteristic(
        TELEMETRY_CRSF_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
        60
    );

    // 新增 WRITE 特性
    NimBLECharacteristic *writeChar = rcService->createCharacteristic(
        WRITE_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE,
        60
    );
    writeChar->setCallbacks(&writeCallback);

    rcService->start();

    // === 设备信息服务 ===
    NimBLEService *dInfo = pServer->createService(DEVICE_INFO_SVC_UUID);
    dInfo->createCharacteristic(MANUFACTURER_NAME_SVC_UUID, NIMBLE_PROPERTY::READ)->setValue("ExpressLRS");
    dInfo->createCharacteristic(MODEL_NUMBER_SVC_UUID, NIMBLE_PROPERTY::READ)->setValue("TX Backpack");
    dInfo->createCharacteristic(SERIAL_NUMBER_SVC_UUID, NIMBLE_PROPERTY::READ)
        ->setValue(String(__TIMESTAMP__) + " - " + getUIDString());
    dInfo->createCharacteristic(SOFTWARE_NUMBER_SVC_UUID, NIMBLE_PROPERTY::READ)->setValue("1");
    dInfo->createCharacteristic(HARDWARE_NUMBER_SVC_UUID, NIMBLE_PROPERTY::READ)->setValue("ESP32");
    dInfo->start();

    // 设置功率
    for (int i = 0; i <= 8; ++i)
        NimBLEDevice::setPower(ESP_PWR_LVL_P21, (esp_ble_power_type_t)(ESP_BLE_PWR_TYPE_CONN_HDL0 + i));
    NimBLEDevice::setPower(ESP_PWR_LVL_P21, ESP_BLE_PWR_TYPE_ADV);
    NimBLEDevice::setPower(ESP_PWR_LVL_P21, ESP_BLE_PWR_TYPE_DEFAULT);

    // 开始广播
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(rcService->getUUID());
    pAdvertising->addServiceUUID(dInfo->getUUID());
    pAdvertising->start(0, &onAdvertisingStopped);

    DBGLN("Starting BLE Telemetry!");
}

int BluetoothTelemetryUpdateValues(const uint8_t *data, const size_t length) {
    if (pServer == nullptr || rcCRSF == nullptr)
        return DURATION_NEVER;

    if (data != nullptr) {
        rcCRSF->setValue(data, length);
        rcCRSF->notify();
    }

    return DURATION_NEVER;
}


static int event() {
    return DURATION_IMMEDIATELY;
}

static int timeout() {
    BluetoothTelemetryUpdateDevice();
    return BluetoothTelemetryUpdateValues(nullptr,-1);
}

device_t BLET_device = {
    .initialize = nullptr,
    .start = nullptr,
    .event = event,
    .timeout = timeout
};

#endif
