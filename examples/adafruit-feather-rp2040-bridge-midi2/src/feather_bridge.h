/*
 * feather_bridge.h — public API of the dual-stack bridge platform layer.
 *
 * Boots TinyUSB on both rhports of the Adafruit Feather RP2040 USB Host:
 *   rhport 0 — native USB device  → exposes a 16-group MIDI 2.0 endpoint to the PC
 *   rhport 1 — PIO-USB host       → enumerates the upstream MIDI device on USB-A
 *
 * Forwards UMP between the two through a single-threaded ring buffer
 * (see ump_router). Upstream USB-MIDI 1.0 devices (alt=0) are uplifted
 * to UMP MT 0x2 so the PC always sees clean MIDI 2.0; PC->upstream
 * traffic is forwarded only when the upstream is MIDI 2.0 in v0.1.
 *
 * Optional callbacks expose mount/unmount events and forwarded UMP for
 * UI/logging. main.cpp uses them to drive the SSD1306 display.
 */
#pragma once

#include <cstdint>
#include <functional>

#include "ump_router.h"

namespace feather_bridge {

// Maximum number of upstream device idx the host stack tracks. Matches
// CFG_TUH_MIDI2 in tusb_config.h. Only idx 0 is forwarded in v0.1.
static constexpr uint8_t MAX_HOST_DEVICES = 4;

// Mount/unmount notifications for the upstream USB-A device.
//   protocol_version: 0 = MIDI 1.0 (alt=0), >=1 = MIDI 2.0 (alt=1)
using HostMountFn   = std::function<void(uint8_t idx, uint8_t protocol_version)>;
using HostUnmountFn = std::function<void(uint8_t idx)>;

// Notification for each forwarded UMP message. count is 1..4 words.
using ForwardFn = std::function<void(const uint32_t* words, uint8_t count)>;

// Notification when the device side mounts/unmounts on the PC.
using DeviceLifecycleFn = std::function<void()>;

// Notification when a queue overflowed (drop counter incremented).
using DropFn = std::function<void(ump_source_t src, uint32_t total_drops)>;

void init();
void task();

// True when the upstream USB-A device is enumerated and forwarding.
bool upstream_present();

// True when the PC has the device side mounted.
bool downstream_present();

// Send a UMP message (1..4 words) directly to the PC. Bypasses the
// router; used by the standalone showcase mode in main.cpp when no
// upstream device is connected. Returns false if the device side is
// not mounted, not in alt=1 (UMP), or the count is out of range.
bool send_to_pc(const uint32_t* words, uint8_t count);

// Callback registration. Latest setter wins. Calling with an empty
// std::function clears the slot.
void onHostMount(HostMountFn fn);
void onHostUnmount(HostUnmountFn fn);
void onForwardUpstream(ForwardFn fn);     // upstream USB-A -> PC
void onForwardDownstream(ForwardFn fn);   // PC -> upstream USB-A
void onDeviceMount(DeviceLifecycleFn fn);
void onDeviceUnmount(DeviceLifecycleFn fn);
void onDrop(DropFn fn);

}  // namespace feather_bridge
