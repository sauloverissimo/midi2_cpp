# [midi2_cpp](../..) | Device MIDI 2.0
## Arduino Nano ESP32

Full-spec USB MIDI 2.0 device example for the [**Arduino Nano ESP32**](https://docs.arduino.cc/hardware/nano-esp32/) (ESP32-S3-MINI-1 in the Arduino Nano form factor, see [pinout cheat sheet](https://docs.arduino.cc/tutorials/nano-esp32/cheat-sheet/)). Headless, full Showcase cycle of every MIDI 2.0 message category beyond MIDI 1.0, identical in behaviour to the [`esp32-s3-devkitc-usb-midi2`](../esp32-s3-devkitc-usb-midi2) recipe with the platform glue swapped for the Arduino Nano ESP32 LED footprint (single-channel `LED_BUILTIN` on D13 / GPIO48, no WS2812 RMT). Lives at `midi2_cpp/examples/arduino-nano-esp32-midi2/` and consumes the parent library directly (no vendoring).

> ⚠️ **TinyUSB override, not yet upstream.** The USB MIDI 2.0 device class driver this project depends on lives in TinyUSB [PR #3571](https://github.com/hathach/tinyusb/pull/3571), still under review. Until that PR merges into `hathach/tinyusb`, this build pulls a personal fork ([`sauloverissimo/tinyusb` branch `feat/midi2-device-host-driver`](https://github.com/sauloverissimo/tinyusb/tree/feat/midi2-device-host-driver)) at a pinned SHA into `idf/external/tinyusb`. Treat the build as **beta**: when the PR lands upstream the override goes away and this README will point at the official TinyUSB.

PID `0x4093` distinguishes this device from the other ESP32 recipes (`0x4090` ESP32-S3-DevKitC-1, `0x4091` ESP32-P4 device, `0x4092` ESP32-P4 bridge); a host enumerating all `midi2_cpp` examples on the same machine sees distinct endpoints.

## What this is

`arduino-nano-esp32-midi2` is the platform layer for a family of MIDI 2.0 devices on the Arduino Nano ESP32. It owns:

- ESP-IDF v5.4 board init (`usb_new_phy` with `USB_PHY_TARGET_INT`, FreeRTOS task scheduler)
- TinyUSB MIDI 2.0 device class wiring via the **PR #3571 fork**
- USB descriptors (VID `0xCAFE`, PID `0x4093`)
- The five [midi2_cpp](https://github.com/sauloverissimo/midi2_cpp) platform hooks already wired: `setWriteFn`, `feedRx`, `setNowFn`, `setMounted` / `setAltSetting`, `CI::setRngFn`
- Single-channel `LED_BUILTIN` indicator (D13 / GPIO48; override with `-DLED_BUILTIN_GPIO=<n>`). The on-board RGB LED (D14 / D15 / D16) is not driven; only `LED_BUILTIN` lights up while USB is mounted.

After `arduino_nano_esp32_midi2::init(midi, ci)`, the application sees only `m2device` and `m2ci`. It never touches `tud_*`, `esp_*`, or any USB symbol.

## Identification

| Field | Value |
|---|---|
| USB VID | `0xCAFE` |
| USB PID | `0x4093` |
| USB Manufacturer | `github.com/sauloverissimo` |
| USB Product | `ArduinoNanoESP32` |
| Endpoint Name | `ArduinoNanoESP32` |
| Product Instance ID | `ArduinoNanoESP32-showcase-0001` |
| MIDI-CI Manufacturer ID | `{0x7D, 0x00, 0x00}` (MIDI Association educational/non-commercial prefix) |
| MIDI-CI Family / Model / Version | `0x0001 / 0x0001 / 0x00010000` |

## Build

Requirements:

- **ESP-IDF v5.4 or newer** with the export script sourced (`. $IDF_PATH/export.sh`)
- An Arduino Nano ESP32 board (ABX00083), USB-C cable
- Internet on the first run (the bootstrap script clones the TinyUSB fork)

```bash
git clone https://github.com/sauloverissimo/midi2_cpp.git
cd midi2_cpp/examples/arduino-nano-esp32-midi2/idf
./scripts/fetch_tinyusb.sh         # one-off, ~36 MB clone of the fork at pinned SHA
. $IDF_PATH/export.sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

The Arduino Nano ESP32 typically exposes a single USB-C jack which is the native USB-OTG; before the firmware claims it the chip exposes the USB-Serial-JTAG ROM bootloader as `/dev/ttyACM0`. After flashing, the firmware reclaims the controller as USB MIDI 2.0 (VID:PID `0xCAFE:0x4093`).

If `idf.py flash` does not auto-reset the chip, force a watchdog reset:

```bash
python -m esptool --chip esp32s3 -p /dev/ttyACM0 --after watchdog_reset run
```

## Hardware

| Pin | Use |
|---|---|
| USB-C | Native USB-OTG (ESP32-S3-MINI-1), MIDI 2.0 device interface (and ROM bootloader before firmware claims it) |
| D13 / GPIO48 | `LED_BUILTIN` (yellow). Lit while USB is mounted. Override with `-DLED_BUILTIN_GPIO=<n>` |
| D14 / GPIO46, D15 / GPIO0, D16 / GPIO45 | RGB LED (active LOW). Not driven by this recipe |
| B1 (BOOT) | Hold during reset to enter the ESP32-S3 ROM bootloader |
| RESET | Reboot. Double-tap RESET to enter the Arduino DFU bootloader (only relevant when reverting to Arduino IDE flow) |

## Showcase

Identical to [`rp2040-midi2`](../rp2040-midi2/README.md#showcase) and [`esp32-s3-devkitc-usb-midi2`](../esp32-s3-devkitc-usb-midi2/README.md#showcase): 22 s scene cycle (A through J) covering every MIDI 2.0 message category beyond MIDI 1.0, plus JR Timestamp heartbeat, UMP Stream Discovery responder, MIDI-CI Discovery + Profile + 3 Properties + Process Inquiry. See those READMEs for the per-Scene table.

## What lives where

```
midi2_cpp/examples/arduino-nano-esp32-midi2/
├── README.md
├── board/                              board photos / pinout (TBD)
├── monitor/                            Microsoft MIDI Console captures (TBD)
└── idf/
    ├── CMakeLists.txt                  ESP-IDF project root
    ├── partitions.csv                  single-app, 8 MB flash
    ├── sdkconfig.defaults              target esp32s3, UART stdio, custom partition table
    ├── scripts/fetch_tinyusb.sh        bootstrap: clones TinyUSB fork into external/tinyusb
    ├── external/                       (gitignored, populated by fetch_tinyusb.sh)
    ├── components/tinyusb/
    │   ├── CMakeLists.txt              shim: registers the fork's selected sources
    │   └── usb_descriptors.c           PID 0x4093, Product "ArduinoNanoESP32"
    └── main/
        ├── CMakeLists.txt              idf_component_register, pulls midi2_cpp from ../../../../src
        ├── idf_component.yml           managed deps (none beyond ESP-IDF >=5.4)
        ├── tusb_config.h               1 group, 1 function block, FS USB-OTG
        ├── arduino_nano_esp32_midi2.h    public API of the platform glue
        ├── arduino_nano_esp32_midi2.cpp  USB-OTG PHY init + TinyUSB task + LED + hooks
        └── main.cpp                    showcase entry, full-spec MIDI 2.0 demo
```

## License

MIT, inherits the parent [`midi2_cpp` LICENSE](../../LICENSE). The TinyUSB fork (cloned on demand into `idf/external/tinyusb`) is MIT (upstream by hathach, fork by sauloverissimo carrying the MIDI 2.0 class drivers from the still-open [PR #3571](https://github.com/hathach/tinyusb/pull/3571)).
