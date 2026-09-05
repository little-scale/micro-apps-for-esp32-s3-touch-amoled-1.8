# ESP32-S3 Classroom Controller Firmware Specification

Status: working specification  
Hardware: Waveshare ESP32-S3-Touch-AMOLED-1.8 V2, 368 x 448  
Display/touch: CO5300 / CST820

## 1. Purpose

The device is a self-contained classroom control surface for learning about touch input, six-axis motion sensing, OSC over Wi-Fi, and raw Bluetooth Low Energy communication. It is not intended to teach firmware programming.

Students may configure and take devices home. The firmware therefore requires no classroom-only infrastructure and no reflashing when a device changes networks.

Computer-side Max, Ableton, MIDI conversion, and teaching patches are outside the firmware deliverables. This document defines the behaviour they can rely on.

## 2. Main interface

The normal interface uses a black background and contains:

- A minimal top strip with settings/Wi-Fi, BLE, raw IMU-output, and battery state
- Page 1 contains a velocity-sensitive one-octave musical keyboard with local octave controls
- Page 2 contains four blank momentary buttons
- Page 3 contains one XY controller
- Page 4 contains four vertical faders
- Page 5 contains up to eight IMU-driven, mutually colliding balls inside a square arena
- Page 6 contains up to four touch-drawn, IMU-gravity pendulums
- Page 7 contains a microphone-energy particle field entering from the lower-left microphone corner and steered by IMU tilt
- Page 8 contains a 32-band multislider and hold-to-capture microphone spectrum control
- One blank shared bottom control advances to the next page

The page control is local only. Eight small square marks indicate the current page. Each page uses the bold design language established by the XY controller: flat white fields, background-coloured 25-pixel position marks, square geometry, and no labels.

### 2.1 XY controller

- X is normalized from `0.0` at the left to `1.0` at the right.
- Y is normalized from `0.0` at the bottom to `1.0` at the top.
- A value is sent immediately on touch-down.
- Updated values are sent repeatedly while the finger moves, limited to 50 Hz.
- Local touch coordinates use adaptive smoothing and a small deadband to suppress sensor jitter while retaining fast intentional movement.
- A final value is sent when the touch ends if the control changed during the gesture.
- Values are clamped to the inclusive range `0.0...1.0`.
- The pad is a solid white field without a separate frame.
- A 25-pixel-wide cross in the current interface background colour follows the XY position.
- The cross intersection is a 25-pixel white square while idle and changes to the interface background colour while touched.

### 2.2 Faders

- The four faders are addressed as `fader0` through `fader3`, from left to right.
- Each fader is normalized from `0.0` at the bottom to `1.0` at the top.
- Each fader is a pure vertical fill: solid white from the bottom to its current value and the interface background colour above it.
- There is no frame, track, handle, or position marker.
- They follow the same continuous touch/update behaviour as the XY controller.
- A held gesture may move horizontally across the complete fader page. Entering another column immediately transfers control to that fader and sends its new value, matching a multislider-style interaction.

### 2.3 Buttons

- The four buttons are addressed as `button0` through `button3`, ordered top-left, top-right, bottom-left, bottom-right.
- Touch-down sends integer `1` and touch release sends integer `0`.
- A release message is always produced after a successful press, including when the finger slides outside the button.
- Each button is square, with a 25-pixel solid-white border and a background-coloured centre.
- Pressing a button makes the complete square solid white.

### 2.4 Bouncing balls

- The balls move inside a square arena with 25-pixel white walls and a background-coloured interior.
- The arena starts empty. Tapping unoccupied arena space adds a ball at that position; tapping a ball removes it.
- Holding anywhere in the arena for 2.5 seconds removes all balls.
- Up to eight balls may be active. Their stable indices are 0–7; removing a ball does not renumber the others, and a new ball reuses the lowest free index.
- The negative of screen-frame accelerometer X and Y estimates downhill gravity and acts as a continuous force on every ball. Velocity is damped, capped, and retained between frames. Raw OSC/BLE IMU signs are unchanged.
- Balls use equal-mass collision response and rebound from one another as well as the arena walls.
- Wall and ball rebounds use the saved bounciness value, 82 percent by default.
- Each new contact produces one OSC event carrying normalized impact strength from `0.0` to `1.0`; sustained contact is not repeatedly emitted.
- `/<device>/collision/wall` carries the ball index, wall index, and impact. Wall indices are 0 left, 1 right, 2 top, and 3 bottom.
- `/<device>/collision/ball` carries both colliding ball indices and impact.
- Collision events are output-only and are not sent over BLE.
- Physics pauses while another interface page is visible and resumes without a time jump when page 5 returns.

### 2.5 Pendulum

- Touch-down places pivot A; dragging places weighted point B and defines the rigid arm length and initial angle.
- After creation, dragging point B changes that arm's length and angle while keeping pivot A fixed. Tapping pivot A deletes that pendulum. Arm length is limited along its drawn direction and may extend to 260 pixels, allowing a downward pendulum to use nearly the full arena height.
- Releasing point B starts that simulation with zero angular velocity. Drawing in empty space creates another pendulum, up to four. Stable indices are 0–3; deletion does not renumber the survivors, and creation reuses the lowest empty slot.
- Holding from empty space for 2.5 seconds cancels the provisional new pendulum and resets every existing bob to its current gravity-defined equilibrium angle with zero angular velocity. Pivot locations, arm lengths and stable indices are preserved.
- Pivot A is a hollow 25-pixel square, point B is a solid 25-pixel circular bob, and the arm is a crisp five-pixel white line.
- The projected screen-plane gravity vector drives the pendulum. The saved Gravity setting controls its strength; Bounce remains specific to the ball page.
- The simulation uses fixed gentle angular damping. It pauses while hidden and resumes without a time jump.
- `/<device>/pendulumN` transmits normalized A/B coordinates, angle in degrees, and angular velocity in degrees per second at up to 25 Hz per active pendulum.
- `/<device>/pendulumN/active` sends `1` when slot N is created and `0` when it is deleted.
- A centre ping is emitted when the bob crosses its instantaneous gravitational equilibrium or nadir. Left and right pings are emitted at the respective turning points (extrema).
- Pendulum state and pings are output-only OSC messages.

### 2.6 Microphone particles

- The onboard analog microphone is captured through the ES8311 codec at 16 kHz and 16-bit resolution.
- Short-window RMS is calculated after removing DC offset. An adaptive ambient-noise floor, fast attack, slower release and a compressive curve produce one normalized `0.0...1.0` energy value.
- White square particles enter from the lower-left microphone corner. Their spawn density follows the energy value; their individual direction, speed, size and extended lifetime vary within bounded ranges.
- Screen-plane IMU tilt continuously accelerates and curves the particles using the same verified axis orientation as the bouncing-ball page. Microphone energy affects density, not steering.
- A 5-pixel white boundary makes the whole particle arena explicit. A particle disappears on its first wall impact and emits one OSC-only `/<device>/particle/wall` ping containing the wall number and its normalized size. Walls are 0 left, 1 right, 2 top, and 3 bottom; size is 0 for the smallest rendered particle and 1 for the largest.
- Raw audio is not stored or transmitted.
- `/<device>/mic0` sends one normalized `float32` energy value over OSC at 25 Hz. BLE type `0x22` carries the identical 32-bit float.
- Audio analysis and network output continue on every interface page; particle animation runs only while page 7 is visible.

### 2.7 Musical keyboard

- Eight equal white keys and five 25-pixel black keys form a complete C-to-C octave, initially MIDI note 60 (C4) through 72 (C5).
- Two large buttons beneath the shortened keybed shift the whole keyboard down or up by 12 semitones. The range is capped at base notes 0 and 108 so all thirteen displayed pitches remain valid MIDI notes.
- The page has no note labels. White positive space, background-coloured divisions and inverted pressed states match the other performance pages.
- The physical controller is monophonic because the CST820 reports one touch point.
- Touch-down sends note-on. Sliding into another key sends note-off for the previous key followed by note-on for the new key. Release sends note-off.
- Touch height sets velocity from 20 at the top to 127 at the bottom. White and black keys each use their own visible height for this mapping.
- OSC uses `/<device>/note` with `int32 MIDI note, int32 state, int32 velocity`. BLE uses message type `0x05`, the MIDI note as its index, then one-byte state and velocity fields.
- Incoming OSC/BLE note states may light multiple keys and obey the normal no-echo rule.

### 2.8 Spectrum capture and multislider

- Thirty-two narrow, normalized vertical sliders form one editable bank. Horizontal drags interpolate across any bands skipped between touch samples.
- Manual editing sends all 32 values together over OSC at up to 50 Hz and once more on release.
- Holding the large capture area beneath the bank enables a 256-sample Hann-windowed FFT. The 16 kHz input is represented as 32 linear bands from low to high frequency, approximately 250 Hz wide.
- Each latest FFT frame replaces the previous one while held. Magnitudes use a fixed linear full-scale reference: there is no per-frame peak normalization, nonlinear boost, noise gate, or temporal averaging. Releasing freezes that final spectral frame into the sliders and sends it once.
- `/<device>/spectrum0` carries 32 `float32` values in ascending frequency order. Values retain absolute level differences and clip only at the full-scale slider limit of 1.0.
- The spectrum control is OSC-only. FFT processing, spectrum input and spectrum output are disabled whenever page 8 is not visible. The previously captured values remain stored locally.

### 2.9 Page control

- The full-width bottom control advances `keyboard -> buttons -> XY -> faders -> balls -> pendulum -> microphone -> spectrum -> keyboard` on touch-down.
- It does not send OSC or BLE data and does not alter any control value.
- Remote updates continue to update hidden page state and appear when that page is next shown, except spectrum updates, which are accepted only while page 8 is visible.

## 3. Device identity

The user configures one canonical device name. The sanitized name is inherited by every transport:

- Wi-Fi hostname: `<device>.local`
- BLE advertised name: `<device>`
- OSC root: `/<device>/`

The canonical name uses lowercase ASCII letters, digits, and hyphens. Spaces and unsupported characters are converted to hyphens. A name cannot contain `/` because it forms part of the OSC address.

A fresh device receives a stable hardware-derived default in the form `device-XXXX`, where `XXXX` is the final four hexadecimal characters derived from its unique chip identity. The on-device `DEVICE` screen can replace it. Saving a new name restarts the board so Wi-Fi/mDNS, BLE, and OSC adopt the same identity together.

## 4. OSC over Wi-Fi

OSC uses UDP on the local network.

### 4.1 Network configuration

Each saved Wi-Fi configuration contains:

- SSID
- Password
- OSC target IPv4 address or hostname
- OSC target UDP port, default `9000`
- Device receive UDP port, default `9001`

Automatic OSC target discovery is not required.

### 4.2 Device-to-computer messages

OSC arguments are separated from the OSC address by spaces below; they are not part of the address string.

| Address | OSC arguments | Behaviour |
| --- | --- | --- |
| `/<device>/xy0` | `float32 x`, `float32 y` | Normalized XY position |
| `/<device>/faderN` | `float32 value` | Normalized vertical fader `N`, where `N` is 0–3 |
| `/<device>/buttonN` | `int32 state` | Button `N` press/release, `1` or `0`, where `N` is 0–3 |
| `/<device>/note` | `int32 MIDI note`, `int32 state`, `int32 velocity` | MIDI note 0–127; press uses velocity 20–127, release uses state/velocity 0 |
| `/<device>/spectrum0` | 32 fixed-scale linear `float32` values | Complete low-to-high spectrum/multislider bank; page 8 only |
| `/<device>/collision/wall` | `int32 ball`, `int32 wall`, `float32 impact` | Indexed ball-to-wall collision; wall is 0 left, 1 right, 2 top, or 3 bottom |
| `/<device>/collision/ball` | `int32 ballA`, `int32 ballB`, `float32 impact` | Collision between indexed balls |
| `/<device>/movement` | `int32 1` | Movement onset caused by a shake, tap, or quick rotation |
| `/<device>/mic0` | one `float32` | Normalized microphone energy/density from 0 to 1 |
| `/<device>/particle/wall` | `int32 wall`, `float32 size` | Particle wall-impact ping; wall is 0 left, 1 right, 2 top, or 3 bottom; size is normalized 0 to 1 |
| `/<device>/pendulumN` | six `float32`: `ax ay bx by angle angularVelocity` | Indexed pendulum position and motion state, N = 0–3 |
| `/<device>/pendulumN/active` | `int32 state` | Pendulum N created (`1`) or deleted (`0`) |
| `/<device>/pendulumN/centre` | `int32 1` | Indexed equilibrium/nadir crossing ping |
| `/<device>/pendulumN/left` | `int32 1` | Indexed left turning-point ping |
| `/<device>/pendulumN/right` | `int32 1` | Indexed right turning-point ping |
| `/<device>/imu0` | nine `float32` values | One synchronized IMU/orientation frame |

`/<device>/imu0` arguments are ordered as:

```text
ax ay az gx gy gz pitch roll yaw
```

One `imu0` message is sent per sample interval. The default rate is 25 Hz; 50 Hz is selectable in configuration.
Periodic raw IMU transmission defaults to off. Tapping the three-axis badge immediately beside
BLE toggles OSC and BLE IMU output and saves the selection. Off suppresses `imu0` and BLE types
`0x10` through `0x13`, but does not stop sensor sampling, local physics, orientation-aware pages,
motion wake, or movement-trigger output.
The status colours are categorical: connected settings/Wi-Fi is green, enabled BLE is amber,
and enabled raw IMU output is pink. Disabled BLE and IMU output are grey.

The movement detector compares consecutive acceleration and angular-velocity samples. A
change of at least `0.28 g` in the acceleration vector or `35 degrees/second` in the gyro
vector emits one `/<device>/movement 1` message over OSC and one BLE movement packet.
There is no release/zero message. A 250 ms retrigger interval prevents a single onset from
flooding either transport while still allowing repeated rhythmic shakes.

Microphone energy is transmitted at 25 Hz. It is a smoothed, noise-gated short-window RMS
measure intended for loudness, breath and particle-density control rather than a raw audio
waveform. Raw samples never leave the device.

### 4.3 Computer-to-device messages

The computer may send the same control addresses and argument types to the device receive port:

```text
/<device>/xy0 <float32 x> <float32 y>
/<device>/faderN <float32 value>   # N = 0...3
/<device>/buttonN <int32 state>    # N = 0...3
/<device>/note <int32 MIDI note> <int32 state> <int32 velocity>
/<device>/spectrum0 <32 float32 values, low to high>
```

An accepted incoming value updates the display immediately but is not transmitted back over OSC or BLE. This is the fundamental no-echo rule. `spectrum0` is accepted only while page 8 is visible.

While the user is actively touching the XY controller, fader, or spectrum bank, local input has priority. Incoming updates for that control are ignored until touch release.

### 4.4 Background colour

The computer may change the complete interface background immediately:

```text
/<device>/background <int32 red> <int32 green> <int32 blue>
```

- Components are clamped to `0...255`.
- The default is `0 0 0`.
- The message is accepted from OSC or BLE.
- It updates the display without being echoed.
- It wakes a dimmed display and resets the inactivity timer.
- Foreground controls and indicators automatically choose a contrasting light or dark treatment.

## 5. IMU behaviour

### 5.1 Coordinate frame and units

The logical device frame is:

- Positive X: toward the right edge of the display
- Positive Y: toward the top edge of the display
- Positive Z: outward through the front of the display

Values are:

- Acceleration X/Y/Z in `g`, including gravity
- Angular velocity X/Y/Z in degrees per second
- Pitch, roll, and yaw in degrees

Pitch, roll, and yaw are calculated by a six-axis orientation filter. Because the board has no magnetometer, yaw is relative to the calibrated starting direction and will drift over time.

### 5.2 Calibration and rate

The planned on-device motion settings screen provides:

- Calibrate/zero while the device is stationary
- 25 Hz output mode, default
- 50 Hz output mode

Calibration stores gyroscope bias and the current orientation reference.

## 6. Bluetooth Low Energy

BLE uses a custom binary GATT service. BLE MIDI is not used. All floating-point values remain IEEE-754 binary32 so no numerical precision is intentionally lost.

### 6.1 GATT UUIDs

| Purpose | UUID | Properties |
| --- | --- | --- |
| Classroom Controller service | `89689de0-1604-42c1-85ba-e286075c4488` | Primary service |
| Control messages | `c2bbf0cb-9029-4925-abbc-3dd86492fc06` | Notify, Write Without Response |
| IMU stream | `2e5abe1b-4f44-429e-8b47-1526afbe343d` | Notify |
| Device information | `fe9c3dcb-451c-4512-b845-a611ffa3280f` | Read |

The Control characteristic notifies device-originated control changes. The computer writes the same packet format to update device state. Incoming writes are never notified back.

### 6.2 Binary packet header

All multi-byte values use little-endian byte order. Structures contain no implicit padding.

| Offset | Size | Type | Meaning |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8` | Protocol version, initially `1` |
| 1 | 1 | `uint8` | Message type |
| 2 | 1 | `uint8` | Control index |
| 3 | 1 | `uint8` | Flags, initially `0` |
| 4 | 2 | `uint16` | Sequence number |
| 6 | variable | bytes | Message payload |

Sequence numbers wrap naturally from `65535` to `0`. A new IMU sample receives a new sequence number. Fallback fragments belonging to the same sample share a sequence number.

### 6.3 Message types

| Type | Meaning | Payload |
| ---: | --- | --- |
| `0x01` | XY | two `float32`: X, Y |
| `0x02` | Fader | one `float32` |
| `0x03` | Toggle | one `uint8`: `0` or `1` |
| `0x04` | Trigger button | one `uint8`: `0` or `1` |
| `0x05` | Keyboard note | index is MIDI note 0–127; `uint8 state`, then `uint8 velocity` |
| `0x10` | Complete IMU frame | nine `float32` values in OSC `imu0` order |
| `0x11` | Acceleration fragment | three `float32`: X, Y, Z |
| `0x12` | Gyroscope fragment | three `float32`: X, Y, Z |
| `0x13` | Orientation fragment | three `float32`: pitch, roll, yaw |
| `0x20` | Background colour | three `uint8`: red, green, blue |
| `0x21` | Movement trigger | one `uint8`: always `1` |
| `0x22` | Microphone energy | one normalized `float32` |

The control index is `0` for XY and microphone energy, `0...3` for faders and buttons,
MIDI note 0–127 for keyboard packets, and ignored for IMU/background packets. Toggle
type `0x03` remains reserved for protocol compatibility but is not used by this interface.

The preferred complete IMU packet is 42 bytes and therefore requires an ATT MTU of at least 45. The device requests an MTU of at least 64. If the connection cannot provide this, it sends message types `0x11`, `0x12`, and `0x13` as three notifications with the same sequence number. This fallback preserves every 32-bit float.

### 6.4 BLE interaction

- Tap the BLE icon to enable or disable BLE.
- Disabled is grey.
- Advertising is amber.
- Connected is green.
- Activity may be indicated by a brief brightness change.
- Hold the BLE icon to clear stored bonds and begin advertising again.
- No separate BLE setup page is required.
- The advertised BLE name is inherited from the canonical device name.

## 7. On-device Wi-Fi configuration

Tapping the cog icon opens a full-screen `NETWORK` / `OSC` / `DEVICE` / `PHYSICS` settings menu. A device with no saved network opens the network picker automatically. The device never creates a setup hotspot.

- Networks are scanned asynchronously, deduplicated and ordered by signal strength.
- Five networks are shown per page, with signal and security indicators.
- The user selects a network and enters its password with an on-screen keyboard.
- The keyboard provides lowercase, uppercase, digits, symbols, space and delete.
- A `SHOW`/`HIDE` control reveals or masks the entered password; each newly selected network starts masked.
- A successful connection is saved and returns to the controller automatically.
- A failed connection returns to password entry with a red password frame.
- Scanning can be repeated without leaving setup.
- The OSC screen edits a numeric IPv4 target, output port and device receive port.
- Saving valid OSC values persists and applies all three fields immediately.
- The DEVICE screen edits the canonical lowercase name with letters, digits and hyphens.
- Saving a valid device name persists it and restarts the device.
- The PHYSICS screen adjusts and persists gravity from `0.5` to `4.0` in `0.1` steps and collision bounciness from `0` to `100` percent in five-percent steps. Defaults are `2.2` and `82` percent.

Setup closes silently after 150 seconds without interaction. Interaction resets the timer. On timeout:

- Unsaved changes are discarded.
- Previously saved settings remain unchanged.
- The device returns to the main controller interface.

The current on-device settings cover SSID/password, OSC routing, device identity and ball physics. IMU rate/calibration, dim timing and factory reset are retained as saved/default values until their additional on-device screens are implemented.

## 8. Remote-state and transport rules

- Touch-originated changes are transmitted over every currently enabled transport.
- Computer-originated changes are applied locally and never retransmitted.
- OSC and BLE use the same normalized values, indices, and IMU ordering.
- Buttons retain press/release semantics in both directions.
- The most recently accepted value is the displayed value.
- No automatic Wi-Fi/BLE failover is performed; students can observe each transport independently.

## 9. Display power behaviour

- The AMOLED dims after its saved inactivity interval.
- Touch, significant motion, an incoming control update, or a background-colour command wakes it immediately.
- Dimming does not stop OSC or BLE communication.
- The normal background is black to minimize AMOLED power use and wear.

## 10. Firmware deliverables

- Flashable firmware for the V2 board
- This protocol and behaviour specification
- A concise flashing/recovery guide
- An OSC address reference
- A BLE UUID and binary-packet reference

No Max patch, Max for Live device, Ableton Set, BLE client, or MIDI conversion software is included.

## 11. Confirmed assumptions

- Control indices are zero-based.
- Fader OSC address is `/<device>/fader0`.
- OSC uses float32 for normalized and IMU values and int32 for discrete controls.
- Acceleration is expressed in `g`; angular velocity and orientation are expressed in degrees.
- A 50 Hz limit is used for continuous touch messages unless the display update rate is lower.
