#pragma once

#include <Arduino.h>

#include "AppTypes.h"

class BLECharacteristic;
class BLEServer;
class BleServerCallbacks;
class BleControlCallbacks;

class BleTransport {
 public:
  void begin(const DeviceSettings &settings, RemoteMessageHandler handler);
  void loop();
  void setEnabled(bool enabled);
  bool enabled() const { return enabled_; }
  bool connected() const { return connected_; }
  void clearBonds();

  void sendXy(float x, float y);
  void sendFader(uint8_t index, float value);
  void sendToggle(uint8_t index, bool value);
  void sendButton(uint8_t index, bool value);
  void sendNote(uint8_t midiNote, bool value, uint8_t velocity);
  void sendMovementTrigger();
  void sendMicEnergy(float energy);
  void sendImu(const ImuFrame &frame);

 private:
  friend class BleServerCallbacks;
  friend class BleControlCallbacks;

  void setConnected(bool connected);
  void setNegotiatedMtu(uint16_t mtu);
  void receive(const uint8_t *data, size_t length);
  void notifyControl(MessageType type, uint8_t index, const void *payload, size_t payloadSize);
  void notifyImuPart(MessageType type, const float values[3], uint16_t sequence);
  size_t makePacket(uint8_t *packet, size_t capacity, MessageType type, uint8_t index,
                    const void *payload, size_t payloadSize, uint16_t sequence);

  BLEServer *server_ = nullptr;
  BLECharacteristic *controlCharacteristic_ = nullptr;
  BLECharacteristic *imuCharacteristic_ = nullptr;
  RemoteMessageHandler handler_ = nullptr;
  bool initialized_ = false;
  volatile bool enabled_ = false;
  volatile bool connected_ = false;
  volatile uint16_t negotiatedMtu_ = 23;
  uint16_t sequence_ = 0;

  static constexpr uint8_t kQueueSize = 8;
  RemoteMessage queue_[kQueueSize];
  volatile uint8_t queueRead_ = 0;
  volatile uint8_t queueWrite_ = 0;
  portMUX_TYPE queueMux_ = portMUX_INITIALIZER_UNLOCKED;
};
