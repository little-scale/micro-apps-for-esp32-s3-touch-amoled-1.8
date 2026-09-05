# Third-party notices

The hardware-support libraries under `vendor/waveshare-v2` were copied from the official [Waveshare ESP32-S3-Touch-AMOLED-1.8 repository](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8) at commit `7ab8f957e22ea1ab811256359f4eddcaaf49ee91` (2026-08-21).

Included libraries:

- GFX Library for Arduino — see `vendor/waveshare-v2/GFX_Library_for_Arduino/license.txt`
- Arduino DriveBus — see `vendor/waveshare-v2/Arduino_DriveBus/LICENSE`
- SensorLib — see `vendor/waveshare-v2/SensorLib/LICENSE`
- XPowersLib — see `vendor/waveshare-v2/XPowersLib/LICENSE`
- Adafruit BusIO — see `vendor/waveshare-v2/Adafruit_BusIO/LICENSE`
- Adafruit XCA9554 — source headers contain the applicable copyright and licence notice
- Waveshare `Mylibrary/pin_config.h` — V2 board pin definitions from the repository above

The ES8311 register sequence in `firmware/AudioService.cpp` is a minimal adaptation of the
Apache-2.0-licensed Espressif ES8311 driver included in Waveshare's official V2 Arduino
example repository linked above.

The build also uses Espressif's Arduino-ESP32 core and its bundled networking, Preferences and BLE libraries. These are installed by Arduino CLI rather than copied into this project.
