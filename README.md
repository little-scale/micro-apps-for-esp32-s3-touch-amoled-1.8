# Micro Apps for ESP32-S3 Touch AMOLED 1.8

Classroom-oriented firmware for the **Waveshare ESP32-S3-Touch-AMOLED-1.8 V2 (368 × 448)**. It turns the board into a compact collection of touch, motion and microphone micro-apps with OSC over Wi-Fi and raw BLE data, including shake/flick trigger messages.

The current eight-page implementation compiles for the V2 hardware and has been flashed and physically verified on an ESP32-S3-Touch-AMOLED-1.8 V2. Display orientation, touch mapping, IMU-driven interactions, OSC and BLE have all been exercised on the board.

## What is on the screen

- Settings/Wi-Fi, BLE, raw IMU-output, microphone-output and battery status in the top row
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
- Enter the password with the large on-screen keyboard. `ABC` changes case, `123` opens common digits and symbols, `MORE` opens the remaining symbols, and space/delete are always available. `SHOW`/`HIDE` reveals or masks the entered password.
- Tap `CONNECT`. Success returns to the controller automatically; failure returns to the password screen with a red frame.
- `EXIT` or 150 seconds without input returns to the controller.

The device does not create a configuration hotspot. The `OSC` screen edits the target IPv4 address, output port and device input port with a numeric keypad; saving applies the new routing immediately. The `DEVICE` screen edits the shared Wi-Fi hostname, BLE advertisement name and OSC address root. Saving a device name restarts the board so all transports adopt it together. The `PHYSICS` screen sets gravity for the ball and pendulum simulations plus ball collision bounciness. IMU options and reset will move to additional on-device screens later.

### Optional private provisioning

For a set of boards that should all join the same network and send OSC to the same computer, copy `firmware/Provisioning.local.h.example` to `firmware/Provisioning.local.h` and enter the private Wi-Fi and OSC values there. The local file is ignored by Git. A numbered provisioning revision applies those values once per board; increase it only when a later build should deliberately replace the saved network and OSC settings. Provisioned binaries also contain the password and should be kept private.

Tap the BLE icon to enable or disable BLE; hold it to clear bonds and advertise again.
Tap the adjacent three-axis IMU icon to enable or disable periodic raw IMU output over
OSC and BLE. It is pink when enabled, defaults to off, and the choice is saved. BLE uses amber
whenever it is enabled, while the settings badge uses green for a connected Wi-Fi network.
This does not disable motion-driven
pages or the movement trigger; it only suppresses the nine-float IMU stream when it is not needed.
Tap the microphone icon to enable or disable periodic microphone-energy output over OSC and BLE.
It defaults to off and the choice is saved. Disabling it suppresses only `mic0`; microphone particles,
FFT capture and their derived events continue to work locally.

## OSC message reference

OSC is sent over UDP. The default destination on the computer is port `9000`; the device listens
for incoming OSC on port `9001`. Both ports and the destination IPv4 address can be changed on the
device's `OSC` settings screen.

Replace `<device>` in every address below with the configured device name. For example, a device
named `device-1234` sends its XY controller as `/device-1234/xy0`. The same device name is used for
the Wi-Fi hostname and BLE advertisement.

OSC values use two data types:

- `f` — 32-bit floating-point value
- `i` — 32-bit signed integer

Signatures such as `2f`, `3i` and `32f` below are shorthand for two floats, three integers and 32
floats respectively. Argument order is significant.

### Device to computer

| OSC address | Type signature | Arguments and behaviour |
| --- | ---: | --- |
| `/<device>/xy0` | `2f` | `x y`; X is normalized from `0.0` left to `1.0` right, and Y from `0.0` bottom to `1.0` top |
| `/<device>/faderN` | `f` | Normalized fader value; `N` is `0`–`3`, bottom is `0.0`, top is `1.0` |
| `/<device>/buttonN` | `i` | Momentary button state; `N` is `0`–`3`, press sends `1`, release sends `0` |
| `/<device>/note` | `3i` | `midiNote state velocity`; note is `0`–`127`, note-on state is `1` with velocity `20`–`127`, note-off is `state 0, velocity 0` |
| `/<device>/spectrum0` | `32f` | Complete low-to-high 32-band multislider or FFT snapshot; fixed linear full-scale values without per-frame peak normalization |
| `/<device>/collision/wall` | `2i f` | `ball wall impact`; ball is `0`–`7`, wall is listed below, impact is normalized `0.0`–`1.0` |
| `/<device>/collision/ball` | `2i f` | `ballA ballB impact`; stable ball indices `0`–`7`, normalized impact `0.0`–`1.0` |
| `/<device>/movement` | `i` | Shake/flick onset sends `1`; there is no release or zero message |
| `/<device>/mic0` | `f` | Smoothed and noise-gated microphone energy from `0.0`–`1.0`; this is a loudness/density measure, not an audio waveform sample |
| `/<device>/particle/wall` | `i f` | `wall size`; wall number is listed below and particle size is normalized `0.0`–`1.0` |
| `/<device>/pendulumN` | `6f` | `ax ay bx by angle angularVelocity`; `N` is `0`–`3`, A/B positions are normalized, angle is degrees, angular velocity is degrees per second |
| `/<device>/pendulumN/active` | `i` | Pendulum `N` was created (`1`) or deleted (`0`) |
| `/<device>/pendulumN/centre` | `i` | Sends `1` when pendulum `N` crosses its instantaneous gravitational equilibrium point |
| `/<device>/pendulumN/left` | `i` | Sends `1` when pendulum `N` reaches its left turning point |
| `/<device>/pendulumN/right` | `i` | Sends `1` when pendulum `N` reaches its right turning point |
| `/<device>/imu0` | `9f` | `ax ay az gx gy gz pitch roll yaw`; acceleration is in g, angular velocity in degrees per second, and orientation in degrees |

`N` is replaced by the actual zero-based index, so fader 2 uses `/<device>/fader2`, not the
literal address `/<device>/faderN`.

Wall numbers are shared by the ball and particle pages:

| Wall | Side |
| ---: | --- |
| `0` | Left |
| `1` | Right |
| `2` | Top |
| `3` | Bottom |

Ball slots are stable: deleting a ball does not renumber the others, and the next ball uses the
lowest free index. The same rule applies to pendulum slots `0`–`3`. Sustained ball contact produces
one collision onset rather than a new message on every physics frame. Pendulum event messages contain
only `1`; there is no corresponding zero message.

### Computer to device

The computer can update the following controls using the same address and data format:

| OSC address | Type signature | Accepted data |
| --- | ---: | --- |
| `/<device>/xy0` | `2f` | Normalized `x y` |
| `/<device>/faderN` | `f` | Normalized value for fader `0`–`3` |
| `/<device>/buttonN` | `i` | State `0` or `1` for button `0`–`3` |
| `/<device>/note` | `3i` | `midiNote state velocity`; remote notes may be polyphonic |
| `/<device>/note` | `2i` | Legacy `midiNote state` form, also accepted |
| `/<device>/spectrum0` | `32f` | Complete 32-band bank, accepted only while the spectrum page is visible |
| `/<device>/background` | `3i` | `red green blue`; each colour component is `0`–`255` and the display changes immediately |

Incoming control messages wake and update the display but are never echoed back over OSC or BLE.
For example, an incoming fader value changes the on-device fader without producing a return message.
If a control is being touched locally, remote changes to that control are ignored until the finger is
released. Collision, movement, microphone, particle, pendulum and IMU messages are device outputs only.

### Timing and sensor details

- XY and fader movement is limited to 50 messages per second, plus a final value on release.
- The physical keyboard is single-touch and supports glissando; remote notes may be polyphonic.
- When its top-row toggle is enabled, microphone energy is sent at 25 Hz. Raw audio is never transmitted or stored.
- Pendulum state is limited to 25 Hz while drawing or simulating.
- IMU output is 25 Hz by default or 50 Hz when selected. It is disabled by default and controlled by the pink IMU badge.
- Acceleration uses the logical screen frame: +X right, +Y toward the top and +Z out of the display.
- Pitch and roll are gravity-corrected. Yaw is relative and will drift because the board has no magnetometer.
- Spectrum input, output and FFT processing stop while the spectrum page is hidden.

For BLE packet layouts, fragmentation rules and the complete transport specification, see
[PROTOCOL.md](PROTOCOL.md).

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
- `firmware/Provisioning.local.h.example` — safe template for optional private fleet provisioning
- `scripts/` — reproducible build and explicit-port flash commands
- `vendor/waveshare-v2/` — pinned Waveshare hardware dependencies and their licences
- `docs/` — rendered handover document and diagrams
- `tools/` — handover-document generator

Generated firmware, compiler caches and machine-local files are intentionally ignored. Runtime
Wi-Fi credentials are entered on the device; optional build-time credentials live only in the
ignored `firmware/Provisioning.local.h` file and must never be committed.

## Licence

The project is released under the [GNU General Public License v3.0](LICENSE). Bundled dependencies
retain their original licences and notices, documented in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

The computer-side Max/Ableton patch is intentionally outside this project.
