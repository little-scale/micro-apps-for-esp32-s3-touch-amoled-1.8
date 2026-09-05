#include "ConfigPortal.h"

#include <WiFi.h>

namespace {
constexpr uint32_t kPortalTimeoutMs = 150000;

uint16_t parsePort(const String &text, uint16_t fallback) {
  const long value = text.toInt();
  return value >= 1 && value <= 65535 ? static_cast<uint16_t>(value) : fallback;
}
}  // namespace

void ConfigPortal::begin(DeviceSettings &settings, ConfigStore &store) {
  settings_ = &settings;
  store_ = &store;
  if (!routesConfigured_) configureRoutes();
}

bool ConfigPortal::start() {
  if (active_ || !settings_) return active_;
  WiFi.mode(WIFI_AP_STA);
  String apName = settings_->deviceName + "-setup";
  if (!WiFi.softAP(apName.c_str())) return false;
  dns_.start(53, "*", WiFi.softAPIP());
  server_.begin();
  active_ = true;
  lastActivityMs_ = millis();
  return true;
}

void ConfigPortal::stop() {
  if (!active_) return;
  dns_.stop();
  server_.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  active_ = false;
}

void ConfigPortal::loop() {
  if (!active_) return;
  dns_.processNextRequest();
  server_.handleClient();
  if (millis() - lastActivityMs_ >= kPortalTimeoutMs) stop();
}

void ConfigPortal::configureRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.on("/calibrate", HTTP_POST, [this]() { handleCalibrate(); });
  server_.on("/factory-reset", HTTP_POST, [this]() { handleFactoryReset(); });
  server_.on("/generate_204", HTTP_ANY, [this]() { redirectToRoot(); });
  server_.on("/hotspot-detect.html", HTTP_ANY, [this]() { redirectToRoot(); });
  server_.on("/connecttest.txt", HTTP_ANY, [this]() { redirectToRoot(); });
  server_.onNotFound([this]() { redirectToRoot(); });
  routesConfigured_ = true;
}

void ConfigPortal::handleRoot() {
  lastActivityMs_ = millis();
  server_.send(200, "text/html", page());
}

void ConfigPortal::handleSave() {
  lastActivityMs_ = millis();
  if (!settings_ || !store_) {
    server_.send(500, "text/plain", "Unavailable");
    return;
  }
  settings_->deviceName = cleanDeviceName(server_.arg("name"));
  settings_->wifiSsid = server_.arg("ssid");
  settings_->wifiPassword = server_.arg("password");
  settings_->oscTarget = server_.arg("osc_target");
  settings_->oscTarget.trim();
  settings_->oscSendPort = parsePort(server_.arg("osc_send"), 9000);
  settings_->oscReceivePort = parsePort(server_.arg("osc_receive"), 9001);
  settings_->imuRateHz = server_.arg("imu_rate") == "50" ? 50 : 25;
  const long dimSeconds = server_.arg("dim_seconds").toInt();
  settings_->dimAfterMs = static_cast<uint32_t>(constrain(dimSeconds, 15L, 600L)) * 1000UL;
  if (!store_->save(*settings_)) {
    server_.send(500, "text/plain", "Could not save settings");
    return;
  }
  server_.send(200, "text/html",
               "<!doctype html><meta name=viewport content='width=device-width'><p>Saved. The device is restarting.</p>");
  restartRequested_ = true;
}

void ConfigPortal::handleCalibrate() {
  lastActivityMs_ = millis();
  calibrationRequested_ = true;
  server_.send(200, "text/html",
               "<!doctype html><meta name=viewport content='width=device-width'><p>Calibration started. Keep the device still and flat for three seconds.</p><p><a href='/'>Back</a></p>");
}

void ConfigPortal::handleFactoryReset() {
  lastActivityMs_ = millis();
  if (server_.arg("confirm") != "yes") {
    server_.send(400, "text/plain", "Tick the confirmation box first.");
    return;
  }
  factoryResetRequested_ = true;
  server_.send(200, "text/html",
               "<!doctype html><meta name=viewport content='width=device-width'><p>Factory settings restored. The device is restarting.</p>");
}

void ConfigPortal::redirectToRoot() {
  server_.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
  server_.send(302, "text/plain", "");
}

bool ConfigPortal::consumeRestartRequest() {
  const bool requested = restartRequested_;
  restartRequested_ = false;
  return requested;
}

bool ConfigPortal::consumeCalibrationRequest() {
  const bool requested = calibrationRequested_;
  calibrationRequested_ = false;
  return requested;
}

bool ConfigPortal::consumeFactoryResetRequest() {
  const bool requested = factoryResetRequested_;
  factoryResetRequested_ = false;
  return requested;
}

String ConfigPortal::htmlEscape(const String &input) {
  String output;
  output.reserve(input.length() + 12);
  for (size_t i = 0; i < input.length(); ++i) {
    switch (input[i]) {
      case '&': output += "&amp;"; break;
      case '<': output += "&lt;"; break;
      case '>': output += "&gt;"; break;
      case '\"': output += "&quot;"; break;
      case '\'': output += "&#39;"; break;
      default: output += input[i];
    }
  }
  return output;
}

String ConfigPortal::cleanDeviceName(const String &input) {
  String output;
  output.reserve(24);
  for (size_t i = 0; i < input.length() && output.length() < 24; ++i) {
    char value = static_cast<char>(tolower(static_cast<unsigned char>(input[i])));
    if ((value >= 'a' && value <= 'z') || (value >= '0' && value <= '9')) {
      output += value;
    } else if (!output.isEmpty() && !output.endsWith("-")) {
      output += '-';
    }
  }
  while (output.endsWith("-")) output.remove(output.length() - 1);
  return output.isEmpty() ? String("device-0000") : output;
}

String ConfigPortal::page() const {
  const String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "not connected";
  String html;
  html.reserve(5000);
  html += F("<!doctype html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
            "<title>Device setup</title><style>body{font:16px system-ui;max-width:34rem;margin:2rem auto;padding:0 1rem;background:#111;color:#eee}"
            "h1{font-size:1.35rem}fieldset{border:1px solid #555;margin:1rem 0;padding:1rem}label{display:block;margin:.8rem 0 .25rem}"
            "input,select,button{box-sizing:border-box;width:100%;padding:.7rem;font:inherit;background:#222;color:#fff;border:1px solid #777;border-radius:4px}"
            "button{margin-top:1rem;background:#eee;color:#111;border:0;font-weight:600}.danger{border-color:#b55}small{color:#aaa}</style></head><body>"
            "<h1>Classroom controller setup</h1><p>Current IP: ");
  html += htmlEscape(ip);
  html += F("</p><form method=post action=/save><fieldset><legend>Identity</legend><label>Device name</label><input name=name maxlength=24 value='");
  html += htmlEscape(settings_->deviceName);
  html += F("' required><small>Used for Wi-Fi, BLE and OSC paths.</small></fieldset><fieldset><legend>Wi-Fi</legend>"
            "<label>Network name (SSID)</label><input name=ssid value='");
  html += htmlEscape(settings_->wifiSsid);
  html += F("'><label>Password</label><input name=password value='");
  html += htmlEscape(settings_->wifiPassword);
  html += F("'></fieldset><fieldset><legend>OSC</legend><label>Computer IP address</label><input name=osc_target value='");
  html += htmlEscape(settings_->oscTarget);
  html += F("'><label>Send port</label><input name=osc_send type=number min=1 max=65535 value='");
  html += settings_->oscSendPort;
  html += F("'><label>Receive port</label><input name=osc_receive type=number min=1 max=65535 value='");
  html += settings_->oscReceivePort;
  html += F("'></fieldset><fieldset><legend>Motion and display</legend><label>IMU update rate</label><select name=imu_rate><option value=25");
  if (settings_->imuRateHz == 25) html += F(" selected");
  html += F(">25 Hz</option><option value=50");
  if (settings_->imuRateHz == 50) html += F(" selected");
  html += F(">50 Hz</option></select><label>Dim after (seconds)</label><input name=dim_seconds type=number min=15 max=600 value='");
  html += settings_->dimAfterMs / 1000UL;
  html += F("'></fieldset><button type=submit>Save and restart</button></form>"
            "<form method=post action=/calibrate><button type=submit>Calibrate IMU</button></form>"
            "<form class=danger method=post action=/factory-reset><fieldset><legend>Factory reset</legend>"
            "<label><input style='width:auto' type=checkbox name=confirm value=yes required> Erase saved settings</label>"
            "<button type=submit>Factory reset</button></fieldset></form></body></html>");
  return html;
}
