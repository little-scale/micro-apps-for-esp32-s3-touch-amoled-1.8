#pragma once

#include <SensorQMI8658.hpp>

#include "AppTypes.h"
#include "ConfigStore.h"

class ImuService {
 public:
  bool begin(const DeviceSettings &settings);
  bool update(ImuFrame &frame);
  bool calibrate(DeviceSettings &settings, ConfigStore &store);
  bool available() const { return available_; }
  bool motionDetected(const ImuFrame &frame) const;
  bool movementTriggered(const ImuFrame &frame);

 private:
  void mapAxes(const IMUdata &accel, const IMUdata &gyro, ImuFrame &frame) const;
  void updateOrientation(ImuFrame &frame, float dt);

  SensorQMI8658 sensor_;
  bool available_ = false;
  bool orientationInitialized_ = false;
  uint8_t rateHz_ = 25;
  uint32_t lastSampleUs_ = 0;
  float gyroBias_[3] = {};
  float pitchOffset_ = 0.0f;
  float rollOffset_ = 0.0f;
  float pitch_ = 0.0f;
  float roll_ = 0.0f;
  float yaw_ = 0.0f;
  bool movementInitialized_ = false;
  float previousMovementAccel_[3] = {};
  float previousMovementGyro_[3] = {};
  uint32_t lastMovementTriggerMs_ = 0;
};
