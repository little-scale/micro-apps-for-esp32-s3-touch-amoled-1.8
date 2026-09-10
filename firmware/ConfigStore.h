#pragma once

#include <Preferences.h>

#include "AppTypes.h"

class ConfigStore {
 public:
  bool begin();
  void load(DeviceSettings &settings);
  bool save(const DeviceSettings &settings);
  uint32_t provisioningRevision();
  bool saveProvisioningRevision(uint32_t revision);
  void clear();

 private:
  Preferences preferences_;
};
