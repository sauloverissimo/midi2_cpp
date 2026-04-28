/*
 * feather_host.h — public API of the Adafruit Feather RP2040 USB Host
 * platform layer.
 *
 * Wires Pico SDK + PIO-USB + TinyUSB host (PR #3571 fork) into m2host
 * via the five public hooks. The application sees only midi2::m2host
 * and never touches tuh_*, pico_*, or any USB symbol directly.
 *
 * Replicating this pattern for another host board is a matter of
 * writing <board>_host.{h,cpp} that exposes the same two-function
 * surface (init + task).
 */
#pragma once

#include "midi2_cpp.h"

namespace feather_host {

// Boots board_init + PIO-USB on GP16/GP17 + tuh_init, drives the USB-A
// 5V power gate, and wires the m2host platform hooks. After this
// returns the app can register callbacks on midi (m2host) and call
// task() in its main loop.
//
// Must be called once at startup, before any midi callback registration
// or sender call.
void init(midi2::m2host& midi);

// Drains the host USB stack (tuh_task), pumps received UMP into
// midi.feedRx via tuh_midi2_rx_cb (deferred from callback to task by
// design; the polling here picks up RX from any device idx that has
// data pending). Call every iteration of the main loop.
void task(midi2::m2host& midi);

}  // namespace feather_host
