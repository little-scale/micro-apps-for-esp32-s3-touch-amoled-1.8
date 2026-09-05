# Micro Apps for ESP32-S3 Touch AMOLED 1.8

Classroom-oriented firmware for the **Waveshare ESP32-S3-Touch-AMOLED-1.8 V2 (368 × 448)**. It turns the board into a compact collection of touch, motion and microphone micro-apps with OSC over Wi-Fi and raw BLE data, including shake/flick trigger messages.

The current eight-page implementation compiles for the V2 hardware and has been flashed and physically verified on an ESP32-S3-Touch-AMOLED-1.8 V2. Display orientation, touch mapping, IMU-driven interactions, OSC and BLE have all been exercised on the board.

## What is on the screen

- Settings/Wi-Fi, BLE, raw IMU-output and battery status in the top row
- Page 1: a velocity-sensitive one-octave keyboard with octave down/up controls and single-touch glissando
- Page 2: four blank momentary buttons
- Page 3: one normalized XY pad
- Page 4: four normalized vertical faders
- Page 5: an initially empty arena for up to eight touch-created IMU-driven balls with wall and inter-ball OSC collisions; hold for 2.5 seconds to clear all balls
- Page 6: up to four touch-drawn IMU-gravity pendulums with editable bobs, tap-to-delete fixtures, indexed triggers, and a 2.5-second empty-space hold to centre them
- Page 7: a microphone-energy particle field entering from the onboard microphone corner, steered by IMU tilt, with OSC pings when particles reach a wall
- Page 8: a 32-band touch multislider with hold-to-capture microphone FFT snapshots
- One shared bottom control that advances through the eight pages

The main screen contains no labels. Incoming OSC or BLE values update it without being echoed back. An incoming background-colour command changes the whole display immediately. The page control is local only and sends no network message.

## First use and configuration

With no saved Wi-Fi network, the device automatically opens its on-screen network picker. Otherwise, tap the cog badge to open the `NETWORK` / `OSC` / `DEVICE` / `PHYSICS` settings menu.

- Select a visible network from the signal-sorted list.
- Use `MORE` to move through additional results or `SCAN` to refresh.
- Enter the password with the on-screen keyboard. `ABC` changes case, `#+=` opens symbols, space/delete are available, and `SHOW`/`HIDE` reveals or masks the entered password.
- Tap `CONNECT`. Success returns to the controller automatically; failure returns to the password screen with a red frame.
- `EXIT` or 150 seconds without input returns to the controller.

The device does not create a configuration hotspot. The `OSC` screen edits the target IPv4 address, output port and device input port with a numeric keypad; saving applies the new routing immediately. The `DEVICE` screen edits the shared Wi-Fi hostname, BLE advertisement name and OSC address root. Saving a device name restarts the board so all transports adopt it together. The `PHYSICS` screen sets gravity for the ball and pendulum simulations plus ball collision bounciness. IMU options and reset will move to additional on-device screens later.

Tap the BLE icon to enable or disable BLE; hold it to clear bonds and advertise again.
Tap the adjacent three-axis IMU icon to enable or disable periodic raw IMU output over
OSC and BLE. It is pink when enabled, defaults to off, and the choice is saved. BLE uses amber
whenever it is enabled, while the settings badge uses green for a connected Wi-Fi network.
This does not disable motion-driven
pages or the movement trigger; it only suppresses the nine-float IMU stream when it is not needed.

## Build

The reproducible build uses [Arduino CLI](https://arduino.github.io/arduino-cli/latest/installation/) and Espressif's ESP32 Arduino core 3.3.10. Waveshare's V2 hardware libraries are pinned in `vendor/waveshare-v2`, so no separate library installation is required.

Install the ESP32 platform once:

```sh
arduino-cli core update-index \
  --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.10 \
  --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Then build from the repository root:

```sh
./scripts/build.sh
```

The ready-to-flash merged image is produced at:

`build/output/firmware.ino.merged.bin`

## Flash

Flashing replaces the factory firmware, but does not erase the whole flash or deliberately clear saved settings. Build first, then pass the board's serial port explicitly:

```sh
./scripts/flash.sh /dev/cu.usbmodem101
```

If a port is not known, `arduino-cli board list` shows attached boards. The project does not automatically flash a device.

## Verification on another board

After flashing another unit:

1. Confirm the display is upright and every corner of the XY pad tracks the corresponding touch corner.
2. Place the board flat before running IMU calibration once that on-device settings screen is added.
3. Tilt the right edge up and confirm the documented screen-frame axis signs in `PROTOCOL.md`.
4. Verify OSC reception and remote no-echo behaviour.
5. Connect a BLE client, request an MTU of at least 64, and verify complete IMU packets; also test the three-fragment fallback at the default MTU.

Yaw is relative and will drift because the QMI8658 is a six-axis IMU with no magnetometer.

## Reference documents

- [Full behaviour specification](FIRMWARE_SPEC.md)
- [OSC and BLE protocol reference](PROTOCOL.md)
- [Third-party software notices](THIRD_PARTY_NOTICES.md)
- [Detailed developer handover](DEVELOPER_HANDOVER.md)

## Repository layout

- `firmware/` — application source
- `scripts/` — reproducible build and explicit-port flash commands
- `vendor/waveshare-v2/` — pinned Waveshare hardware dependencies and their licences
- `docs/` — rendered handover document and diagrams
- `tools/` — handover-document generator

Generated firmware, compiler caches and machine-local files are intentionally ignored. Runtime
Wi-Fi credentials are entered on the device and are never written into this repository.

## Licence

The project is released under the [GNU General Public License v3.0](LICENSE). Bundled dependencies
retain their original licences and notices, documented in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

The computer-side Max/Ableton patch is intentionally outside this project.
