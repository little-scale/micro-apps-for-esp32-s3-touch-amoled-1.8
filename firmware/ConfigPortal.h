#pragma once

#include <DNSServer.h>
#include <WebServer.h>

#include "AppTypes.h"
#include "ConfigStore.h"

class ConfigPortal {
 public:
  void begin(DeviceSettings &settings, ConfigStore &store);
  bool start();
  void stop();
  void loop();
  bool active() const { return active_; }
  bool consumeRestartRequest();
  bool consumeCalibrationRequest();
  bool consumeFactoryResetRequest();

 private:
  void configureRoutes();
  void handleRoot();
  void handleSave();
  void handleCalibrate();
  void handleFactoryReset();
  void redirectToRoot();
  String page() const;
  static String htmlEscape(const String &input);
  static String cleanDeviceName(const String &input);

  DNSServer dns_;
  WebServer server_{80};
  DeviceSettings *settings_ = nullptr;
  ConfigStore *store_ = nullptr;
  bool active_ = false;
  bool routesConfigured_ = false;
  bool restartRequested_ = false;
  bool calibrationRequested_ = false;
  bool factoryResetRequested_ = false;
  uint32_t lastActivityMs_ = 0;
};
