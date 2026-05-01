"""
Pre-build patch for ESP32_Host_MIDI v6.0.0 on chips without USB-OTG.

The library exposes per-transport opt-in headers in v6.0 (umbrella header
no longer auto-includes USBConnection / BLEConnection / ESPNowConnection),
but PlatformIO's Library Dependency Finder compiles every .cpp it finds
under the library's src/. On a chip with no USB-OTG hardware (ESP32-C6,
ESP32-C3, classic ESP32, ESP32-H2), USBConnection.cpp and
USBMIDI2Connection.cpp transitively include esp_idf headers that do not
exist for that target (`usb/usb_host.h`), so the build aborts before the
recipe's main.cpp is ever compiled.

This script replaces the body of those two files with an empty
translation unit (the .h headers stay intact and gated by
`ESP32_HOST_MIDI_HAS_USB`, which is false on these chips). It is
idempotent: a second invocation detects the marker and is a no-op.

Loaded via `extra_scripts = pre:extra/patch_esp32_host_midi.py` in
platformio.ini. Runs at script-load time, before the LDF compiles the
library.
"""
from pathlib import Path

Import("env")  # type: ignore[name-defined]  # noqa: F821

PATCH_MARKER = "// PATCHED-FOR-NO-USB-OTG"

EMPTY_BODY = (
    PATCH_MARKER + "\n"
    "// This translation unit is intentionally empty.\n"
    "// On chips without USB-OTG hardware (ESP32-C6, ESP32-C3, classic ESP32,\n"
    "// ESP32-H2), the USB Host transports of ESP32_Host_MIDI are unavailable.\n"
    "// Recipe code never references this transport, but PlatformIO's LDF\n"
    "// compiles every .cpp under the library's src/ regardless. The empty\n"
    "// body keeps the build green without changing the library on disk for\n"
    "// other recipes that DO use USB Host on this same machine.\n"
    "// See examples/esp32-c6-devkitc-multi-midi2/pio/extra/patch_esp32_host_midi.py.\n"
)

TARGETS = (
    "USBConnection.cpp",
    "USBMIDI2Connection.cpp",
)


def patch_lib():
    project_dir = Path(env["PROJECT_DIR"])  # type: ignore[name-defined]  # noqa: F821
    libdir = (
        project_dir
        / ".pio"
        / "libdeps"
        / env["PIOENV"]  # type: ignore[name-defined]  # noqa: F821
        / "ESP32_Host_MIDI"
        / "src"
    )
    if not libdir.is_dir():
        # First configure: lib_deps not cloned yet. PlatformIO clones the lib
        # before invoking the compile step but after this script's load. The
        # script gets re-imported on a second pass in the same run via the LDF
        # entry point, so leave a hook for that case.
        return
    patched_any = False
    for name in TARGETS:
        target = libdir / name
        if not target.is_file():
            continue
        text = target.read_text(encoding="utf-8")
        if text.startswith(PATCH_MARKER):
            continue
        target.write_text(EMPTY_BODY, encoding="utf-8")
        patched_any = True
    if patched_any:
        print(
            "[patch_esp32_host_midi] neutralized USB Host transports "
            "(target chip has no USB-OTG)"
        )


patch_lib()
