# ESP32 S3 Touch AMOLED Classroom Controller Developer Handover

**Project baseline:** 5 September 2026  
**Target hardware:** Waveshare ESP32-S3-Touch-AMOLED-1.8 V2  
**Display:** 368 × 448 portrait AMOLED using CO5300  
**Touch:** CST820 single-point capacitive touch  
**Current firmware:** eight active performance pages, on-device Wi-Fi and OSC setup, BLE GATT, IMU, microphone energy, FFT capture, remote control, and no-echo routing  
**Verified build size:** 1,386,659 bytes of a 3,145,728-byte application partition, 44 percent
**Verified RAM use:** 99,340 bytes of 327,680 bytes of static dynamic-memory allocation, 30 percent

This is the main handover for anyone continuing the firmware in a fresh development context. Read it together with [README.md](README.md), [FIRMWARE_SPEC.md](FIRMWARE_SPEC.md), and [PROTOCOL.md](PROTOCOL.md). The specification describes intended behaviour; the protocol reference is the contract with Max, Ableton, or another host; this document explains why the firmware is shaped as it is and how to extend it safely.

## Current status at a glance

The firmware builds, flashes, boots, draws correctly on V2 hardware, joins Wi-Fi, sends and receives OSC, advertises and connects over BLE, streams full-precision IMU values, reads the microphone, and responds to touch. The physical prototype has been used repeatedly to tune the screen geometry, touch targets, IMU direction, drawing cadence, particle rendering, and settings workflow.

The former radar sequencer has deliberately been removed from the compiled interface and its OSC endpoint. The current page count is eight. This leaves a clear extension point for students to add a ninth page of their own without inheriting a finished solution. A reconstruction of the radar is included later as a worked example, not as active firmware.

The current firmware was built and flashed successfully to `/dev/cu.usbmodem1101` after radar removal, final page reordering, and addition of the saved raw-IMU output toggle. A normal flash preserves saved settings because the project does not issue a full-chip erase.

## Product intent

This is a classroom musical-interface instrument rather than a programming exercise. Students should be able to explore touch, motion, sound, Wi-Fi, BLE, OSC, Max, and Ableton without first understanding the firmware implementation. The device therefore favours direct manipulation, no labels on performance pages, immediately visible feedback, predictable normalized values, and a small number of explicit configuration screens.

The visual language is intentionally unlike a website. It uses a black or remotely selected solid background, hard-edged white positive space, thick geometry, and very little text outside settings. The 25-pixel XY cross and button borders roughly acknowledge fingertip scale. Status remains in a narrow top strip, and one blank full-width button at the bottom changes page.

The software is also a teaching platform. New behaviours should expose a legible relationship between a physical gesture and a message arriving at the computer. Keep that relationship easy to explain aloud.

## Hardware identity and version warning

This project targets the board marked **V2** on its rear label. Waveshare changed important display and touch hardware between revisions. V2 uses a CO5300 display controller and CST820 touch controller. Older V1 examples use SH8601 and FT3168 and are not drop-in replacements.

The verified V2 hardware includes:

| Part | Role | Project use |
| --- | --- | --- |
| ESP32-S3R8 | Dual-core microcontroller with 2.4 GHz Wi-Fi and BLE | Main application, UDP, BLE, display and sensors |
| 16 MB NOR flash | Program and file storage | 3 MB application partition plus filesystem space |
| 8 MB PSRAM | External working memory | Full and partial display canvases |
| CO5300 | 368 × 448 AMOLED controller | QSPI portrait display |
| CST820 | Capacitive touch controller | One touch point over I2C |
| QMI8658 | Three-axis accelerometer and gyroscope | Raw 6DoF, orientation, motion triggers and physics |
| ES8311 | Audio codec | Onboard microphone capture through I2S |
| AXP2101 | Power management | Battery level, charging state and display power support |
| XCA9554 | I/O expander | Peripheral reset and enable sequencing |
| PCF85063 | Real-time clock | Present on board but not used by this firmware |

The authoritative board starting points are the [Waveshare V2 documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.8), [Waveshare resource downloads](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.8/Resources-And-Documents), and [product page](https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm). Waveshare’s documentation explicitly identifies V2 as CO5300 plus CST820 and lists 368 × 448 resolution, QMI8658, ES8311, AXP2101, 8 MB PSRAM, and 16 MB flash.

### Pin definitions

The vendor-derived pin source of truth is `vendor/waveshare-v2/Mylibrary/pin_config.h`.

| Function | GPIO or address |
| --- | --- |
| Display QSPI data 0 to 3 | GPIO 4, 5, 6, 7 |
| Display clock | GPIO 11 |
| Display chip select | GPIO 12 |
| Shared I2C SDA and SCL | GPIO 15 and 14 |
| Touch interrupt | GPIO 21 |
| I2S MCLK, BCLK, WS, data in, data out | GPIO 16, 9, 45, 10, 8 |
| Audio power amplifier | GPIO 46 |
| XCA9554 I2C address | `0x20` |
| ES8311 I2C address | `0x18` |

Do not guess pins from a V1 tutorial. Keep the copied vendor libraries under `vendor/waveshare-v2` so builds do not silently change when a globally installed Arduino library is updated.

## Repository map

| Path | Responsibility |
| --- | --- |
| `firmware/firmware.ino` | Composition root, main loop, service ownership, event routing and no-echo rule |
| `firmware/AppTypes.h` | Shared control state, IMU frame, message types and saved settings model |
| `firmware/UserInterface.h` | Page state, touch targets, drawing state, settings workflow and UI event types |
| `firmware/UserInterface.cpp` | Display and touch initialization, all page interactions, physics and rendering |
| `firmware/ImuService.*` | QMI8658 setup, screen-axis transform, orientation and movement onset detection |
| `firmware/AudioService.*` | ES8311 and I2S setup, microphone energy and 256-point FFT |
| `firmware/OscTransport.*` | OSC 1.0 UDP encoding, decoding, addresses and validation |
| `firmware/BleTransport.*` | BLE advertising, GATT service, packet encoding, decoding and IMU fragmentation |
| `firmware/ConfigStore.*` | Nonvolatile Preferences storage and validation |
| `firmware/Provisioning.h` | Optional compile-time private Wi-Fi/OSC defaults with revision gating |
| `firmware/Provisioning.local.h.example` | Checked-in provisioning template; the populated `.local.h` is ignored |
| `firmware/ConfigPortal.*` | Earlier portal component retained in source but not used by the current no-hotspot flow |
| `vendor/waveshare-v2` | Pinned board support libraries copied from the V2 examples |
| `scripts/build.sh` | Reproducible Arduino CLI build with the correct board options |
| `scripts/flash.sh` | Verified upload of the already-built binary to an explicit serial port |
| `README.md` | Short user and developer starting point |
| `FIRMWARE_SPEC.md` | Detailed intended behaviour |
| `PROTOCOL.md` | Exact OSC and BLE contract |

The architecture is deliberately service-oriented without an operating-system task per service. The Arduino `loop()` rapidly services OSC, BLE, mDNS, audio, IMU, the UI, and queued UI events, then yields for one millisecond. Individual services use their own time gates rather than blocking delays.

```text
Touch ───────────────> UserInterface ── UiEvent ──> firmware.ino router
                              │                         │
Remote OSC ─> OscTransport ───┤                         ├─> OscTransport output
Remote BLE ─> BleTransport ───┤                         └─> BleTransport output
                              │
QMI8658 ─────> ImuService ────┼─> UI physics and visual state
                              ├─> OSC IMU and movement
ES8311 ──────> AudioService ──┼─> BLE IMU movement and mic
                              └─> particles and FFT capture
```

The central router is important. It is where local UI events are allowed to become outbound messages and where remote messages are applied without retransmission.

## Build and flash workflow

The checked-in scripts use Arduino CLI and Espressif Arduino core 3.3.10. The full board configuration is:

```text
esp32:esp32:esp32s3:
FlashSize=16M,
PartitionScheme=app3M_fat9M_16MB,
PSRAM=opi,
USBMode=hwcdc,
CDCOnBoot=cdc
```

Arduino CLI’s upload command does not compile automatically, so always build first. See the official [Arduino CLI getting-started guide](https://arduino.github.io/arduino-cli/0.30/getting-started/) and [upload command reference](https://arduino.github.io/arduino-cli/dev/commands/arduino-cli_upload/). Espressif’s [Arduino ESP32 installation guide](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html) describes installation of the board package.

From the project root:

```sh
./scripts/build.sh
arduino-cli board list
./scripts/flash.sh /dev/cu.usbmodem1101
```

The serial device name can change after reconnecting. Never bake it into the script. The build produces individual bootloader, partition and application images plus:

```text
build/output/firmware.ino.merged.bin
```

The September 2026 eight-page baseline with the larger setup keyboard, optional provisioning and saved sensor-output toggles reports 1,386,659 program bytes, or 44 percent of the 3 MB application partition. Static global allocation is 99,340 bytes, or 30 percent of internal RAM. The 8 MB PSRAM is used at runtime for display canvases and is not represented by that global allocation figure.

### Flash and recovery notes

- A normal upload changes firmware while preserving Preferences data such as Wi-Fi credentials and device name. A private provisioning header overrides Wi-Fi/OSC values only when its numbered revision is newer than the board's stored Micro Apps provisioning revision.
- If a bad sketch prevents automatic upload, hold BOOT while connecting or resetting, then retry with the detected serial port.
- A full erase is materially different from a normal flash because it removes saved settings. Use it only intentionally.
- Waveshare publishes demo and factory-style binaries in its resource area, so the project does not retain a factory image.
- Successful upload ends with hash verification and a hard reset.

## Main interface baseline

The screen is 368 pixels wide by 448 high. The stable layout is:

| Region | Geometry | Purpose |
| --- | --- | --- |
| Top status | `y 0…42` | Settings, BLE, raw IMU-output and battery state |
| Performance canvas | `x 20…347`, `y 57…366` | A 328 × 310 page area |
| Page control | `x 20…347`, `y 385…433` | One blank 328 × 49 next-page button |
| Page marks | Inside bottom control | Eight small square indicators |

The touch target for the bottom control extends through the complete strip below the performance canvas. This is intentional: touch accuracy falls near the rounded lower glass edge. The visual button is not moved to match that expanded forgiving target.

### Active page order

| Page | Interaction | Principal output |
| ---: | --- | --- |
| 1 | One-octave velocity keyboard with octave controls | Note, state and velocity |
| 2 | Four momentary square buttons | `/<device>/buttonN state` |
| 3 | Normalized XY controller | `/<device>/xy0 x y` |
| 4 | Four vertical multislider-style faders | `/<device>/faderN value` |
| 5 | Up to eight IMU-driven colliding balls | Wall and ball collision events |
| 6 | Up to four IMU-gravity pendulums | State, active and centre or turning-point pings |
| 7 | Microphone-density particles steered by IMU | Mic energy and particle wall pings |
| 8 | 32-band multislider and held FFT capture | One 32-float spectrum bank |

Page change is local only. It does not send OSC or BLE and does not modify control values. Hidden page state is retained. Expensive page-specific work is paused when hidden: ball and pendulum physics do not accumulate a time jump, particle drawing runs only on its page, and FFT analysis runs only while page 8’s capture area is held.

### Visual language

- Performance background is RGB `0 0 0` by default and may be changed immediately over OSC or BLE.
- Primary controls are white positive space with the current background used as negative space.
- General thin lines are 5 pixels. Major touch-scale shapes are 25 pixels.
- Performance pages contain no text or symbols except the piano’s structural key shapes.
- Setup screens may use crisp bitmap text because they are occasional, explicit configuration surfaces.
- Remote background colour should never reduce control contrast by recolouring white geometry.

## Touch handling

The CST820 supplies one touch point. The firmware polls every 8 ms and treats touch as a gesture with start, move and end phases. `hitTest()` chooses the control only at touch-down, except for the fader page where horizontal movement deliberately transfers the gesture into another column.

Continuous controls send at no more than 50 Hz and send a final value on release. The XY input applies adaptive filtering plus a 1.5-pixel deadband to reduce coordinate jitter without making deliberate motion laggy. Values are clamped before transmission.

Important interaction conventions:

- XY uses X left 0 to right 1 and Y bottom 0 to top 1.
- Faders use bottom 0 to top 1.
- Sliding sideways across faders edits each newly entered fader, like Max’s `multislider`.
- Buttons always produce a release after a valid press even if the finger leaves their visual area.
- Physical keyboard touch is monophonic and supports glissando by sending the old note off before the new note on.
- Long-hold actions are 2.5 seconds for ball clear and pendulum centre reset.
- The settings screen closes silently after 150 seconds without touch.

When adding a control, define its visible bounds and hit area together. If the hit area is deliberately enlarged for edge usability, record that fact beside the code. Earlier versions of this project became confusing when button images and touch rectangles drifted apart.

Touch startup is deliberately defensive. The CST816 can respond late after a USB flash/reset,
so `UserInterface` retries initialization four times and continues a low-rate recovery attempt if
all startup probes fail. A transient startup race must not leave the interface untouchable until the
next reboot.

## Display rendering and performance lessons

Rendering has been the most hardware-specific part of this work. The interface originally showed trails, partial redraws, white flashes, tearing, and slow interaction. The stable approach now uses PSRAM-backed canvases and bounded transfers.

Three canvases are allocated:

- a 368 × 44 top-strip canvas
- a 328 × 312 performance canvas, including invisible alignment rows
- a 368 × 448 full settings canvas

The CO5300 region-transfer path behaves best when update starts are even and inclusive ends are odd. `flushXyRegion()` expands dirty rectangles to those boundaries. The performance canvas begins one hidden row above the visible page and includes one hidden row below it so alignment does not move the visible interface.

The main visual cadence is 50 Hz, measured start-to-start so display transfer time is part of the frame budget. The particle page draws at 30 Hz because it has many moving objects. Status pixels transfer only when status actually changes, avoiding an unrelated top-strip stall during animation.

The XY page is the main optimized case. A full redraw is used on page entry. During movement, horizontal and vertical dirty bands are transferred from the already-composited canvas. The narrow vertical strip is copied into a contiguous buffer before transfer, avoiding a full 328-pixel stride for a 25-pixel line. New position data is sent before restoring a distant old line so the user sees at worst a momentary duplicate rather than a conspicuous blank flash.

Practical rules for new pages:

- Render the complete desired region into memory before transferring it.
- Avoid drawing individual primitives directly to the panel inside a high-rate loop.
- Prefer one composed region transfer to many small bus transactions.
- Do not redraw the top strip unless its state changes.
- Time simulations with elapsed seconds and cap large `dt` values after page changes.
- Do not allocate memory in a frame loop.
- If a page has many moving objects, cap both object count and draw cadence.
- Test vertical motion as well as horizontal motion; panel scan artefacts are direction-dependent.
- Preserve even-start and odd-end transfer alignment for CO5300 partial updates.

## IMU frame and orientation

`ImuService` configures the QMI8658 accelerometer for ±4 g at 250 Hz ODR and the gyroscope for ±512 degrees per second at about 224 Hz ODR. Firmware samples at 25 Hz by default or 50 Hz when selected in saved settings.

The chip is mounted 90 degrees relative to the portrait display. Raw sensor axes are mapped once, centrally:

```cpp
screenAccelX = rawAccelY;
screenAccelY = rawAccelX;
screenAccelZ = rawAccelZ;

screenGyroX = rawGyroY - biasX;
screenGyroY = rawGyroX - biasY;
screenGyroZ = rawGyroZ - biasZ;
```

The public logical frame is:

- positive X toward screen-right
- positive Y toward screen-top
- positive Z out through the display
- acceleration in g
- angular velocity in degrees per second
- pitch, roll and yaw in degrees

OSC and BLE use this public frame unchanged. Screen physics may derive a visually convenient force from it. The verified ball, pendulum and particle mapping uses negative logical X for display-horizontal gravity and positive logical Y for display-vertical gravity. Keep this sign choice local to the visual simulation rather than changing the shared IMU protocol.

Pitch and roll use a complementary filter with 97 percent gyro integration and 3 percent accelerometer correction. Yaw is gyro integration only and therefore relative; it will drift because this board has no magnetometer. Calibration averages 150 stationary readings for gyro bias and stores pitch and roll offsets, although a finished calibration control has not yet been added to the settings UI.

Movement onset compares consecutive frames. A vector delta of at least 0.28 g or 35 degrees per second emits a single movement trigger. A 250 ms retrigger interval prevents floods while allowing rhythmic shakes.

Raw IMU streaming is deliberately opt-in. The three-axis badge immediately after BLE is grey
when streaming is off and pink when it is on. A tap toggles and saves `imuOutputEnabled`, whose
default is false. Only the periodic OSC `imu0` packet and BLE IMU notifications are gated. Sampling,
pitch/roll/yaw calculation, page physics, motion wake, and the compact movement trigger continue.

The top-row colour identities are intentionally stable: green means settings/Wi-Fi connected,
amber means BLE enabled, pink means raw IMU output enabled, and cyan means microphone-energy
output enabled. BLE no longer changes from amber
to green merely because a client connects; connection state remains available internally.

## Microphone and FFT

`AudioService` configures the ES8311 for 16 kHz, 16-bit stereo I2S and chooses the louder received channel. Audio is processed locally; raw samples are neither stored nor sent.

Microphone energy is short-window RMS after DC removal. An adaptive ambient floor, noise gate, fast attack, slower release and a square-root loudness curve produce a stable normalized density value from 0 to 1. This is designed for breath and activity rather than calibrated sound-pressure measurement.

Periodic microphone-energy transport is opt-in and defaults to off. The microphone badge beside
the IMU badge saves `micOutputEnabled` and gates only OSC `mic0` plus the equivalent BLE packet.
Audio capture and all local consumers continue running, so the particle and FFT pages behave
normally and particle wall events remain available while the badge is grey.

FFT capture is intentionally different:

- 256 microphone samples
- DC removal
- Hann window
- in-place radix-2 transform
- bins 1 through 128 grouped four at a time into 32 linear bands
- about 250 Hz per band at a 16 kHz sample rate
- fixed full-scale reference
- no frame-by-frame peak normalization
- no temporal averaging
- no square-root boost

The result therefore preserves loud-versus-quiet differences. Values only clip at 1.0 because the on-screen faders and current OSC contract are normalized. FFT sampling drains queued I2S blocks so display work cannot create an ever-growing audio-analysis delay.

## Configuration and identity

The cog badge opens a fully on-device settings interface. There is no temporary hotspot and no web page. A new device with no stored SSID automatically opens the network picker.

The settings flow supports:

- scanning and paging through visible networks
- password entry with large seven-key-or-fewer rows and separate letters, common-symbol, and additional-symbol modes
- masked or visible password text
- connect success and failure states
- OSC target IPv4 address, transmit port and receive port
- canonical device name
- ball and pendulum gravity strength
- ball bounciness
- explicit exit and silent 150-second inactivity exit

The BLE and raw-IMU badges are direct status-row controls rather than settings pages. BLE remains
enabled by default. Raw IMU transmission remains disabled by default so unused nine-float streams
do not consume Wi-Fi airtime, BLE notification bandwidth, or host processing.

The device name defaults to `device-XXXX`, with the suffix derived from the final four hexadecimal characters of the ESP32 unique identity. It is sanitized to lowercase letters, digits and hyphens. One name controls all three identities:

```text
Wi-Fi hostname     <device>.local
BLE advertisement <device>
OSC root           /<device>/
```

Saving a new name restarts the board so every transport adopts it consistently. Wi-Fi runs in station mode and reconnects automatically. mDNS advertises the OSC receive service. Espressif’s official [Wi-Fi API documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html) covers station mode and scanning.

Preferences use namespace `classroom` and keys visible in `ConfigStore.cpp`. Passwords are stored in ESP32 nonvolatile preferences as plain configuration data, not as a hardened credential vault. That is acceptable for this explicitly classroom-oriented design but should be reconsidered for sensitive networks.

For repeated classroom-board flashing, `Provisioning.h` optionally imports an ignored `Provisioning.local.h`. A populated private file supplies the shared SSID, password, OSC host, and ports. `ConfigStore` records its revision under the Micro Apps-specific `micro_prov` key, so a revision is applied only once and does not collide with TouchOSC Parser Mini's provisioning marker. Increment the revision only for an intentional fleet-wide settings replacement. Never commit the populated header or distribute its compiled binary publicly, because the password is embedded in both.

Wi-Fi scanning deliberately suspends auto-reconnect, pauses briefly after station mode is selected, and retries empty or failed asynchronous scans up to twice. Closing Settings restores auto-reconnect and the last selected credentials. A release guard consumes the physical touch that closed Settings before normal page hit-testing resumes; without it, the same held contact can activate the underlying performance control.

## OSC contract

OSC 1.0 messages are sent in UDP datagrams. OSC integers and floats are encoded big-endian as required by the [OSC 1.0 specification](https://opensoundcontrol.stanford.edu/spec-1_0.html). The default computer receive port is 9000 and device receive port is 9001.

The canonical address root is the device name. For `device-1234`, examples are:

```text
/device-1234/xy0 0.25 0.80
/device-1234/fader2 0.61
/device-1234/button0 1
/device-1234/button0 0
/device-1234/imu0 ax ay az gx gy gz pitch roll yaw
/device-1234/background 24 0 80
```

### Device to computer summary

| Address | Arguments | Rate or event |
| --- | --- | --- |
| `/<device>/xy0` | two float32 values | Up to 50 Hz while touched plus final |
| `/<device>/faderN` | one float32 | Up to 50 Hz while touched plus final |
| `/<device>/buttonN` | int32 state | Press 1 and release 0 |
| `/<device>/note` | MIDI note, state, velocity as int32 | Touch transitions |
| `/<device>/spectrum0` | 32 float32 values low to high | Page 8 edits or FFT release |
| `/<device>/imu0` | nine float32 values | 25 or 50 Hz while locally enabled |
| `/<device>/movement` | int32 1 | Detected onset |
| `/<device>/mic0` | one float32 | 25 Hz |
| `/<device>/collision/wall` | ball, wall, impact | New ball-wall contact |
| `/<device>/collision/ball` | ball A, ball B, impact | New ball-ball contact |
| `/<device>/particle/wall` | wall, particle size | Particle removal at wall |
| `/<device>/pendulumN` | A and B coordinates, angle, angular velocity | Up to 25 Hz per pendulum |
| `/<device>/pendulumN/active` | int32 state | Create or delete |
| `/<device>/pendulumN/centre` | int32 1 | Equilibrium crossing |
| `/<device>/pendulumN/left` | int32 1 | Left turning point |
| `/<device>/pendulumN/right` | int32 1 | Right turning point |

Wall numbers are 0 left, 1 right, 2 top, and 3 bottom. Stable ball indices are 0 through 7; stable pendulum indices are 0 through 3. Removed objects do not renumber survivors.

The computer may write XY, fader, button, note, spectrum, and background. Background uses three integer values from 0 through 255. Exact signatures and edge cases remain authoritative in [PROTOCOL.md](PROTOCOL.md).

### Max starting point

Max’s built-in [`udpreceive`](https://docs.cycling74.com/reference/udpreceive/) listens on a local UDP port and can decode messages; [`udpsend`](https://docs.cycling74.com/reference/udpsend/) sends to a host and port. A minimal teaching patch can begin with:

```text
[udpreceive 9000]
        |
[route /device-1234/xy0 /device-1234/imu0 /device-1234/movement]

[/device-1234/background 255 0 80]
        |
[udpsend 192.168.1.123 9001]
```

Whether a Max version outputs decoded OSC messages directly or a packet form depends on object settings. Confirm the device’s configured destination is the computer’s current IPv4 address, firewall permits inbound UDP 9000, and Max is listening before debugging firmware.

## BLE contract

BLE exposes one custom service with three characteristics:

| Purpose | UUID | Properties |
| --- | --- | --- |
| Primary service | `89689de0-1604-42c1-85ba-e286075c4488` | Service |
| Controls | `c2bbf0cb-9029-4925-abbc-3dd86492fc06` | Notify and Write Without Response |
| IMU | `2e5abe1b-4f44-429e-8b47-1526afbe343d` | Notify |
| Information | `fe9c3dcb-451c-4512-b845-a611ffa3280f` | Read |

The information characteristic contains JSON identifying protocol version 1, the device name, and IEEE-754 little-endian floats. The status BLE badge can enable or disable advertising. Holding it for 1.2 seconds clears bonds and restarts advertising.

Every binary packet starts with a six-byte header:

| Byte | Meaning |
| ---: | --- |
| 0 | Protocol version, `0x01` |
| 1 | Message type |
| 2 | Index |
| 3 | Flags, currently zero |
| 4 and 5 | Little-endian unsigned sequence number |
| 6 onward | Type-specific payload |

Control floats and all IMU floats are IEEE-754 binary32 little-endian. This is deliberately not BLE MIDI; Max or another host can reconstruct exact 32-bit values and map them to MIDI if desired. Espressif’s [Arduino BLE overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ble.html) and [ESP32-S3 BLE guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/ble/index.html) provide stack background.

### BLE IMU packets

A full IMU frame is 42 bytes: six header bytes plus nine floats. The firmware requests ATT MTU 64. If the connection negotiates a smaller MTU, the same sample is split into three 18-byte notifications sharing one sequence number:

| Type | Meaning | Payload order |
| ---: | --- | --- |
| `0x10` | Complete IMU | `ax ay az gx gy gz pitch roll yaw` |
| `0x11` | Accelerometer fragment | `ax ay az` |
| `0x12` | Gyroscope fragment | `gx gy gz` |
| `0x13` | Orientation fragment | `pitch roll yaw` |

Example JavaScript decoder logic suitable for adaptation in Node for Max:

```javascript
function decodePacket(bytes) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (view.getUint8(0) !== 1 || bytes.byteLength < 6) return null;

  const type = view.getUint8(1);
  const index = view.getUint8(2);
  const sequence = view.getUint16(4, true);
  const floats = [];
  for (let offset = 6; offset + 3 < bytes.byteLength; offset += 4) {
    floats.push(view.getFloat32(offset, true));
  }
  return { type, index, sequence, floats };
}
```

Subscribe to notifications on the IMU characteristic for motion, then enable the adjacent three-axis
badge on the device. Subscribe to Controls for touch, note, microphone energy, and movement trigger
messages. Write remote XY, fader, button, note, or background packets to Controls using Write Without
Response. There is intentionally no remote command for toggling the raw stream in protocol version 1.

## No echo and ownership rule

No echo is a defining system behaviour, not a minor optimization.

```text
Local touch ──> local state ──> OSC and BLE output
Remote OSC ───> local state ──> display only
Remote BLE ───> local state ──> display only
```

A received control value is never forwarded to the other transport and never returned to its sender. Otherwise a Max patch that mirrors values could create a feedback loop. While a control is touched locally, remote changes for that same control are ignored until release so the user retains physical ownership of the gesture. Sensor streams are different: IMU and microphone data originate on the device and may be sent independently on both active transports.

When adding a remote-capable control, update all of these together:

1. `MessageType` and shared state in `AppTypes.h`
2. local event generation in `UserInterface`
3. local routing in `sendLocalControl()`
4. OSC and BLE encoders
5. strict inbound validation
6. `applyRemoteMessage()` without outbound calls
7. active-touch ownership in `isControlActive()`
8. `PROTOCOL.md`

## Worked extension example from the retired radar

The removed radar page is a useful model because it combined touch-created objects, quantization, IMU mapping, animation, event timing, OSC output, and hidden-page suspension. It is intentionally described rather than compiled.

### Behaviour of the retired page

- A white circular arena contained eight concentric C-major pitch lanes, C D E F G A B C.
- Up to 16 nodes occupied stable indices.
- A tap inside the circle created or removed a node.
- Radius snapped to the closest pitch lane while angle remained continuous.
- A continuously clockwise ray acted as playhead.
- Projected tilt magnitude accelerated sweep speed from 35 to 240 degrees per second by 0.8 g.
- Speed used a 200 ms low-pass response so hand tremor did not become tempo jitter.
- Crossing a node emitted indexed note-on at velocity 100, followed by note-off after 120 ms.
- Holding outside the circle for 2.5 seconds cleared all nodes.
- Work stopped while the page was hidden.

### Quantizing node radius

The useful principle is to store musical meaning and derive pixels for drawing. For eight lanes, convert radius to a normalized lane position, round it, then reconstruct the display radius:

```cpp
float lanePosition = (touchRadius - innerRadius) /
                     (outerRadius - innerRadius) * 7.0f;
uint8_t lane = constrain((int)roundf(lanePosition), 0, 7);
float snappedRadius = innerRadius +
                      lane * (outerRadius - innerRadius) / 7.0f;

const uint8_t cMajor[8] = {60, 62, 64, 65, 67, 69, 71, 72};
node.midiNote = cMajor[lane];
node.angle = atan2f(y - centreY, x - centreX);
node.radius = snappedRadius;
```

This keeps note selection stable even if visual dimensions change later.

### Mapping IMU energy to speed

The last design ignored tilt direction and used projected magnitude as an accelerator:

```cpp
float projectedG = sqrtf(imu.accel[0] * imu.accel[0] +
                         imu.accel[1] * imu.accel[1]);
float amount = constrain(projectedG / 0.8f, 0.0f, 1.0f);
float targetSpeed = 35.0f + amount * (240.0f - 35.0f);

float blend = 1.0f - expf(-dt / 0.20f);
smoothedSpeed += (targetSpeed - smoothedSpeed) * blend;
playheadAngle = wrapRadians(playheadAngle + radians(smoothedSpeed) * dt);
```

Time-based exponential smoothing makes response nearly independent of frame rate. This is preferable to a fixed per-frame coefficient when display load can vary.

### Detecting crossings

Do not test whether the ray is merely close to a node because that can trigger repeatedly. Retain the previous angle and test whether the forward sweep interval crossed the node’s angle, including wrap at one full rotation. Mark the note active and schedule its release timestamp. Service scheduled releases every loop without blocking:

```cpp
if (crossedForward(previousAngle, currentAngle, node.angle)) {
  enqueueRadarNote(node.index, node.midiNote, true, 100);
  node.noteOffAtMs = millis() + 120;
  node.noteActive = true;
}

if (node.noteActive && (int32_t)(millis() - node.noteOffAtMs) >= 0) {
  enqueueRadarNote(node.index, node.midiNote, false, 0);
  node.noteActive = false;
}
```

The key lesson is separation: simulation decides that an event happened; the UI queue records it; the composition root sends it. A future student page can follow the same route without coupling physics directly to UDP.

### Reintroducing it as a student page

A new ninth page would require these bounded edits:

1. Change `kControlPageCount` from 8 to 9.
2. Add a touch target only if the page needs direct interaction.
3. Add compact fixed-size page state in `UserInterface.h`.
4. Add page-specific update functions that immediately return when hidden.
5. Add drawing to the final branch of `drawCurrentPage()`.
6. Add hit testing and gesture lifecycle handling.
7. Add a `UiEventType` for output-only events or a `MessageType` for remotely controlled state.
8. Route local events through `firmware.ino`.
9. Add OSC output and, only if pedagogically useful, BLE output.
10. Update the protocol and run the complete verification checklist.

Do not copy the retired endpoint back into the active protocol accidentally. A new class may choose a different concept and address.

## Recipe for a new student page

The lowest-risk first extension is an output-only page with fixed-size state. For example, a four-zone tilt instrument can draw four large quadrants and emit a zone event when the gravity vector crosses a boundary.

### Design first

Write a short contract before writing code:

- What physical action changes the page
- What the screen shows before, during, and after that action
- What exact message is sent
- Whether it is a state value or a one-shot event
- Whether the computer can update it remotely
- Whether BLE adds educational value or only duplicate work
- What should happen while the page is hidden

### Add state without heap churn

Prefer a fixed structure:

```cpp
struct StudentPageState {
  float value = 0.0f;
  uint8_t zone = 0;
  bool dirty = true;
  uint32_t lastUpdateUs = 0;
};
```

Avoid `new`, `String` concatenation, or growing containers in the animation loop. Fixed arrays made the ball, particle, pendulum, and former radar pages predictable.

### Keep update draw and transport separate

Use three layers:

```text
updateStudentPage(imu, audio, dt)  changes state and enqueues events
drawStudentPage()                  renders current state into the page canvas
handleUiEvents()                   maps events to OSC or BLE
```

This keeps a simulation testable and preserves the no-echo policy.

### Choose a message shape

For continuously varying data, send float32 values at a stated maximum rate. For collisions, crossings, or gestures, send one-shot integer `1` plus any identity or strength needed by the host. Avoid sending a constant stream of repeated trigger values while contact persists.

Use stable indices when multiple objects may be created and deleted. Stable identity is much easier to route in Max than a list that renumbers itself.

### Decide page lifecycle

The default policy is:

- input and drawing are active only while visible
- physics pauses while hidden
- retained state reappears on return
- no hidden backlog is simulated
- sensor services continue globally
- outbound state streams stop unless they are explicitly global sensors

The spectrum page is stricter: remote input and FFT processing are also disabled while hidden. State this kind of exception in the protocol.

## Verification checklist

Run this list after every new page or protocol change.

### Build integrity

- `./scripts/build.sh` completes without warnings introduced by the change.
- Program and RAM figures are recorded and remain comfortably below limits.
- A search for removed endpoint or page names finds only intentional historical documentation.
- `PROTOCOL.md`, `FIRMWARE_SPEC.md`, and the interface count agree.

### Device boot

- Serial reports UI touch, IMU, and audio as ready.
- Canvas allocation succeeds and reports available PSRAM.
- Existing saved Wi-Fi and device identity survive a normal flash.

### Display and touch

- All eight or newly increased page marks can be reached.
- The bottom button works near the rounded edge.
- Every visual control’s touch area feels aligned.
- No previous-page pixels remain after page changes.
- Fast vertical and horizontal motion show no trails or conspicuous white gaps.
- Touch release is always observed for buttons and notes.
- Exiting Settings leaves app touch responsive and does not activate the control underneath.

### Wi-Fi and OSC

- Cog opens a fully redrawn settings screen from every performance page.
- Network scan can be cancelled or allowed to time out.
- A scan on an already configured board finds nearby networks and reconnects after exit.
- Password show and hide works.
- Every letter/symbol mode is reachable and the wide keys register near their edges.
- Target address and both ports save and take effect.
- Device name updates hostname, OSC root, and BLE name after restart.
- OSC reaches the correct computer and incoming controls update the display.
- Remote values do not echo.

### BLE

- Device advertises its configured name and custom service.
- Controls and IMU notifications can be subscribed.
- Full IMU works at MTU 64 or greater.
- Three fragments reconstruct one frame at default MTU.
- The IMU characteristic is silent while the saved raw-output toggle is off.
- Floats decode little-endian without loss.
- Disabling BLE disconnects peers and stops advertising.

### Sensors and simulation

- Logical IMU X points right, Y points up, and Z points out of the screen.
- Raw OSC and BLE signs agree.
- Derived visual gravity moves in the intended screen direction.
- Yaw drift is treated as expected, not as a regression.
- Movement trigger fires once per onset rather than every frame.
- Audio capture does not build increasing latency behind display work.
- A hidden simulation does not jump forward when reopened.

## Known limitations and deliberate omissions

- CST820 interaction is single touch. Remote keyboard states can be polyphonic, but local fingers cannot.
- QMI8658 has no magnetometer, so there is no absolute compass heading and yaw drifts.
- Wi-Fi setup supports ordinary scanned networks but not a browser portal. Enterprise authentication is not implemented.
- Credentials are stored locally in Preferences without application-level encryption.
- IMU calibration code exists but the settings control is unfinished.
- Raw IMU output defaults off and is controlled locally; there is no protocol-version-1 remote toggle.
- There is no on-device factory-reset control in the present settings flow.
- Spectrum is OSC-only and active only on page 8. Sending 32 floats continuously over BLE was not judged necessary.
- Physics collision and pendulum messages are OSC-only.
- `ConfigPortal.*` is legacy unused code and can be removed after confirming no planned return to hotspot setup.
- There are no automated hardware tests. Visual, touch, radio, and axis validation require the physical board.
- The firmware is currently one Arduino loop rather than multiple FreeRTOS tasks. That simplicity is useful until a measured timing problem requires a more complex design.

## Recommended next steps

1. Treat the newly removed ninth slot as the class extension point.
2. Add an on-device IMU calibration action and a visible success or failure result.
3. Create a small Max reference patch that discovers the device manually, routes all current OSC messages, and shows the no-echo behaviour.
4. Add a BLE reference client or Node for Max decoder that handles both full and fragmented IMU frames.
5. Remove the unused hotspot portal after a final decision that it will not return.
6. Consider a lightweight protocol version query over OSC if classroom host patches will need to support multiple firmware generations.
7. Photograph and record a short acceptance test for the exact V2 unit used in class.

## Quick context block for a future development session

Use the following short block when opening a fresh development context:

```text
Continue the Classroom Controller firmware in this repository.
Target only Waveshare ESP32-S3-Touch-AMOLED-1.8 V2, 368x448,
CO5300 plus CST820. Read DEVELOPER_HANDOVER.md, FIRMWARE_SPEC.md,
and PROTOCOL.md before editing. The verified baseline has eight pages:
keyboard, four buttons, XY, four faders, balls, pendulums,
microphone particles, and 32-band spectrum capture. Radar was intentionally removed
to leave a student extension slot. Preserve the black/remote-colour
background, white positive-space visual language, 25-pixel major shapes,
50 Hz interaction cadence, CO5300 aligned partial redraws, screen-frame
IMU convention, locally gated raw IMU output defaulting off, stable indices,
and strict remote no-echo behaviour.
Build with ./scripts/build.sh and flash only to an explicitly detected port.
```

## External references

- [Waveshare V2 board documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.8)
- [Waveshare downloads and schematics](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.8/Resources-And-Documents)
- [Waveshare product page](https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm)
- [Espressif Arduino ESP32 getting started](https://docs.espressif.com/projects/arduino-esp32/en/latest/getting_started.html)
- [Espressif Arduino ESP32 installation](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- [Espressif Wi-Fi API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html)
- [Espressif Arduino BLE overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ble.html)
- [Espressif ESP32-S3 BLE guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/ble/index.html)
- [Arduino CLI getting started](https://arduino.github.io/arduino-cli/0.30/getting-started/)
- [Arduino CLI upload reference](https://arduino.github.io/arduino-cli/dev/commands/arduino-cli_upload/)
- [Open Sound Control 1.0 specification](https://opensoundcontrol.stanford.edu/spec-1_0.html)
- [Max udpreceive reference](https://docs.cycling74.com/reference/udpreceive/)
- [Max udpsend reference](https://docs.cycling74.com/reference/udpsend/)

## Handover conclusion

The project is in a strong teaching-ready state: the hardware-specific foundations are solved, the visual style is coherent, OSC and BLE are working, and enough program and RAM headroom remains for meaningful student experiments. The safest continuation strategy is to treat existing pages and transports as stable examples, add one concept at a time through the event router, and protect the frame-rate, alignment, and no-echo decisions that were learned through physical testing.
