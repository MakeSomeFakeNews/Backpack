#pragma once

#include "device.h"

#if defined(PLATFORM_ESP32)
extern device_t BLET_device;
int BluetoothTelemetryUpdateValues(const uint8_t *data,const size_t length);
void BluetoothTelemetryShutdown();
#endif 