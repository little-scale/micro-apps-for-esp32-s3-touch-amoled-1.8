# OSC and BLE reference

Replace `<device>` with the configured device name. The same name is used for the Wi-Fi hostname, BLE advertisement and OSC root.

## OSC over Wi-Fi

OSC uses UDP. The device defaults to computer port `9000` and device receive port `9001`.

### Device to computer

| OSC address | Arguments | Meaning |
| --- | --- | --- |
| `/<device>/xy0` | `float32 x, float32 y` | X: left 0 to right 1; Y: bottom 0 to top 1 |
| `/<device>/faderN` | `float32 value` | Fader N (0–3): bottom 0 to top 1 |
| `/<device>/buttonN` | `int32 state` | Button N (0–3): press 1, release 0 |
| `/<device>/note` | `int32 MIDI note, int32 state, int32 velocity` | Note 0–127; press state is 1 with velocity 20–127, release is `state 0, velocity 0` |
| `/<device>/spectrum0` | 32 `float32` values | Complete low-to-high multislider/FFT bank on a fixed linear full-scale reference |
| `/<device>/collision/wall` | `int32 ball, int32 wall, float32 impact` | Ball-to-wall collision; walls: 0 left, 1 right, 2 top, 3 bottom |
| `/<device>/collision/ball` | `int32 ballA, int32 ballB, float32 impact` | Collision between two indexed balls |
| `/<device>/movement` | `int32 1` | Shake/flick onset trigger; no corresponding release or zero message |
| `/<device>/mic0` | `float32 energy` | Smoothed, noise-gated microphone energy from 0 to 1 |
| `/<device>/particle/wall` | `int32 wall, float32 size` | Particle wall-impact ping; walls: 0 left, 1 right, 2 top, 3 bottom; size is normalized 0 to 1 |
| `/<device>/pendulumN` | six `float32` values | Pendulum N (0–3): `ax ay bx by angle angularVelocity` |
| `/<device>/pendulumN/active` | `int32 state` | Pendulum N created (`1`) or deleted (`0`) |
| `/<device>/pendulumN/centre` | `int32 1` | Pendulum N crosses its instantaneous gravitational equilibrium point |
| `/<device>/pendulumN/left` | `int32 1` | Pendulum N reaches its left turning point |
| `/<device>/pendulumN/right` | `int32 1` | Pendulum N reaches its right turning point |
| `/<device>/imu0` | nine `float32` values | `ax ay az gx gy gz pitch roll yaw` |

Ball indices are stable slots from 0–7. Removing one ball does not renumber the others;
the next ball added reuses the lowest available index. Collision impact is normalized to
`0.0...1.0`. A sustained contact emits only its initial collision rather than repeating every
physics frame.

Up to four pendulums occupy stable slots 0–3. Removing one does not renumber the others;
the next pendulum reuses the lowest free slot. Pendulum A/B coordinates use the same normalized frame as the XY pad: left-to-right and
bottom-to-top are `0.0...1.0`. Angle is in degrees with `0` pointing toward the bottom of
the screen and positive angles toward the right; angular velocity is degrees per second.
The state message is limited to 25 Hz while drawing or simulating. Centre, left and right
pings contain only `1`, with no release/zero message. Creation/deletion is reported separately
by the `active` message. Pendulum output is currently OSC-only.

Microphone energy is derived from short-window RMS with adaptive ambient-noise removal,
fast attack and slower release. It is a normalized density/loudness measure rather than a
waveform sample. Raw audio is neither transmitted nor stored. `mic0` is sent at 25 Hz.
Each particle disappears when it reaches the particle arena boundary and emits one
OSC-only wall ping. Wall numbering matches the ball arena: 0 left, 1 right, 2 top,
and 3 bottom. The second argument is the particle's normalized size: 0 for the
smallest rendered particle and 1 for the largest.

The XY pad and faders transmit at no more than 50 Hz while moving. The keyboard is
monophonic for local touch and supports glissando: entering a new key sends note-off for
the previous key followed by note-on for the new one. Pressing nearer the top of a key
produces velocity 20 and pressing nearer its bottom produces velocity 127; black-key
velocity is scaled over the black key's own height. Octave changes are local and capped
to complete C-to-C ranges inside MIDI 0–127. IMU output is 25 Hz by default or
50 Hz when selected in setup.

Periodic `imu0` output is disabled by default. The three-axis badge beside BLE toggles it
locally and the choice persists across restarts. When disabled, neither OSC complete IMU
messages nor BLE complete/fragmented IMU notifications are sent. The QMI8658 remains active
for local physics, orientation-aware pages, display wake and `movement` onset messages.

The spectrum page contains 32 linearly spaced bands spanning approximately 0–8 kHz at
the microphone's 16 kHz sample rate. Manual edits send the complete bank in one OSC
message at no more than 50 Hz, plus a final message on release. Holding the capture area
runs a 256-sample Hann-windowed FFT and displays each latest fixed-scale linear spectrum frame;
releasing freezes and sends the final frame once. Spectrum OSC input, output and FFT
processing are disabled while this page is hidden. This bank is intentionally OSC-only.
FFT bands are not peak-normalized per frame: their height and transmitted value retain
level differences between quiet and loud sounds. Values clip only at full scale.

### Computer to device

The computer may send the XY, fader, button, note and spectrum address forms above with the same
types. The legacy two-integer note form (`note, state`) is also accepted. Fader/button
`N` must be in the range 0–3 and note must be 0–127. Remote note
states may be polyphonic even though the physical touch panel is single-touch. Collision
messages are output-only. The computer may also send:

| OSC address | Arguments | Meaning |
| --- | --- | --- |
| `/<device>/background` | `int32 red, int32 green, int32 blue` | Immediate display background, each 0–255 |

Accepted incoming control and colour messages update and wake the display. They are not sent back by OSC or BLE. While a control is being touched locally, remote updates for that control are ignored until release. Spectrum updates are accepted only while the spectrum page is visible.

## IMU units and frame

- Acceleration: `g`
- Angular velocity: degrees per second
- Pitch, roll and relative yaw: degrees
- Intended logical axes: +X toward screen-right, +Y toward screen-top, +Z out of the display

The initial firmware keeps the board-axis transform in one isolated function so any physical axis correction found during bring-up can be made without changing the protocol. Pitch and roll are gravity-corrected; yaw is integrated from the gyro and therefore drifts.

## BLE GATT

| Purpose | UUID | Properties |
| --- | --- | --- |
| Service | `89689de0-1604-42c1-85ba-e286075c4488` | Primary service |
| Controls | `c2bbf0cb-9029-4925-abbc-3dd86492fc06` | Notify, Write Without Response |
| IMU | `2e5abe1b-4f44-429e-8b47-1526afbe343d` | Notify |
| Information | `fe9c3dcb-451c-4512-b845-a611ffa3280f` | Read |

Device-originated control packets notify on the Controls characteristic. The computer writes the same format to that characteristic for remote changes. IMU packets notify on the IMU characteristic only while the local IMU-output badge is enabled; its saved default is off.

### Packet header

All multi-byte fields and floats are little-endian. Floats are IEEE-754 binary32.

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | Protocol version: `0x01` |
| 1 | 1 | Message type |
| 2 | 1 | Control index |
| 3 | 1 | Flags: currently `0x00` |
| 4 | 2 | Unsigned sequence number |
| 6 | variable | Payload |

### Message types

| Type | Payload |
| ---: | --- |
| `0x01` | XY: two `float32`, X then Y |
| `0x02` | Fader: one `float32` |
| `0x03` | Toggle: one `uint8`, 0 or 1 |
| `0x04` | Trigger: one `uint8`, 0 or 1 |
| `0x05` | Keyboard note: index is MIDI note 0–127; payload is `uint8 state, uint8 velocity` |
| `0x10` | Complete IMU: nine `float32` in OSC order |
| `0x11` | Accelerometer fragment: three `float32` |
| `0x12` | Gyroscope fragment: three `float32` |
| `0x13` | Orientation fragment: pitch, roll, yaw as three `float32` |
| `0x20` | Background: red, green, blue as three `uint8` |
| `0x21` | Movement trigger: one `uint8` with value `1` |
| `0x22` | Microphone energy: one normalized `float32` |

Index is 0 for XY, movement and microphone energy, 0–3 for faders and buttons, MIDI
note 0–127 for keyboard messages, and ignored for IMU/background. Note-on velocity is
20–127; note-off uses state and velocity 0. Incoming legacy one-byte note-state payloads
remain accepted. Type `0x03` is
reserved for compatibility and is not used by the current interface. Movement and
microphone packets are device-originated only.

The full IMU packet is 42 bytes. The device requests ATT MTU 64. If the negotiated MTU is below 64, each sample is sent as `0x11`, `0x12` and `0x13`; all three fragments share one sequence number. This fallback retains all nine 32-bit floats.

## No-echo rule

Only local touch changes originate control output. A state received from either computer transport changes the device UI but is never forwarded to the other transport or returned to its sender. IMU and microphone measurements are always device-originated and are independently transmitted on both available transports.
