#include "BleTransport.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gap_ble_api.h>
#endif
#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/util/util.h>
#endif

namespace {
constexpr char kServiceUuid[] = "89689de0-1604-42c1-85ba-e286075c4488";
constexpr char kControlUuid[] = "c2bbf0cb-9029-4925-abbc-3dd86492fc06";
constexpr char kImuUuid[] = "2e5abe1b-4f44-429e-8b47-1526afbe343d";
constexpr char kInfoUuid[] = "fe9c3dcb-451c-4512-b845-a611ffa3280f";
constexpr uint8_t kProtocolVersion = 1;
constexpr size_t kHeaderSize = 6;

BleTransport *activeTransport = nullptr;

bool decodeFloat(const uint8_t *data, float &value) {
  memcpy(&value, data, sizeof(value));
  return isfinite(value);
}
}  // namespace

class BleServerCallbacks : public BLEServerCallbacks {
 public:
  void onConnect(BLEServer *) override {
    if (activeTransport) activeTransport->setConnected(true);
  }

  void onDisconnect(BLEServer *) override {
    if (activeTransport) activeTransport->setConnected(false);
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
  void onMtuChanged(BLEServer *, esp_ble_gatts_cb_param_t *param) override {
    if (activeTransport && param) activeTransport->setNegotiatedMtu(param->mtu.mtu);
  }
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  void onMtuChanged(BLEServer *, ble_gap_conn_desc *, uint16_t mtu) override {
    if (activeTransport) activeTransport->setNegotiatedMtu(mtu);
  }
#endif
};

class BleControlCallbacks : public BLECharacteristicCallbacks {
 public:
  void onWrite(BLECharacteristic *characteristic) override {
    if (!activeTransport || !characteristic) return;
    const String value = characteristic->getValue();
    activeTransport->receive(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
  }
};

void BleTransport::begin(const DeviceSettings &settings, RemoteMessageHandler handler) {
  handler_ = handler;
  activeTransport = this;

  BLEDevice::init(settings.deviceName.c_str());
  BLEDevice::setMTU(64);
  server_ = BLEDevice::createServer();
  server_->setCallbacks(new BleServerCallbacks());

  BLEService *service = server_->createService(kServiceUuid);
  controlCharacteristic_ = service->createCharacteristic(
      kControlUuid, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE_NR);
  controlCharacteristic_->addDescriptor(new BLE2902());
  controlCharacteristic_->setCallbacks(new BleControlCallbacks());

  imuCharacteristic_ = service->createCharacteristic(kImuUuid, BLECharacteristic::PROPERTY_NOTIFY);
  imuCharacteristic_->addDescriptor(new BLE2902());

  BLECharacteristic *info = service->createCharacteristic(kInfoUuid, BLECharacteristic::PROPERTY_READ);
  String metadata = "{\"protocol\":1,\"device\":\"" + settings.deviceName +
                    "\",\"float\":\"ieee754-le\"}";
  info->setValue(metadata);
  service->start();

  BLEAdvertising *advertising = server_->getAdvertising();
  advertising->addServiceUUID(kServiceUuid);
  advertising->setScanResponse(true);
  initialized_ = true;
  setEnabled(settings.bleEnabled);
}

void BleTransport::setConnected(bool connected) {
  connected_ = connected;
  negotiatedMtu_ = 23;
  if (!connected && enabled_ && server_) server_->startAdvertising();
}

void BleTransport::setNegotiatedMtu(uint16_t mtu) {
  negotiatedMtu_ = mtu;
}

void BleTransport::setEnabled(bool enabled) {
  if (!initialized_ || !server_) return;
  enabled_ = enabled;
  if (enabled) {
    server_->startAdvertising();
  } else {
    server_->getAdvertising()->stop();
    const auto peers = server_->getPeerDevices(false);
    for (const auto &peer : peers) server_->disconnect(peer.first);
    connected_ = false;
  }
}

void BleTransport::clearBonds() {
#if defined(CONFIG_BLUEDROID_ENABLED)
  int count = esp_ble_get_bond_device_num();
  if (count > 0) {
    esp_ble_bond_dev_t *devices = static_cast<esp_ble_bond_dev_t *>(
        malloc(sizeof(esp_ble_bond_dev_t) * count));
    if (devices && esp_ble_get_bond_device_list(&count, devices) == ESP_OK) {
      for (int i = 0; i < count; ++i) esp_ble_remove_bond_device(devices[i].bd_addr);
    }
    free(devices);
  }
#elif defined(CONFIG_NIMBLE_ENABLED)
  ble_store_clear();
#endif
  if (enabled_ && server_) server_->startAdvertising();
}

size_t BleTransport::makePacket(uint8_t *packet, size_t capacity, MessageType type, uint8_t index,
                                const void *payload, size_t payloadSize, uint16_t sequence) {
  if (capacity < kHeaderSize + payloadSize) return 0;
  packet[0] = kProtocolVersion;
  packet[1] = static_cast<uint8_t>(type);
  packet[2] = index;
  packet[3] = 0;
  packet[4] = static_cast<uint8_t>(sequence);
  packet[5] = static_cast<uint8_t>(sequence >> 8);
  if (payloadSize) memcpy(packet + kHeaderSize, payload, payloadSize);
  return kHeaderSize + payloadSize;
}

void BleTransport::notifyControl(MessageType type, uint8_t index, const void *payload,
                                 size_t payloadSize) {
  if (!enabled_ || !connected_ || !controlCharacteristic_) return;
  uint8_t packet[48];
  const size_t length = makePacket(packet, sizeof(packet), type, index, payload, payloadSize,
                                   sequence_++);
  if (!length) return;
  controlCharacteristic_->setValue(packet, length);
  controlCharacteristic_->notify();
}

void BleTransport::sendXy(float x, float y) {
  const float values[] = {x, y};
  notifyControl(MessageType::Xy, 0, values, sizeof(values));
}

void BleTransport::sendFader(uint8_t index, float value) {
  notifyControl(MessageType::Fader, index, &value, sizeof(value));
}

void BleTransport::sendToggle(uint8_t index, bool value) {
  const uint8_t state = value ? 1 : 0;
  notifyControl(MessageType::Toggle, index, &state, sizeof(state));
}

void BleTransport::sendButton(uint8_t index, bool value) {
  const uint8_t state = value ? 1 : 0;
  notifyControl(MessageType::Button, index, &state, sizeof(state));
}

void BleTransport::sendNote(uint8_t midiNote, bool value, uint8_t velocity) {
  const uint8_t payload[] = {static_cast<uint8_t>(value ? 1 : 0),
                             static_cast<uint8_t>(value ? velocity : 0)};
  notifyControl(MessageType::Note, midiNote, payload, sizeof(payload));
}

void BleTransport::sendMovementTrigger() {
  const uint8_t triggered = 1;
  notifyControl(MessageType::MovementTrigger, 0, &triggered, sizeof(triggered));
}

void BleTransport::sendMicEnergy(float energy) {
  notifyControl(MessageType::MicEnergy, 0, &energy, sizeof(energy));
}

void BleTransport::notifyImuPart(MessageType type, const float values[3], uint16_t sequence) {
  if (!imuCharacteristic_) return;
  uint8_t packet[18];
  const size_t length = makePacket(packet, sizeof(packet), type, 0, values,
                                   sizeof(float) * 3, sequence);
  imuCharacteristic_->setValue(packet, length);
  imuCharacteristic_->notify();
}

void BleTransport::sendImu(const ImuFrame &frame) {
  if (!enabled_ || !connected_ || !imuCharacteristic_) return;
  const uint16_t frameSequence = sequence_++;
  if (negotiatedMtu_ >= 64) {
    float values[9];
    memcpy(values, frame.accel, sizeof(frame.accel));
    memcpy(values + 3, frame.gyro, sizeof(frame.gyro));
    memcpy(values + 6, frame.orientation, sizeof(frame.orientation));
    uint8_t packet[42];
    const size_t length = makePacket(packet, sizeof(packet), MessageType::Imu, 0, values,
                                     sizeof(values), frameSequence);
    imuCharacteristic_->setValue(packet, length);
    imuCharacteristic_->notify();
  } else {
    notifyImuPart(MessageType::ImuAccel, frame.accel, frameSequence);
    notifyImuPart(MessageType::ImuGyro, frame.gyro, frameSequence);
    notifyImuPart(MessageType::ImuOrientation, frame.orientation, frameSequence);
  }
}

void BleTransport::receive(const uint8_t *data, size_t length) {
  if (!enabled_ || !data || length < kHeaderSize || data[0] != kProtocolVersion) return;
  RemoteMessage message;
  message.type = static_cast<MessageType>(data[1]);
  message.index = data[2];
  const uint8_t *payload = data + kHeaderSize;
  const size_t payloadLength = length - kHeaderSize;

  switch (message.type) {
    case MessageType::Xy:
      if (message.index != 0 || payloadLength != 8 ||
          !decodeFloat(payload, message.values[0]) || !decodeFloat(payload + 4, message.values[1]) ||
          message.values[0] < 0.0f || message.values[0] > 1.0f ||
          message.values[1] < 0.0f || message.values[1] > 1.0f) return;
      break;
    case MessageType::Fader:
      if (message.index >= 4 || payloadLength != 4 || !decodeFloat(payload, message.values[0]) ||
          message.values[0] < 0.0f || message.values[0] > 1.0f) return;
      break;
    case MessageType::Button:
      if (message.index >= 4 || payloadLength != 1 || payload[0] > 1) return;
      message.intValue = payload[0];
      break;
    case MessageType::Note:
      if (message.index > 127 || (payloadLength != 1 && payloadLength != 2) ||
          payload[0] > 1 || (payloadLength == 2 && payload[1] > 127)) return;
      message.intValue = payload[0];
      break;
    case MessageType::Background:
      if (payloadLength != 3) return;
      memcpy(message.rgb, payload, 3);
      break;
    default:
      return;
  }

  portENTER_CRITICAL(&queueMux_);
  const uint8_t next = (queueWrite_ + 1) % kQueueSize;
  if (next != queueRead_) {
    queue_[queueWrite_] = message;
    queueWrite_ = next;
  }
  portEXIT_CRITICAL(&queueMux_);
}

void BleTransport::loop() {
  while (true) {
    RemoteMessage message;
    bool available = false;
    portENTER_CRITICAL(&queueMux_);
    if (queueRead_ != queueWrite_) {
      message = queue_[queueRead_];
      queueRead_ = (queueRead_ + 1) % kQueueSize;
      available = true;
    }
    portEXIT_CRITICAL(&queueMux_);
    if (!available) break;
    if (handler_) handler_(message, InputSource::Ble);
  }
}
