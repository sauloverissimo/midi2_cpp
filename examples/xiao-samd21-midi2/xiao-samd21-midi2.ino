/*
 * xiao-samd21-midi2.ino, USB MIDI 2.0 device sketch for the Seeed Studio
 * XIAO SAMD21 (ATSAMD21G18A, 48 MHz Cortex-M0+, 32 KB SRAM, 256 KB
 * flash). Tier C minimal core: NoteOn/Off + CC + MIDI-CI Discovery
 * responder + UMP Stream Discovery responder + JR Timestamp heartbeat.
 *
 * Identity:
 *   USB VID:PID         0xCAFE:0x40F0
 *   Manufacturer string github.com/sauloverissimo
 *   Product string      XiaoSAMD21
 *   Endpoint Name       XiaoSAMD21
 *   Product Inst Id     XiaoSAMD21-showcase-0001
 *
 * Build:
 *   Arduino IDE 2.x or arduino-cli with the Adafruit_TinyUSB_Arduino
 *   fork that carries the TinyUSB PR #3571 MIDI 2.0 device class. See
 *   the recipe README for the install steps.
 */

#include <Adafruit_TinyUSB.h>

#include "midi2_cpp.h"   // parent library; vendor or symlink ../../src
                         // into your sketch's libraries path so the
                         // Arduino IDE finds it.

// Adafruit_USBD_MIDI2 ships with the Adafruit_TinyUSB_Arduino fork that
// carries TinyUSB PR #3571. The class exposes the same surface as the
// upstream Adafruit_USBD_MIDI plus 32-bit UMP read/write.
Adafruit_USBD_MIDI2 usb_midi2;

using namespace midi2;

static m2device midi;
static m2ci     ci(midi);

/*--------------------------------------------------------------------+
 * Identity
 *--------------------------------------------------------------------*/
static const uint8_t  kMfrId[3]        = {0x7D, 0x00, 0x00};
static const uint16_t kFamilyId        = 0x0001;
static const uint16_t kModelId         = 0x0001;
static const uint32_t kVersion         = 0x00010000;
static const char     kEndpointName[]  = "XiaoSAMD21";
static const char     kProductInstId[] = "XiaoSAMD21-showcase-0001";
static const char     kFbName[]        = "Main";

/*--------------------------------------------------------------------+
 * Platform hooks
 *--------------------------------------------------------------------*/
static void platform_write_fn(const uint32_t* words, size_t count) {
    if (!usb_midi2.mounted()) return;
    if (usb_midi2.altSetting() != 1) return;
    usb_midi2.write(words, count);
}

static uint32_t platform_now_fn() {
    return (uint32_t)millis();
}

static uint32_t platform_rng_fn() {
    // SAMD21 has no hardware TRNG. Seed the Arduino PRNG with an
    // unconnected ADC pin's noise once in setup(); here we just return
    // 32 bits of `random()`.
    return ((uint32_t)random(0, 0x10000) << 16) | (uint32_t)random(0, 0x10000);
}

/*--------------------------------------------------------------------+
 * UMP Stream Discovery responder (mandatory for any MIDI 2.0 device)
 *--------------------------------------------------------------------*/
static void install_stream_responder() {
    midi.onEndpointDiscovery([](uint8_t filter) {
        if (filter & 0x01) {
            midi.sendEndpointInfo(/*ump_ver*/ 1, 1,
                                  /*static_fb*/ true, /*num_fb*/ 1,
                                  /*midi2*/ true, /*midi1*/ true,
                                  /*rx_jr*/ false, /*tx_jr*/ true);
        }
        if (filter & 0x02) midi.sendDeviceIdentity(kMfrId, kFamilyId, kModelId, kVersion);
        if (filter & 0x04) midi.sendEndpointNameUpdate(kEndpointName);
        if (filter & 0x08) midi.sendProductInstanceIdUpdate(kProductInstId);
        if (filter & 0x10) midi.sendStreamConfigNotify(/*protocol*/ 0x02);
    });
    midi.onFbDiscovery([](uint8_t fbNum, uint8_t filter) {
        uint8_t target = (fbNum == 0xFF) ? 0 : fbNum;
        if (target != 0) return;
        if (filter & 0x01) {
            midi.sendFbInfo(/*active*/ true, /*fb_num*/ 0,
                            /*direction*/ 0x03, /*first_group*/ 0,
                            /*num_groups*/ 1, /*midi_ci_ver*/ 0x02,
                            /*sysex8*/ false, /*protocol*/ 0x02);
        }
        if (filter & 0x02) midi.sendFbNameUpdate(0, kFbName);
    });
    midi.onStreamConfigRequest([](uint8_t protocol) {
        midi.sendStreamConfigNotify(protocol);
    });
}

/*--------------------------------------------------------------------+
 * Tier C demo: chromatic walk with CC sweep, no Per-Note, no SysEx,
 * no Flex Data, no PE storage. Everything that fits in 32 KB SRAM.
 *--------------------------------------------------------------------*/
constexpr uint8_t  kCh           =    0;
constexpr uint8_t  kBaseNote     =   60;       // C4
constexpr uint8_t  kStepCount    =    8;
constexpr uint32_t kStepMs       =  500;
constexpr uint32_t kCycleGapMs   = 2000;       // pause between cycles

static uint8_t  g_step       = 0;
static uint32_t g_last_step  = 0;
static bool     g_in_cycle   = false;
static uint32_t g_cycle_end  = 0;

static void tier_c_step() {
    if (!midi.isMounted() || midi.altSetting() != 1) return;

    uint32_t now = millis();

    // gap between cycles
    if (!g_in_cycle && now < g_cycle_end) return;

    if (g_step < kStepCount && (g_step == 0 || (now - g_last_step) >= kStepMs)) {
        if (g_step > 0) {
            midi.noteOff(kCh, (uint8_t)(kBaseNote + g_step - 1));
        }
        uint8_t note = (uint8_t)(kBaseNote + g_step);
        // 16-bit velocity ramp
        uint16_t vel = (uint16_t)(0x2000u + (uint32_t)g_step *
                                  ((0xFFFFu - 0x2000u) / (kStepCount - 1)));
        midi.noteOn(kCh, note, vel);
        // 32-bit CC #74 sweep
        uint32_t cc_val = 0x20000000u + (uint32_t)g_step *
                          ((0xFFFFFFFFu - 0x20000000u) / (kStepCount - 1));
        midi.cc(kCh, /*idx*/ 74, cc_val);

        g_last_step = now;
        g_step++;
        g_in_cycle = true;
    }

    if (g_in_cycle && g_step == kStepCount && (now - g_last_step) >= kStepMs) {
        midi.noteOff(kCh, (uint8_t)(kBaseNote + kStepCount - 1));
        g_step       = 0;
        g_in_cycle   = false;
        g_cycle_end  = now + kCycleGapMs;
    }
}

/*--------------------------------------------------------------------+
 * Arduino entry points
 *--------------------------------------------------------------------*/
void setup() {
    // Seed the SAMD21 Arduino PRNG from an unconnected analog pin.
    // Used by platform_rng_fn for MUID generation. Quality is poor but
    // sufficient for an educational example; production firmware should
    // use a real entropy source.
    randomSeed((unsigned long)analogRead(A0));

    // USB identity must be set BEFORE TinyUSB is started (which happens
    // implicitly when usb_midi2.begin() runs).
    USBDevice.setID(0xCAFE, 0x40F0);
    USBDevice.setManufacturerDescriptor("github.com/sauloverissimo");
    USBDevice.setProductDescriptor("XiaoSAMD21");
    USBDevice.setSerialDescriptor("XiaoSAMD21-0001");

    usb_midi2.setStringDescriptor(kFbName);
    usb_midi2.begin();

    // Wait for USB enumeration before booting the library wiring; the
    // SAMD21 USB stack does not survive being driven before plug.
    while (!USBDevice.mounted()) delay(10);

    midi.setWriteFn(platform_write_fn);
    midi.setNowFn(platform_now_fn);
    midi.setMounted(true);
    midi.setAltSetting(usb_midi2.altSetting());
    ci.setRngFn(platform_rng_fn);

    midi.begin();
    midi.enableJRHeartbeat(500);
    ci.begin(kMfrId, kFamilyId, kModelId, kVersion);

    install_stream_responder();

    // Tier C: only Discovery on the MIDI-CI side. No Profile, no PE
    // storage, no Process Inquiry advertising.
}

void loop() {
    // Refresh USB lifecycle state into the library.
    bool mounted = usb_midi2.mounted();
    midi.setMounted(mounted);
    midi.setAltSetting(mounted ? usb_midi2.altSetting() : 0);

    // Drain RX into the library dispatcher.
    if (mounted) {
        uint32_t buf[8];
        for (;;) {
            uint32_t n = usb_midi2.read(buf, 8);
            if (n == 0) break;
            midi.feedRx(buf, n);
        }
    }

    midi.task();        // heartbeat, deferred sends
    tier_c_step();      // demo emissions
}
