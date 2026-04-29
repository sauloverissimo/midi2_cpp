# [midi2_cpp](../..) | Host MIDI 2.0
## ESP32-P4-WIFI6-DEV-KIT

USB MIDI 2.0 **host** example for the **Waveshare ESP32-P4-WIFI6-DEV-KIT**. Plugs the upstream device into either of the two USB-A jacks (UTMI PHY, OTG_HS controller, rhport 1), routes UMP through `m2host`, and renders device topology + live UMP stream on the UART console (CH343 USB-Serial-JTAG bridge on the "ToUART" USB-C jack). Lives at `midi2_cpp/examples/esp32-p4-devkit-host-midi2/` and consumes the parent library directly (no vendoring).

![esp32-p4-devkit-host-midi2 banner](board/banner.jpg)

> ⚠️ **TinyUSB override, not yet upstream.** The USB MIDI 2.0 host class driver this project depends on lives in TinyUSB [PR #3571](https://github.com/hathach/tinyusb/pull/3571), still under review. Until that PR merges into `hathach/tinyusb`, this build pulls a personal fork ([`sauloverissimo/tinyusb` branch `feat/midi2-device-host-driver`](https://github.com/sauloverissimo/tinyusb/tree/feat/midi2-device-host-driver)) at a pinned SHA into `idf/external/tinyusb`, registered as an ESP-IDF component by the shim at `idf/components/tinyusb`. Treat the build as **beta**: when the PR lands upstream the override goes away and this README will point at the official TinyUSB.

## What this is

`esp32-p4-devkit-host-midi2` is the platform layer for a MIDI 2.0 host on the ESP32-P4. It owns:

- ESP-IDF v5.4 board init (`usb_new_phy` with `USB_PHY_TARGET_UTMI`, host role, `USB_PHY_SPEED_UNDEFINED`, FreeRTOS task scheduler)
- TinyUSB MIDI 2.0 host class wiring via the **PR #3571 fork**, dropped into `idf/external/tinyusb` by the bootstrap script
- The four [midi2_cpp](https://github.com/sauloverissimo/midi2_cpp) host platform hooks already wired into `m2host`: `setWriteFn`, `feedRx`, `setNowFn`, `setRngFn`
- Auto-discovery on mount: UMP Stream Endpoint Discovery + MIDI-CI Discovery Inquiry fired without app code, then `DeviceIdentity` populated as replies arrive
- Multi-device addressing by `idx`. Up to `MIDI2_CPP_HOST_MAX_DEVICES` (default 4) connected MIDI 2.0 devices simultaneously through the dev-kit's onboard CH334F USB hub

The Waveshare ESP32-P4-WIFI6-DEV-KIT exposes two USB-C jacks and two USB-A jacks. The USB-A pair routes the P4's high-speed UTMI PHY (OTG_HS controller, 480 Mbps) through the onboard CH334F hub, which is what this recipe drives. The USB-C jack labelled **ToUART** routes the CH343 USB-Serial-JTAG bridge for console + flashing. The other USB-C jack labelled **USB-Device** routes the P4's full-speed INT PHY (OTG_FS controller) — unused in this host-only recipe; see [`esp32-p4-devkit-bridge-midi2`](../esp32-p4-devkit-bridge-midi2) for the dual-stack variant.

After `esp32_p4_devkit_host::init(host)`, the application sees only `m2host`. It never touches `tuh_*` or `esp_*` symbols. Replicating the same shape on another ESP32 host board is a matter of writing `<board>_host.{h,cpp}` with the same two-function surface.

## What this is not

Not a finished product. The bundled `esp32-p4-devkit-host-midi2-monitor` executable is a **demo application** that prints identity + decoded UMP traffic on the UART console. Real applications copy this core and replace the printf monitor with their own behaviour layer:

- **Bridge**: forward upstream UMP to the USB-Device USB-C jack so the PC sees the upstream device through the P4. See [`esp32-p4-devkit-bridge-midi2`](../esp32-p4-devkit-bridge-midi2).
- **Logger**: capture every UMP to SD card or flash for offline analysis (the dev-kit has a microSD slot).
- *(your project here)*

## Identification

Host's own MIDI-CI Initiator identity:

| Field | Value |
|---|---|
| MIDI-CI Manufacturer ID | `{0x7D, 0x00, 0x00}` (MIDI Association educational/non-commercial prefix, applies if the host promotes itself to a CI Initiator) |
| Host MUID | seeded on `begin()` from `esp_random()`, masked to 28 bits |

The host has its own identity because it is the **CI Initiator**: it sends Discovery Inquiry to plugged-in devices and stores their Discovery Reply MUIDs in `m2host::identity(idx).ciMuid`.

## Build

Requirements:

- **ESP-IDF v5.4 or newer** with the export script sourced (`. $IDF_PATH/export.sh`)
- The matching RISC-V toolchain (`riscv32-esp-elf`); run `$IDF_PATH/install.sh esp32p4` once on a fresh IDF
- A Waveshare ESP32-P4-WIFI6-DEV-KIT, two USB-C cables (one to the **ToUART** jack for flashing + console, the **USB-Device** jack stays unused), one USB-A cable per upstream MIDI 2.0 device under test
- Internet on the first run (the bootstrap script clones the TinyUSB fork into `idf/external/tinyusb`)

```bash
git clone https://github.com/sauloverissimo/midi2_cpp.git
cd midi2_cpp/examples/esp32-p4-devkit-host-midi2/idf
./scripts/fetch_tinyusb.sh         # one-off, ~36 MB clone of the fork at pinned SHA
. $IDF_PATH/export.sh
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/ttyACM0 flash monitor    # ToUART jack on the Waveshare kit
```

The Waveshare kit's "ToUART" USB-C jack uses a **CH343 USB-Serial-JTAG bridge** (VID `1a86:55d3`) which the Linux mainline kernel binds to `cdc_acm`, so `/dev/ttyACM0` is the natural device node. The CH343 has real DTR/RTS, so `idf.py flash` auto-resets the chip into download mode without a button press.

### Override TinyUSB with a local working copy

```bash
ln -sfn /path/to/your/tinyusb idf/external/tinyusb
idf.py reconfigure
```

## Hardware

| Connector / Pin | Use |
|---|---|
| USB-A jacks (×2) | UTMI host PHY (OTG_HS, 480 Mbps), routed through onboard CH334F USB hub. Plug upstream MIDI 2.0 devices here. |
| USB-C "ToUART" | CH343 USB-Serial-JTAG bridge, console stdio @ 115200 8N1 + flashing |
| USB-C "USB-Device" | INT device PHY (OTG_FS), **not used** in this host-only recipe |
| RJ45 | Ethernet, not used |
| Speaker JST | I2S audio out, not used |
| MIPI-CSI ribbon | Camera, not used |
| MIPI-DSI ribbon | Display, not used |
| BOOT button | Hold during reset to enter download mode (rarely needed; CH343 auto-reset handles it) |
| RESET button | Reboot |

## Console output

What the bundled `esp32-p4-devkit-host-midi2-monitor` executable prints on the UART console:

**Always-on (boot to forever):**

- `[boot] esp32-p4-devkit-host-midi2-monitor` on app start
- `Host UTMI PHY ready (rhport 1)` when the UTMI host PHY initialises
- `[host] waiting for device on USB-A jacks...` once the host stack is up
- **Auto-discovery** on mount: UMP Stream Endpoint Discovery + MIDI-CI Discovery Inquiry fire without app code
- **CI Initiator role**: host transmits Discovery Inquiry, replies populate `m2host::identity(idx)`

**Per device (when mounted):**

| Event | Console line |
|---|---|
| Mount | `[host] device idx=N connected, alt=A (UMP\|byte-stream)` |
| Endpoint Info notification | `[ep] idx=N UMP vM.m, F FB, MIDI2=1` |
| Endpoint Name notification | `[ep] idx=N Endpoint Name: <product>` |
| NoteOn | `[in idxN] NoteOn ch=C note=N vel=0xVVVV` |
| NoteOff | `[in idxN] NoteOff ch=C note=N vel=0xVVVV` |
| CC (32-bit) | `[in idxN] CC ch=C #I val=0xVVVVVVVV` |
| Pitch Bend (32-bit) | `[in idxN] PitchBend ch=C val=0xVVVVVVVV` |
| Disconnect | `[host] device idx=N disconnected` |

## Validation

Plug any of the device-side examples we ship into either USB-A jack — [`rp2040-midi2`](../rp2040-midi2), [`esp32-s3-devkitc-usb-midi2`](../esp32-s3-devkitc-usb-midi2), [`esp32-p4-devkit-usb-midi2`](../esp32-p4-devkit-usb-midi2), [`waveshare-rp2040-midi2`](../waveshare-rp2040-midi2), or any third-party MIDI 2.0 device. The 22 s cycle of those devices emits every category of UMP MIDI 2.0 brings beyond MIDI 1.0 (Flex Data, Per-Note expression, 16-bit velocity walk, 32-bit CC sweep, Program+Bank, RPN/NRPN/Relative, Note Attribute pitch_7_9, SysEx8, Delta Clockstamp, PE Notify, JR Heartbeat). Each of those should appear decoded on the UART console in real time, proving the round trip works end-to-end at the wire level.

The dev-kit's onboard CH334F hub means up to 4 MIDI 2.0 devices can be plugged simultaneously and addressed by `idx` from the application — the same multi-device shape Adafruit's [`adafruit-feather-rp2040-host-midi2`](../adafruit-feather-rp2040-host-midi2) ships, just over high-speed UTMI instead of PIO-USB.

## What lives where

```
midi2_cpp/
├── src/                            parent library (consumed by this example
│                                   via ../../../src in idf/main/CMakeLists.txt)
└── examples/esp32-p4-devkit-host-midi2/
    ├── README.md
    ├── board/
    │   ├── banner.jpg              repo banner (used in this README)
    │   ├── board.png               Waveshare top-down product photo
    │   ├── features.png            feature callouts
    │   ├── pinout.png              pinout reference
    │   └── ESP32-P4-WIFI6-DEV-KIT-datasheet.pdf
    ├── monitor/                    UART captures (TBD)
    └── idf/
        ├── CMakeLists.txt          ESP-IDF project root
        ├── partitions.csv          single-app, 16 MB flash
        ├── sdkconfig.defaults      target esp32p4, UART stdio, custom partition table
        ├── scripts/
        │   └── fetch_tinyusb.sh    bootstrap: clones TinyUSB fork into external/tinyusb
        ├── external/                (gitignored, populated by fetch_tinyusb.sh)
        │   └── tinyusb/             raw clone of the PR #3571 fork at pinned SHA
        ├── components/
        │   └── tinyusb/
        │       └── CMakeLists.txt   shim: registers the fork's host sources as
        │                            an ESP-IDF component named "tinyusb"
        │                            (no usb_descriptors.c on host-only recipes)
        └── main/
            ├── CMakeLists.txt      idf_component_register, pulls midi2_cpp from ../../../../src
            ├── idf_component.yml
            ├── tusb_config.h       CFG_TUH_MIDI2=1, rhport 1 (UTMI HS)
            ├── esp32_p4_devkit_host.h    public API of the platform glue
            ├── esp32_p4_devkit_host.cpp  UTMI host PHY init + TinyUSB host task + m2host hooks
            └── main.cpp            monitor entry, m2host callbacks → UART printf
```

The TinyUSB PR #3571 fork is dropped into `idf/external/tinyusb` (gitignored) by `idf/scripts/fetch_tinyusb.sh`. The shim component at `idf/components/tinyusb/` registers a curated subset of the fork's sources (tusb core + host stack + MIDI 2.0 host class driver + DWC2 HCD) as an ESP-IDF component named `tinyusb`. There is no `usb_descriptors.c` on this host-only recipe; descriptors are a device concept.

The board datasheet is bundled (`board/ESP32-P4-WIFI6-DEV-KIT-datasheet.pdf`, 4 MB) because it carries the dev-kit-specific schematic excerpt and pinout that is hard to find elsewhere. The MCU silicon datasheet is not bundled; read it on Espressif's site: [ESP32-P4 series datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-p4_datasheet_en.pdf).

## License

MIT, inherits the parent [`midi2_cpp` LICENSE](../../LICENSE). The TinyUSB fork (cloned on demand into `idf/external/tinyusb`) is MIT (upstream by hathach, fork by sauloverissimo carrying the MIDI 2.0 class drivers from the still-open [PR #3571](https://github.com/hathach/tinyusb/pull/3571)).
