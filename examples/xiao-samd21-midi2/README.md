# [midi2_cpp](../..) | Device MIDI 2.0
## Seeed Studio XIAO SAMD21

Tier C minimal-core USB MIDI 2.0 device example for the [**Seeed Studio XIAO SAMD21**](https://wiki.seeedstudio.com/Seeeduino-XIAO/) (ATSAMD21G18A, 48 MHz Cortex-M0+, 32 KB SRAM, 256 KB flash). Single-file Arduino sketch that demonstrates MIDI 2.0 enumeration, MIDI-CI Discovery, UMP Stream Discovery, JR Timestamp heartbeat, and a chromatic walk with 16-bit velocity + 32-bit CC sweep on Channel 1. Lives at `midi2_cpp/examples/xiao-samd21-midi2/` and consumes the parent library directly.

> ⚠️ **Adafruit_TinyUSB_Arduino fork required, not yet upstream.** This sketch depends on `Adafruit_USBD_MIDI2`, the wrapper class added by a fork of [`adafruit/Adafruit_TinyUSB_Arduino`](https://github.com/adafruit/Adafruit_TinyUSB_Arduino) that vendors TinyUSB at the [PR #3571](https://github.com/hathach/tinyusb/pull/3571) commit (`31d730d8b...`). Until both upstreams (Adafruit + TinyUSB) merge the support, install the fork manually. See **Build** below.

PID `0x40F0` distinguishes this device from the Tier 1 / Tier 2 recipes. The Tier 3 / experimental window is `0x40F0..0x40FF`.

## What this is

`xiao-samd21-midi2.ino` is a single-file sketch that exercises the smallest viable MIDI 2.0 device contract on the SAMD21:

- **MIDI-CI Discovery responder** (Manufacturer + Family + Model + Version, MAX SysEx 512 bytes)
- **UMP Stream Discovery responder** (Endpoint Info, Device Identity, Endpoint Name, Product Instance ID, Stream Config Notify, FB Info, FB Name)
- **JR Timestamp heartbeat** every 500 ms (MT 0x0)
- **Chromatic walk** on note C4 to G#4 with 16-bit velocity ramp + 32-bit CC #74 sweep, every 500 ms, 2 s gap between cycles

No Per-Note expression, no Flex Data, no SysEx, no Property Exchange storage, no Process Inquiry advertising. Those would not fit in 32 KB SRAM with the Adafruit_TinyUSB stack already resident; the recipe stays Tier C by design.

## What this is not

Not a finished product. Real-world XIAO SAMD21 MIDI 2.0 applications can extend this sketch with:

- A capacitive touch input (XIAO has 5 ADC pins) emitting Per-Note Pitch Bend
- An encoder + LED ring driven by the chromatic walk
- A footswitch sending Program Change with bank

## Identification

| Field | Value |
|---|---|
| USB VID | `0xCAFE` |
| USB PID | `0x40F0` |
| USB Manufacturer | `github.com/sauloverissimo` |
| USB Product | `XiaoSAMD21` |
| Endpoint Name | `XiaoSAMD21` |
| Product Instance ID | `XiaoSAMD21-showcase-0001` |
| MIDI-CI Manufacturer ID | `{0x7D, 0x00, 0x00}` (MIDI Association educational/non-commercial prefix) |
| MIDI-CI Family / Model / Version | `0x0001 / 0x0001 / 0x00010000` |

The USB VID `0xCAFE` is the TinyUSB educational identifier. **Production firmware MUST replace both `idVendor` and `idProduct`** with a real allocation (`0x1209` pid.codes, `0x16C0` V-USB, or a purchased USB-IF VID).

## Build

Requirements:

1. **Arduino IDE 2.x** or **arduino-cli 1.0+**
2. **Seeed SAMD board package** in the Arduino Boards Manager (URL: `https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json`)
3. **Adafruit_TinyUSB_Arduino fork** with TinyUSB PR #3571 vendored (see below)
4. **midi2_cpp library** in the Arduino sketchbook's `libraries/` folder

### Install the Adafruit_TinyUSB_Arduino fork

```bash
cd ~/Arduino/libraries
git clone https://github.com/sauloverissimo/Adafruit_TinyUSB_Arduino.git
cd Adafruit_TinyUSB_Arduino
git checkout feat/midi2  # branch carrying the PR #3571 vendored TinyUSB
```

If the fork branch does not exist yet, the user can build it locally by:

1. Cloning `adafruit/Adafruit_TinyUSB_Arduino`
2. Replacing `src/arduino/ports/<chip>/tusb_config_<chip>.h` to enable `CFG_TUD_MIDI2`
3. Vendoring the TinyUSB MIDI 2.0 device class from `sauloverissimo/tinyusb` at SHA `31d730d8bb0b5c0832c5490378a2a2dd60ab72aa`
4. Adding `Adafruit_USBD_MIDI2.{h,cpp}` mirroring `Adafruit_USBD_MIDI` plus `tud_midi2_n_*` calls

This is non-trivial and is the main reason this recipe is **Tier 3 (guidance only)**. The skill's `tier-2-3-guidance.md` documents the path; the user owns the integration. Hardware test will likely surface gotchas; iterate.

### Install midi2_cpp

```bash
cd ~/Arduino/libraries
git clone https://github.com/sauloverissimo/midi2_cpp.git
```

The Arduino IDE picks up `midi2_cpp` automatically once it sits under `libraries/`.

### Compile and upload

```bash
arduino-cli compile --fqbn Seeeduino:samd:seeed_XIAO_m0 examples/xiao-samd21-midi2/
arduino-cli upload  --fqbn Seeeduino:samd:seeed_XIAO_m0 -p /dev/ttyACM0 examples/xiao-samd21-midi2/
```

To enter UF2 bootloader mode on the XIAO SAMD21, **double-tap the small RST pad** with a wire / tweezers (no built-in button). The board re-enumerates as `XIAO BOOT` mass-storage; drag-and-drop a UF2 alternative also works.

## Hardware

| Pin | Use |
|---|---|
| USB-C | Native USB-FS, MIDI 2.0 device interface |
| D13 / PA17 | `LED_BUILTIN` (yellow, **active LOW**). Lit while USB is mounted. Library does not toggle this in the v0.1 sketch; user can add `digitalWrite(LED_BUILTIN, !mounted)` in `loop()` if desired |
| D14 / PA18 | RST pad (double-tap to enter UF2 bootloader) |

The XIAO SAMD21 has no hardware True-Random-Number-Generator. The sketch seeds the Arduino PRNG from an unconnected analog pin (A0) once in `setup()` and uses `random()` for MUID generation. Quality is poor; production firmware should use a better entropy source.

## Spec coverage

**Tier C** (minimal core). Hardware-bracket reference for SAMD21G18A with very tight flash and 32 KB SRAM.

### What this recipe emits and demonstrates

| UMP MT | Transport | Spec section | Showcase Scene | Notes |
|---|---|---|---|---|
| 0x0 Utility | USB | M2-104-UM §3 | JR heartbeat | 500 ms periodicity |
| 0x4 MIDI 2.0 Channel Voice | USB | M2-104-UM §7 | chromatic walk | NoteOn/Off + CC #74 only |
| 0xF UMP Stream | USB | M2-104-UM §10 | (responder, not a Scene) | Endpoint Discovery, Device Identity, Endpoint Name, Product Instance ID, Stream Config Notify, FB Info, FB Name |

### MIDI-CI surface (M2-101-UM)

| Subsystem | Coverage |
|---|---|
| Discovery (Initiator + Responder) | responder: yes (MUID, Manufacturer, Family, Model, Version, MaxSysEx, Categories) |
| Profile Configuration | not advertised |
| Property Exchange | not advertised |
| Process Inquiry | not advertised |

### What this recipe does NOT cover (and why)

- **Per-Note expression family (MT 0x4 sub-statuses)**, the SAMD21 has the cycles, but the showcase keeps emissions minimal to leave SRAM headroom for the user's own code.
- **MT 0x3 SysEx7 / MT 0x5 SysEx8**, the parent library reassembly buffers (default 512 bytes each) plus the Adafruit_TinyUSB stack would push past 32 KB SRAM.
- **MT 0xD Flex Data**, scope drop, future Tier C+ recipe could opt into Tempo + Time Sig only.
- **Property Exchange storage**, requires 8 properties × ~50 bytes each plus subscriber list; SAMD21 SRAM cannot afford it.
- **Process Inquiry capability advertising**, drops with PE.
- **MIDI 2.0 Initiator role for CI**, this is a device-side responder.

## Showcase

What the sketch emits after enumeration, while `usb_midi2.mounted() && altSetting()==1`:

| Always-on | Detail |
|---|---|
| **JR Timestamp heartbeat** | every 500 ms (MT 0x0 status 0x2) |
| **UMP Stream Discovery responder** | replies to host Endpoint Discovery and FB Discovery |
| **MIDI-CI Discovery responder** | replies with Manufacturer + Family + Model + Version |

| Per cycle (every 4.5 s) | Detail |
|---|---|
| **Chromatic walk** | C4, C#4, D4, D#4, E4, F4, F#4, G4, with 16-bit velocity ramp 0x2000 to 0xFFFF |
| **CC #74 sweep** | 32-bit CC value 0x20000000 to 0xFFFFFFFF synchronised with note steps |
| **2 s gap** | between cycles |

## Validation

Hardware steps:

1. Plug the XIAO SAMD21 into a Linux / macOS / Windows host via USB-C.
2. Confirm enumeration:
   - **Linux**: `lsusb | grep cafe:40F0` shows `XiaoSAMD21`. `amidi -l` lists `Group 1 (Main)`. `aseqdump -p <port>` shows the chromatic walk live.
   - **Windows**: Microsoft MIDI Services Console shows `XiaoSAMD21` with Native data format = UMP, MIDI 2.0 Protocol = True.
   - **macOS**: Audio MIDI Setup shows `XiaoSAMD21`.
3. Pair with a known-good MIDI 2.0 host recipe ([`adafruit-feather-rp2040-host-midi2`](../adafruit-feather-rp2040-host-midi2/), [`esp32-p4-devkit-host-midi2`](../esp32-p4-devkit-host-midi2/)) for a cross-platform sanity check.

## What lives where

```
midi2_cpp/examples/xiao-samd21-midi2/
├── README.md
├── board/                              board photo / pinout (TBD)
├── monitor/                            Microsoft MIDI Console captures (TBD)
└── xiao-samd21-midi2.ino               Arduino sketch, single file
```

## License

MIT, inherits the parent [`midi2_cpp` LICENSE](../../LICENSE). The Adafruit_TinyUSB_Arduino fork (installed by the user into `~/Arduino/libraries/`) is MIT (upstream by Adafruit, fork by sauloverissimo carrying the MIDI 2.0 class drivers from the still-open [TinyUSB PR #3571](https://github.com/hathach/tinyusb/pull/3571)).
