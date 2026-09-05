#include "ImuService.h"

#include <Wire.h>

#include "pin_config.h"

namespace {
constexpr float kRadiansToDegrees = 57.2957795131f;
constexpr float kComplementaryGyroWeight = 0.97f;
constexpr float kMovementAccelerationDelta = 0.28f;
constexpr float kMovementGyroDelta = 35.0f;
constexpr uint32_t kMovementRetriggerMs = 250;

float pitchFromAccel(const float accel[3]) {
  return atan2f(-accel[0], sqrtf(accel[1] * accel[1] + accel[2] * accel[2])) *
         kRadiansToDegrees;
}

float rollFromAccel(const float accel[3]) {
  return atan2f(accel[1], accel[2]) * kRadiansToDegrees;
}

float wrap180(float value) {
  while (value > 180.0f) value -= 360.0f;
  while (value < -180.0f) value += 360.0f;
  return value;
}
}  // namespace

bool ImuService::begin(const DeviceSettings &settings) {
  rateHz_ = settings.imuRateHz == 50 ? 50 : 25;
  memcpy(gyroBias_, settings.gyroBias, sizeof(gyroBias_));
  pitchOffset_ = settings.pitchOffset;
  rollOffset_ = settings.rollOffset;

  available_ = sensor_.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!available_) return false;
  sensor_.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,
                              SensorQMI8658::ACC_ODR_250Hz,
                              SensorQMI8658::LPF_MODE_0);
  sensor_.configGyroscope(SensorQMI8658::GYR_RANGE_512DPS,
                          SensorQMI8658::GYR_ODR_224_2Hz,
                          SensorQMI8658::LPF_MODE_3);
  sensor_.enableAccelerometer();
  sensor_.enableGyroscope();
  lastSampleUs_ = micros();
  return true;
}

void ImuService::mapAxes(const IMUdata &accel, const IMUdata &gyro, ImuFrame &frame) const {
  // The QMI8658 is mounted 90 degrees clockwise relative to the portrait display.
  // Convert its sensor frame into +X screen-right, +Y screen-top and +Z out of
  // the display. Keep the gyro biases in this same logical screen frame.
  frame.accel[0] = accel.y;
  frame.accel[1] = accel.x;
  frame.accel[2] = accel.z;
  frame.gyro[0] = gyro.y - gyroBias_[0];
  frame.gyro[1] = gyro.x - gyroBias_[1];
  frame.gyro[2] = gyro.z - gyroBias_[2];
}

void ImuService::updateOrientation(ImuFrame &frame, float dt) {
  const float accelPitch = pitchFromAccel(frame.accel);
  const float accelRoll = rollFromAccel(frame.accel);
  if (!orientationInitialized_) {
    pitch_ = accelPitch;
    roll_ = accelRoll;
    orientationInitialized_ = true;
  } else {
    pitch_ = kComplementaryGyroWeight * (pitch_ + frame.gyro[1] * dt) +
             (1.0f - kComplementaryGyroWeight) * accelPitch;
    roll_ = kComplementaryGyroWeight * (roll_ + frame.gyro[0] * dt) +
            (1.0f - kComplementaryGyroWeight) * accelRoll;
  }
  yaw_ = wrap180(yaw_ + frame.gyro[2] * dt);
  frame.orientation[0] = wrap180(pitch_ - pitchOffset_);
  frame.orientation[1] = wrap180(roll_ - rollOffset_);
  frame.orientation[2] = yaw_;
}

bool ImuService::update(ImuFrame &frame) {
  if (!available_) return false;
  const uint32_t now = micros();
  const uint32_t period = 1000000UL / rateHz_;
  if (now - lastSampleUs_ < period || !sensor_.getDataReady()) return false;
  float dt = static_cast<float>(now - lastSampleUs_) / 1000000.0f;
  lastSampleUs_ = now;
  dt = constrain(dt, 0.001f, 0.1f);

  IMUdata accel;
  IMUdata gyro;
  if (!sensor_.getAccelerometer(accel.x, accel.y, accel.z) ||
      !sensor_.getGyroscope(gyro.x, gyro.y, gyro.z)) return false;
  mapAxes(accel, gyro, frame);
  updateOrientation(frame, dt);
  return true;
}

bool ImuService::calibrate(DeviceSettings &settings, ConfigStore &store) {
  if (!available_) return false;
  float previousBias[3];
  memcpy(previousBias, gyroBias_, sizeof(previousBias));
  constexpr uint16_t kSamples = 150;
  float gyroSum[3] = {};
  float accelSum[3] = {};
  uint16_t count = 0;
  const uint32_t deadline = millis() + 5000;
  while (count < kSamples && static_cast<int32_t>(deadline - millis()) > 0) {
    if (!sensor_.getDataReady()) {
      delay(2);
      continue;
    }
    IMUdata accel;
    IMUdata gyro;
    if (sensor_.getAccelerometer(accel.x, accel.y, accel.z) &&
        sensor_.getGyroscope(gyro.x, gyro.y, gyro.z)) {
      ImuFrame sample;
      memset(gyroBias_, 0, sizeof(gyroBias_));
      mapAxes(accel, gyro, sample);
      for (uint8_t axis = 0; axis < 3; ++axis) {
        gyroSum[axis] += sample.gyro[axis];
        accelSum[axis] += sample.accel[axis];
      }
      ++count;
    }
    delay(18);
  }
  if (count < kSamples / 2) {
    memcpy(gyroBias_, previousBias, sizeof(gyroBias_));
    return false;
  }

  for (uint8_t axis = 0; axis < 3; ++axis) {
    gyroBias_[axis] = gyroSum[axis] / count;
    settings.gyroBias[axis] = gyroBias_[axis];
    accelSum[axis] /= count;
  }
  pitchOffset_ = pitchFromAccel(accelSum);
  rollOffset_ = rollFromAccel(accelSum);
  settings.pitchOffset = pitchOffset_;
  settings.rollOffset = rollOffset_;
  yaw_ = 0.0f;
  orientationInitialized_ = false;
  return store.save(settings);
}

bool ImuService::motionDetected(const ImuFrame &frame) const {
  return fabsf(frame.gyro[0]) > 12.0f || fabsf(frame.gyro[1]) > 12.0f ||
         fabsf(frame.gyro[2]) > 12.0f;
}

bool ImuService::movementTriggered(const ImuFrame &frame) {
  if (!movementInitialized_) {
    memcpy(previousMovementAccel_, frame.accel, sizeof(previousMovementAccel_));
    memcpy(previousMovementGyro_, frame.gyro, sizeof(previousMovementGyro_));
    movementInitialized_ = true;
    return false;
  }

  float accelerationDeltaSquared = 0.0f;
  float gyroDeltaSquared = 0.0f;
  for (uint8_t axis = 0; axis < 3; ++axis) {
    const float accelerationDelta = frame.accel[axis] - previousMovementAccel_[axis];
    const float gyroDelta = frame.gyro[axis] - previousMovementGyro_[axis];
    accelerationDeltaSquared += accelerationDelta * accelerationDelta;
    gyroDeltaSquared += gyroDelta * gyroDelta;
    previousMovementAccel_[axis] = frame.accel[axis];
    previousMovementGyro_[axis] = frame.gyro[axis];
  }

  const bool enoughMovement =
      accelerationDeltaSquared >= kMovementAccelerationDelta * kMovementAccelerationDelta ||
      gyroDeltaSquared >= kMovementGyroDelta * kMovementGyroDelta;
  const uint32_t now = millis();
  if (!enoughMovement || now - lastMovementTriggerMs_ < kMovementRetriggerMs) return false;
  lastMovementTriggerMs_ = now;
  return true;
}
