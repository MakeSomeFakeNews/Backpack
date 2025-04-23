#pragma once

#include "device.h"

#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
extern device_t WIFI_device;
#define HAS_WIFI

extern const char *VERSION;

#if defined(TARGET_TX_BACKPACK)
void SendCRSFTelemetryOverUDP(const uint8_t *payload, uint8_t length);
#if defined(PLATFORM_ESP32)
void SendCRSFTelemetryOverBLE(const uint8_t *payload, uint8_t length);
bool InitializeBLEService();
void HandleBLEEvents();
#endif
#endif

#endif