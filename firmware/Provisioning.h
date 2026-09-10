#pragma once

#include "AppTypes.h"

// Provisioning.local.h is intentionally private and excluded from Git. The
// checked-in example documents every supported value.
#if __has_include("Provisioning.local.h")
#include "Provisioning.local.h"
#endif

#ifndef MICRO_APPS_PROVISIONING_REVISION
#define MICRO_APPS_PROVISIONING_REVISION 0
#endif

#ifndef MICRO_APPS_WIFI_SSID
#define MICRO_APPS_WIFI_SSID ""
#endif

#ifndef MICRO_APPS_WIFI_PASSWORD
#define MICRO_APPS_WIFI_PASSWORD ""
#endif

#ifndef MICRO_APPS_OSC_TARGET
#define MICRO_APPS_OSC_TARGET "192.168.1.2"
#endif

#ifndef MICRO_APPS_OSC_SEND_PORT
#define MICRO_APPS_OSC_SEND_PORT 9000
#endif

#ifndef MICRO_APPS_OSC_RECEIVE_PORT
#define MICRO_APPS_OSC_RECEIVE_PORT 9001
#endif

namespace Provisioning {

inline uint32_t revision() {
  return static_cast<uint32_t>(MICRO_APPS_PROVISIONING_REVISION);
}

inline bool available() {
  return revision() > 0 && String(MICRO_APPS_WIFI_SSID).length() > 0;
}

inline uint16_t validPort(uint32_t value, uint16_t fallback) {
  return value >= 1 && value <= 65535 ? static_cast<uint16_t>(value) : fallback;
}

inline void apply(DeviceSettings &settings) {
  settings.wifiSsid = MICRO_APPS_WIFI_SSID;
  settings.wifiPassword = MICRO_APPS_WIFI_PASSWORD;
  settings.oscTarget = MICRO_APPS_OSC_TARGET;
  settings.oscTarget.trim();
  settings.oscSendPort = validPort(MICRO_APPS_OSC_SEND_PORT, 9000);
  settings.oscReceivePort = validPort(MICRO_APPS_OSC_RECEIVE_PORT, 9001);
}

}  // namespace Provisioning
