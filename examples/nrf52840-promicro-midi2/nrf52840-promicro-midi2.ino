/*
 * nrf52840-promicro-midi2.ino, USB MIDI 2.0 device sketch for the
 * nRF52840 Pro Micro / Nice!Nano-class boards (nRF52840 Cortex-M4F
 * @ 64 MHz, 256 KB SRAM, 1 MB flash, native USB FS).
 *
 * Tier B standard subset: MT 0x0 (Utility / JR), MT 0x4 (channel voice
 * including Per-Note + RPN/NRPN), MT 0xF (UMP Stream Discovery
 * responder), MIDI-CI Discovery responder. SysEx7/SysEx8 optional;
 * Flex Data, MDS, Property Exchange storage, Process Inquiry advanced
 * are out of v0.1 scope.
 *
 * Identity:
 *   USB VID:PID         0xCAFE:0x40F1
 *   Manufacturer string github.com/sauloverissimo
 *   Product string      Nrf52840ProMicro
 *   Endpoint Name       Nrf52840ProMicro
 *   Product Inst Id     Nrf52840ProMicro-showcase-0001
 *
 * Build:
 *   Arduino IDE 2.x or arduino-cli with the Adafruit_TinyUSB_Arduino
 *   fork that carries the TinyUSB PR #3571 MIDI 2.0 device class. See
 *   the recipe README for the install steps.
 */

#include <Adafruit_TinyUSB.h>

#include "midi2_cpp.h"

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
static const char     kEndpointName[]  = "Nrf52840ProMicro";
static const char     kProductInstId[] = "Nrf52840ProMicro-showcase-0001";
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

// nRF52840 has a hardware RNG peripheral, but accessing it cleanly from
// the Adafruit Bluefruit core requires SoftDevice initialization which
// is overkill for the MUID generation use case. Use the Arduino PRNG
// seeded from analogRead in setup(), like the SAMD21 sketch.
static uint32_t platform_rng_fn() {
    return ((uint32_t)random(0, 0x10000) << 16) |
           (uint32_t)random(0, 0x10000);
}

/*--------------------------------------------------------------------+
 * UMP Stream Discovery responder
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
 * Tier B showcase: chromatic walk + Per-Note Pitch Bend + RPN/NRPN.
 * Drops Flex Data, SysEx, PE, PI, End-of-Clip vs the rp2040 Tier A.
 *--------------------------------------------------------------------*/
constexpr uint8_t  kCh         =    0;

constexpr uint8_t  kPnNote     =   60;       // C4 with Per-Note PB
constexpr uint32_t kPnOnMs     =  400;
constexpr uint32_t kPnOffMs    = 4000;
constexpr uint32_t kPnVibMs    =   25;       // 40 Hz Per-Note PB updates

constexpr uint8_t  kWalkBase   =   72;       // C5 chromatic walk
constexpr uint8_t  kWalkCount  =    8;
constexpr uint32_t kWalkStart  = 4500;
constexpr uint32_t kWalkStep   =  500;
constexpr uint32_t kWalkEnd    = kWalkStart + kWalkCount * kWalkStep;

constexpr uint32_t kRpnMs      = 9000;
constexpr uint32_t kNrpnMs     = 9500;
constexpr uint32_t kRelRpnMs   =10000;
constexpr uint32_t kRelNrpnMs  =10500;

constexpr uint32_t kCycleMs    =13000;

struct Showcase {
    uint32_t cycle_start = 0;
    bool pn_on = false, pn_off = false;
    uint32_t pn_last_vib = 0;
    uint8_t walk_idx = 0;
    uint32_t walk_last = 0;
    bool walk_released = false;
    bool rpn_done = false, nrpn_done = false;
    bool relrpn_done = false, relnrpn_done = false;
    uint32_t cycle_count = 0;
};

static Showcase g_show;

static void scene_pernote(uint32_t t, uint32_t now) {
    if (!g_show.pn_on && t >= kPnOnMs) {
        midi.noteOn(kCh, kPnNote, 0xC000);
        g_show.pn_on = true;
    }
    if (g_show.pn_on && !g_show.pn_off && t < kPnOffMs) {
        if (now - g_show.pn_last_vib >= kPnVibMs) {
            float secs = (float)(t - kPnOnMs) / 1000.0f;
            float v    = sinf(secs * 2.0f * 3.14159265f * 5.0f);
            int32_t off = (int32_t)(v * (float)0x10000000);
            uint32_t pb = (uint32_t)((int64_t)0x80000000 + off);
            midi.sendPerNotePitchBend(0, kCh, kPnNote, pb);
            g_show.pn_last_vib = now;
        }
    }
    if (g_show.pn_on && !g_show.pn_off && t >= kPnOffMs) {
        midi.sendPerNotePitchBend(0, kCh, kPnNote, 0x80000000u);
        midi.noteOff(kCh, kPnNote);
        g_show.pn_off = true;
    }
}

static void scene_walk(uint32_t t, uint32_t now) {
    if (g_show.walk_idx < kWalkCount && t >= kWalkStart &&
        (g_show.walk_idx == 0 || (now - g_show.walk_last) >= kWalkStep)) {
        if (g_show.walk_idx > 0) {
            midi.noteOff(kCh, (uint8_t)(kWalkBase + g_show.walk_idx - 1));
        }
        uint8_t note = (uint8_t)(kWalkBase + g_show.walk_idx);
        uint16_t vel = (uint16_t)(0x2000u + (uint32_t)g_show.walk_idx *
                                  ((0xFFFFu - 0x2000u) / (kWalkCount - 1)));
        midi.noteOn(kCh, note, vel);
        uint32_t cc_val = 0x20000000u + (uint32_t)g_show.walk_idx *
                          ((0xFFFFFFFFu - 0x20000000u) / (kWalkCount - 1));
        midi.cc(kCh, /*idx*/ 74, cc_val);
        g_show.walk_last = now;
        g_show.walk_idx++;
    }
    if (!g_show.walk_released && g_show.walk_idx == kWalkCount && t >= kWalkEnd) {
        midi.noteOff(kCh, (uint8_t)(kWalkBase + kWalkCount - 1));
        g_show.walk_released = true;
    }
}

static void scene_rpn_nrpn(uint32_t t) {
    if (!g_show.rpn_done && t >= kRpnMs) {
        midi.sendRpn(0, kCh, /*msb*/ 0, /*lsb*/ 0, /*val32*/ 0x40000000u);
        g_show.rpn_done = true;
    }
    if (!g_show.nrpn_done && t >= kNrpnMs) {
        midi.sendNrpn(0, kCh, /*msb*/ 0x12, /*lsb*/ 0x34, /*val32*/ 0xDEADBEEFu);
        g_show.nrpn_done = true;
    }
    if (!g_show.relrpn_done && t >= kRelRpnMs) {
        midi.sendRelRpn(0, kCh, /*msb*/ 0, /*lsb*/ 0, /*delta*/ 0x01000000);
        g_show.relrpn_done = true;
    }
    if (!g_show.relnrpn_done && t >= kRelNrpnMs) {
        midi.sendRelNrpn(0, kCh, /*msb*/ 0x12, /*lsb*/ 0x34, /*delta*/ -0x00800000);
        g_show.relnrpn_done = true;
    }
}

static void showcase_step() {
    if (!midi.isMounted() || midi.altSetting() != 1) return;

    uint32_t now = millis();
    if (g_show.cycle_start == 0 || (now - g_show.cycle_start) >= kCycleMs) {
        uint32_t prev = g_show.cycle_count;
        g_show = Showcase{};
        g_show.cycle_start = now;
        g_show.cycle_count = prev + 1;
    }
    uint32_t t = now - g_show.cycle_start;

    scene_pernote(t, now);
    scene_walk(t, now);
    scene_rpn_nrpn(t);
}

/*--------------------------------------------------------------------+
 * Arduino entry points
 *--------------------------------------------------------------------*/
void setup() {
    randomSeed((unsigned long)analogRead(A0));

    USBDevice.setID(0xCAFE, 0x40F1);
    USBDevice.setManufacturerDescriptor("github.com/sauloverissimo");
    USBDevice.setProductDescriptor("Nrf52840ProMicro");
    USBDevice.setSerialDescriptor("Nrf52840ProMicro-0001");

    usb_midi2.setStringDescriptor(kFbName);
    usb_midi2.begin();

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
}

void loop() {
    bool mounted = usb_midi2.mounted();
    midi.setMounted(mounted);
    midi.setAltSetting(mounted ? usb_midi2.altSetting() : 0);

    if (mounted) {
        uint32_t buf[16];
        for (;;) {
            uint32_t n = usb_midi2.read(buf, 16);
            if (n == 0) break;
            midi.feedRx(buf, n);
        }
    }

    midi.task();
    showcase_step();
}
