#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "AppTypes.h"
#include "AudioService.h"
#include "BleTransport.h"
#include "ConfigStore.h"
#include "ImuService.h"
#include "OscTransport.h"
#include "Provisioning.h"
#include "UserInterface.h"

namespace {
DeviceSettings settings;
ControlState controls;
ImuFrame imuFrame;

ConfigStore configStore;
OscTransport osc;
BleTransport ble;
ImuService imu;
AudioService audio;
UserInterface ui;

bool mdnsStarted = false;
uint32_t restartAtMs = 0;

void applyRemoteMessage(const RemoteMessage &message, InputSource) {
  if (ui.isControlActive(message.type, message.index)) return;
  switch (message.type) {
    case MessageType::Xy:
      controls.xyX = message.values[0];
      controls.xyY = message.values[1];
      break;
    case MessageType::Fader:
      if (message.index < 4) controls.faders[message.index] = message.values[0];
      break;
    case MessageType::Button:
      if (message.index < 4) controls.buttons[message.index] = message.intValue != 0;
      break;
    case MessageType::Note:
      if (message.index <= 127) controls.keyboardNotes[message.index] = message.intValue != 0;
      break;
    case MessageType::Spectrum:
      if (!ui.spectrumPageActive()) return;
      memcpy(controls.spectrum, message.values, sizeof(controls.spectrum));
      break;
    case MessageType::Background:
      memcpy(controls.background, message.rgb, sizeof(controls.background));
      break;
    default:
      return;
  }
  // Remote changes redraw and wake the device, but are never forwarded to either transport.
  ui.wake();
}

void sendLocalControl(const UiEvent &event) {
  switch (event.control) {
    case MessageType::Xy:
      osc.sendXy(event.values[0], event.values[1]);
      ble.sendXy(event.values[0], event.values[1]);
      break;
    case MessageType::Fader:
      osc.sendFader(event.index, event.values[0]);
      ble.sendFader(event.index, event.values[0]);
      break;
    case MessageType::Button:
      osc.sendButton(event.index, event.state);
      ble.sendButton(event.index, event.state);
      break;
    case MessageType::Note:
      osc.sendNote(event.index, event.state, event.numbers[0]);
      ble.sendNote(event.index, event.state, event.numbers[0]);
      break;
    case MessageType::Spectrum:
      if (ui.spectrumPageActive()) osc.sendSpectrum(event.values);
      break;
    default:
      break;
  }
}

void handleUiEvents() {
  UiEvent event;
  while (ui.popEvent(event)) {
    switch (event.type) {
      case UiEventType::Control:
        sendLocalControl(event);
        break;
      case UiEventType::WallCollision:
        osc.sendWallCollision(event.index, event.numbers[0], event.values[0]);
        break;
      case UiEventType::BallCollision:
        osc.sendBallCollision(event.index, event.numbers[0], event.values[0]);
        break;
      case UiEventType::ParticleWall:
        osc.sendParticleWall(event.index, event.values[0]);
        break;
      case UiEventType::MazeCollision:
        osc.sendMazeCollision(event.numbers[0], event.numbers[1],
                              event.index, event.values[0]);
        break;
      case UiEventType::MazeGoal:
        osc.sendMazeGoal();
        break;
      case UiEventType::PendulumState:
        osc.sendPendulum(event.index, event.values);
        break;
      case UiEventType::PendulumPing:
        osc.sendPendulumPing(event.index, event.numbers[0]);
        break;
      case UiEventType::PendulumActive:
        osc.sendPendulumActive(event.index, event.state);
        break;
      case UiEventType::WifiCredentials:
        settings.wifiSsid = event.text[0];
        settings.wifiPassword = event.text[1];
        configStore.save(settings);
        WiFi.disconnect(false, false);
        WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
        ui.wake();
        break;
      case UiEventType::OscSettings:
        settings.oscTarget = event.text[0];
        settings.oscSendPort = event.numbers[0];
        settings.oscReceivePort = event.numbers[1];
        configStore.save(settings);
        osc.reconfigure(settings);
        if (mdnsStarted) {
          MDNS.end();
          mdnsStarted = false;
        }
        ui.wake();
        break;
      case UiEventType::DeviceName:
        settings.deviceName = event.text[0];
        configStore.save(settings);
        restartAtMs = millis() + 700;
        break;
      case UiEventType::PhysicsSettings:
        settings.ballGravity = event.values[0];
        settings.ballBounciness = event.values[1];
        configStore.save(settings);
        ui.wake();
        break;
      case UiEventType::ToggleBle:
        settings.bleEnabled = !ble.enabled();
        ble.setEnabled(settings.bleEnabled);
        configStore.save(settings);
        ui.wake();
        break;
      case UiEventType::ToggleImuOutput:
        settings.imuOutputEnabled = !settings.imuOutputEnabled;
        configStore.save(settings);
        ui.wake();
        break;
      case UiEventType::ToggleMicOutput:
        settings.micOutputEnabled = !settings.micOutputEnabled;
        configStore.save(settings);
        ui.wake();
        break;
      case UiEventType::ClearBleBonds:
        ble.clearBonds();
        ui.wake();
        break;
    }
  }
}

void beginWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(settings.deviceName.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  if (!settings.wifiSsid.isEmpty()) {
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
  }
}

void serviceMdns() {
  if (WiFi.status() == WL_CONNECTED && !mdnsStarted) {
    mdnsStarted = MDNS.begin(settings.deviceName.c_str());
    if (mdnsStarted) MDNS.addService("osc", "udp", settings.oscReceivePort);
  } else if (WiFi.status() != WL_CONNECTED && mdnsStarted) {
    MDNS.end();
    mdnsStarted = false;
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);

  const bool configReady = configStore.begin();
  if (configReady) configStore.load(settings);
  const uint32_t storedProvisioningRevision =
      configReady ? configStore.provisioningRevision() : 0;
  const bool applyProvisioning =
      Provisioning::available() &&
      Provisioning::revision() > storedProvisioningRevision;
  if (applyProvisioning) Provisioning::apply(settings);

  const String uniqueName = hardwareDeviceName();
  char correctedLegacyName[20];
  char buggyDeviceName[16];
  char buggyLegacyName[20];
  const uint16_t buggySuffix = static_cast<uint16_t>(ESP.getEfuseMac());
  snprintf(correctedLegacyName, sizeof(correctedLegacyName), "classroom-%04x",
           hardwareDeviceSuffix());
  snprintf(buggyDeviceName, sizeof(buggyDeviceName), "device-%04x", buggySuffix);
  snprintf(buggyLegacyName, sizeof(buggyLegacyName), "classroom-%04x", buggySuffix);
  bool generatedUniqueName = false;
  if (settings.deviceName == "device-0000" || settings.deviceName == "classroom-01" ||
      settings.deviceName == correctedLegacyName || settings.deviceName == buggyDeviceName ||
      settings.deviceName == buggyLegacyName) {
    settings.deviceName = uniqueName;
    generatedUniqueName = true;
  }
  if (configReady && (applyProvisioning || generatedUniqueName)) {
    const bool settingsSaved = configStore.save(settings);
    if (settingsSaved && applyProvisioning) {
      configStore.saveProvisioningRevision(Provisioning::revision());
      Serial.printf("local provisioning applied revision=%lu\n",
                    static_cast<unsigned long>(Provisioning::revision()));
    }
  }
  const bool uiReady = ui.begin(settings);
  beginWifi();
  osc.begin(settings, applyRemoteMessage);
  ble.begin(settings, applyRemoteMessage);
  const bool imuReady = imu.begin(settings);
  const bool audioReady = audio.begin();

  // A new device opens the fully local network picker; no setup hotspot is created.
  if (settings.wifiSsid.isEmpty()) ui.openWifiSetup(true);
  Serial.printf("boot ui_touch=%s imu=%s audio=%s device=%s\n",
                uiReady ? "ok" : "failed", imuReady ? "ok" : "failed",
                audioReady ? "ok" : "failed", settings.deviceName.c_str());
}

void loop() {
  osc.loop();
  ble.loop();
  serviceMdns();

  if (audio.update()) {
    if (settings.micOutputEnabled) {
      osc.sendMicEnergy(audio.energy());
      ble.sendMicEnergy(audio.energy());
    }
  }

  float spectrum[32] = {};
  const bool spectrumReady = audio.takeSpectrum(spectrum);

  if (imu.update(imuFrame)) {
    if (settings.imuOutputEnabled) {
      osc.sendImu(imuFrame);
      ble.sendImu(imuFrame);
    }
    if (imu.movementTriggered(imuFrame)) {
      osc.sendMovementTrigger();
      ble.sendMovementTrigger();
      ui.noteMotion();
    } else if (imu.motionDetected(imuFrame)) {
      ui.noteMotion();
    }
  }

  ui.loop(controls, imuFrame, audio.energy(), spectrum, spectrumReady,
          WiFi.status() == WL_CONNECTED,
          ble.enabled(), ble.connected(), settings.imuOutputEnabled,
          settings.micOutputEnabled);
  audio.setFftEnabled(ui.spectrumPageActive() && ui.fftCaptureActive());
  handleUiEvents();

  if (restartAtMs != 0 && static_cast<int32_t>(millis() - restartAtMs) >= 0) ESP.restart();

  delay(1);
}
