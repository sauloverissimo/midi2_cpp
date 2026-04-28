# adafruit-feather-rp2040-host-midi2 — example for the [midi2_cpp](../..) library

Full-spec USB MIDI 2.0 **host** example for the **Adafruit Feather RP2040 USB Host**. Receives UMP from any MIDI 2.0 device plugged into the USB-A port (PIO-USB on GP16/GP17), routes it through `m2host`, and renders the device topology + live UMP stream on a 128x64 SSD1306 OLED over I2C1 (STEMMA QT).

> ⚠️ **TinyUSB host driver override — not yet upstream.** The MIDI 2.0 host class driver this example consumes lives in TinyUSB [PR #3571](https://github.com/hathach/tinyusb/pull/3571), still under review. Build pulls a personal fork ([`sauloverissimo/tinyusb` branch `feat/midi2-device-host-driver`](https://github.com/sauloverissimo/tinyusb/tree/feat/midi2-device-host-driver)) at a pinned SHA via CMake FetchContent. Same status as the device-side `rp2040-midi2` example: stable surface, beta override path.

## What this proves

- `m2host` from the parent library handles a real USB MIDI 2.0 host on bare metal RP2040
- The 5-hook platform contract (`setWriteFn` / `feedRx` / `setNowFn` / `setMounted` / `setRngFn`) ports cleanly to PIO-USB + TinyUSB host
- Multi-device addressing by `idx` — supports up to `MIDI2_CPP_HOST_MAX_DEVICES` (default 4) connected MIDI 2.0 devices simultaneously
- Auto-discovery on mount: the host fires UMP Stream Endpoint Discovery + MIDI-CI Discovery Inquiry without app code, then populates `DeviceIdentity` as replies arrive
- Identity (manufacturer / family / model / endpoint name / product instance ID) and live UMP traffic (NoteOn/Off / CC / Pitch Bend / Channel Pressure / Poly Pressure / Per-Note PB) decoded into human-readable lines on the OLED in real time

## Hardware

| Component | Use |
|---|---|
| Adafruit Feather RP2040 USB Host | RP2040 + USB-A host port via PIO-USB |
| USB-A → MIDI 2.0 device | the device under test (plug any UMP device, e.g. our own [`rp2040-midi2`](../rp2040-midi2)) |
| 128x64 SSD1306 OLED (I2C 0x3C) | live display, on STEMMA QT (I2C1 GP2/GP3) |
| USB-C cable to host PC | programming + power |

## Pinout

| Pin | Use |
|---|---|
| GP16 | USB-A D+ (PIO-USB) |
| GP17 | USB-A D- (PIO-USB) |
| GP18 | USB-A 5V power gate (driven high in `feather_host::init`) |
| GP2  | I2C1 SDA (STEMMA QT) |
| GP3  | I2C1 SCL (STEMMA QT) |
| GP0  | UART TX (debug print @ 115200) |
| GP1  | UART RX |

## Identification (host's own MIDI-CI Initiator identity)

| Field | Value |
|---|---|
| MIDI-CI Manufacturer ID | `{0x7D, 0x00, 0x00}` (educational/non-commercial prefix) |
| MIDI-CI Family / Model / Version | `0x0001 / 0x0001 / 0x00010000` |
| Host MUID | seeded on `begin()` from `pico_rand`'s `get_rand_32`, masked to 28 bits |

The host has its own identity because it is the **CI Initiator** — it sends Discovery Inquiry to plugged-in devices and stores their Discovery Reply MUIDs in `m2host::identity(idx).ciMuid`.

## Build

Requirements:

- **Pico SDK 2.x** with `PICO_SDK_PATH` exported
- **arm-none-eabi-gcc** toolchain
- **CMake 3.14+**
- Internet on the first `cmake -B build` (FetchContent pulls TinyUSB fork + Pico-PIO-USB)

```bash
git clone https://github.com/sauloverissimo/midi2_cpp.git
cd midi2_cpp/examples/adafruit-feather-rp2040-host-midi2
cmake -B build           # first run fetches deps (~5 MB TinyUSB + ~1 MB Pico-PIO-USB)
cmake --build build -j   # offline from here
```

Flash `build/adafruit-feather-rp2040-host-midi2-showcase.uf2` onto the Feather in BOOTSEL mode.

To use a local TinyUSB fork or local Pico-PIO-USB working copy:

```bash
cmake -B build \
  -DPICO_TINYUSB_PATH=/path/to/your/tinyusb \
  -DPICO_PIO_USB_PATH=/path/to/your/Pico-PIO-USB
```

## What you see on the OLED

**Splash → spinner → live**:

1. **Splash** (1.5 s on boot): title and credits
2. **Spinner** while no MIDI 2.0 device is plugged in
3. **Live** as soon as the first device mounts:

```
┌──────────────────────────┐
│ MIDI 2.0 Host            │  ← header
├──────────────────────────┤
│ [0] MIDI 2.0             │
│ name: rp2040-midi2       │  ← from Endpoint Name notification
│ [0] On C4 ch3 vC000      │  ← live UMP scrolling
│ [0] PNPB C4 ch3 8000F00C │
│ [0] CC74 ch0 80000000    │
│ [0] PB ch0 80000000      │
├──────────────────────────┤
│ n=42 devs=1              │  ← status bar
└──────────────────────────┘
```

UART debug on GP0 mirrors most events for headless monitoring.

## Validation suggestion

Plug the device-side example we ship — [`rp2040-midi2-showcase`](../rp2040-midi2) — into the Feather's USB-A port. The 22 s cycle of that device emits every category of UMP MIDI 2.0 brings beyond MIDI 1.0 (Flex Data, Per-Note expression, 16-bit velocity walk, 32-bit CC sweep, Program+Bank, RPN/NRPN/Relative, Note Attribute pitch_7_9, SysEx8, Delta Clockstamp, PE Notify, JR Heartbeat). Each of those should appear decoded on the host's OLED in real time, proving the round trip works end-to-end at the wire level.

## What lives where

```
midi2_cpp/
├── src/                            parent library (consumed via ../../src)
└── examples/adafruit-feather-rp2040-host-midi2/
    ├── CMakeLists.txt              FetchContent for TinyUSB PR #3571 + Pico-PIO-USB
    ├── pico_sdk_import.cmake
    ├── README.md
    ├── board/                      schematic / pinout images (TBD)
    └── src/
        ├── feather_host.{h,cpp}    PIO-USB + TinyUSB glue → m2host hooks
        ├── tusb_config.h           CFG_TUH_RPI_PIO_USB=1, CFG_TUH_MIDI2=1
        ├── display.{h,c}           SSD1306 driver (128x64, I2C, 6-line log + status)
        ├── font5x7.h               5x7 ASCII bitmap font
        └── main.cpp                showcase entry — m2host callbacks → display_log
```

The TinyUSB PR #3571 fork and Pico-PIO-USB are fetched at configure time into `build/_deps/` (gitignored). This example folder itself is ~1 MB.

## Hot-swap caveat

Per gingo.p4 production findings, the TinyUSB host stack on RP2040 can get stuck after a device is unplugged and not re-enumerate on re-plug. A 3 s watchdog that resets the host stack works around this; not implemented in this example for simplicity. If you observe the same, add the watchdog in `feather_host::task` based on a "no mount events for N seconds while devices were just present" timer.

## License

MIT — inherits the parent [`midi2_cpp` LICENSE](../../LICENSE). The TinyUSB fork (fetched on demand) and Pico-PIO-USB are MIT.
