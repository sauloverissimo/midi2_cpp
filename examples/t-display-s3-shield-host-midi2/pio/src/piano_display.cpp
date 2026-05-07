/*
 * piano_display.cpp, ST7789 piano roll + MIDI 2.0 message log for the
 * LilyGo T-Display S3 (Arduino / PlatformIO).
 *
 * Layout, top to bottom:
 *   y = 0..15    line 1, identity + status
 *   y = 16..31   line 2, device endpoint name + remote MUID
 *   y = 32..47   line 3, latest MIDI event (category-coloured tag +
 *                payload string)
 *   y = 48..59   line 4, per-category counters
 *   y = 60..71   line 5, view range + out-of-view triangles
 *   y = 72..89   spacer / divider
 *   y = 90..169  piano roll, 25 keys (15 white + 10 black)
 *
 * Display config: ST7789 1.9" 320x170 IPS, parallel 8-bit on
 * GPIO39..48 (D0..D7) + GP8 WR + GP9 RD + GP6 CS + GP7 RS/DC + GP5 RST
 * + GP38 backlight (PWM channel 7) + GP15 panel power gate.
 */
#include "piano_display.h"

#include <atomic>
#include <cstdio>
#include <cstring>

#include <Arduino.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// LGFX_USE_V1 is defined globally in platformio.ini build_flags.
#include <LovyanGFX.hpp>

namespace piano_display {

namespace {

// ---- Screen layout (landscape, 320x170 after rotation) --------------------
constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 170;

constexpr int LINE1_Y  = 2;     // identity + status        (Font 2, ~15px)
constexpr int LINE2_Y  = 18;    // device + MUID             (Font 2, ~15px)
constexpr int LINE3_Y  = 34;    // latest MIDI event         (Font 2, ~15px)
constexpr int LINE4_Y  = 52;    // counters                  (Font 1, ~10px)
constexpr int LINE5_Y  = 64;    // view range + triangles    (Font 1, ~8px)
constexpr int INFO_H   = 76;    // total info bar height

constexpr int PIANO_Y  = INFO_H;
constexpr int PIANO_H  = SCREEN_H - INFO_H;   // 94 px

// ---- Piano key geometry, 25 keys (C..C, 15 white) -------------------------
constexpr int KEYS_SPAN    = 25;
constexpr int WHITE_KEYS   = 15;
constexpr int WHITE_KEY_W  = 21;
constexpr int WHITE_KEY_H  = PIANO_H;
constexpr int BLACK_KEY_W  = 12;
constexpr int BLACK_KEY_H  = static_cast<int>(PIANO_H * 0.60f);
constexpr int PIANO_X      = (SCREEN_W - WHITE_KEYS * WHITE_KEY_W) / 2;

constexpr int VIEW_DEFAULT = 48;
constexpr int VIEW_MIN     = 0;
constexpr int VIEW_MAX     = 103;

// ---- Colours (RGB565) -----------------------------------------------------
constexpr uint16_t COL_WHITE_NORMAL  = 0xFFFF;
constexpr uint16_t COL_WHITE_ACTIVE  = 0x07FF;   // cyan
constexpr uint16_t COL_BLACK_NORMAL  = 0x0841;
constexpr uint16_t COL_BLACK_ACTIVE  = 0xFBE0;   // warm orange
constexpr uint16_t COL_KEY_BORDER    = 0x0000;
constexpr uint16_t COL_INFO_BG       = 0x0000;
constexpr uint16_t COL_DIVIDER       = 0x2945;
constexpr uint16_t COL_DIM           = 0x8410;
constexpr uint16_t COL_TEXT          = 0xFFFF;
constexpr uint16_t COL_ACCENT        = 0x07FF;   // cyan
constexpr uint16_t COL_HEADER        = 0xFFE0;   // yellow
constexpr uint16_t COL_OFF_BELOW     = 0xF800;   // red
constexpr uint16_t COL_OFF_ABOVE     = 0x001F;   // blue

// Per-category colours for the "latest event" tag.
constexpr uint16_t COL_CAT_NOTE_ON     = 0x07E0;   // green
constexpr uint16_t COL_CAT_NOTE_OFF    = 0x8410;   // grey
constexpr uint16_t COL_CAT_CC          = 0xFFE0;   // yellow
constexpr uint16_t COL_CAT_PB          = 0x07FF;   // cyan
constexpr uint16_t COL_CAT_CHNPRES     = 0xFD20;   // orange
constexpr uint16_t COL_CAT_POLYPRES    = 0xFB60;   // amber
constexpr uint16_t COL_CAT_PERNOTEPB   = 0xF81F;   // magenta
constexpr uint16_t COL_CAT_PERNOTECTRL = 0xC81F;   // purple
constexpr uint16_t COL_CAT_PROGRAM     = 0xAFE5;   // pale green
constexpr uint16_t COL_CAT_SYSEX       = 0x7BEF;   // grey-blue
constexpr uint16_t COL_CAT_FLEXDATA    = 0xFFFF;   // white
constexpr uint16_t COL_CAT_OTHER       = 0x8410;   // grey

// ---- Semitone lookup ------------------------------------------------------
constexpr int SEMITONE_TO_WHITE[12]    = {0,-1,1,-1,2,3,-1,4,-1,5,-1,6};
constexpr int BLACK_LEFT_NEIGHBOR[12]  = {-1,0,-1,2,-1,-1,5,-1,7,-1,9,-1};
const char* const NOTE_NAMES[12]       = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

// ---- LovyanGFX driver tied to the T-Display S3 wiring ---------------------
class TDisplayS3LCD : public lgfx::LGFX_Device {
public:
    TDisplayS3LCD() {
        {
            auto cfg = _bus.config();
            cfg.pin_wr = 8; cfg.pin_rd = 9; cfg.pin_rs = 7;
            cfg.pin_d0 = 39; cfg.pin_d1 = 40; cfg.pin_d2 = 41; cfg.pin_d3 = 42;
            cfg.pin_d4 = 45; cfg.pin_d5 = 46; cfg.pin_d6 = 47; cfg.pin_d7 = 48;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs   = 6;
            cfg.pin_rst  = 5;
            cfg.pin_busy = -1;
            cfg.offset_rotation = 1;
            cfg.offset_x = 35;
            cfg.readable    = false;
            cfg.invert      = true;
            cfg.rgb_order   = false;
            cfg.dlen_16bit  = false;
            cfg.bus_shared  = false;
            cfg.panel_width  = 170;
            cfg.panel_height = 320;
            _panel.config(cfg);
        }
        setPanel(&_panel);
        {
            auto cfg = _bl.config();
            cfg.pin_bl     = 38;
            cfg.invert     = false;
            cfg.freq       = 22000;
            cfg.pwm_channel = 7;
            _bl.config(cfg);
            _panel.setLight(&_bl);
        }
    }
private:
    lgfx::Bus_Parallel8 _bus;
    lgfx::Panel_ST7789  _panel;
    lgfx::Light_PWM     _bl;
};

// ---- State ----------------------------------------------------------------
TDisplayS3LCD g_lcd;
LGFX_Sprite   g_sprite(&g_lcd);
bool          g_initialised = false;

bool          g_active[128] = {};
int           g_view_start  = VIEW_DEFAULT;

std::atomic<uint32_t> g_count[12] = {};   // indexed by Category

constexpr size_t TXT_LEN = 64;
char            g_status[TXT_LEN]      = "init...";
char            g_dev_name[TXT_LEN]    = "";
uint32_t        g_dev_muid             = 0;
bool            g_dev_muid_valid       = false;
char            g_event_body[TXT_LEN]  = "";
Category        g_event_category       = Category::Other;

SemaphoreHandle_t g_str_mutex = nullptr;

const char* category_label(Category c) {
    switch (c) {
        case Category::NoteOn:      return "NOTE ON";
        case Category::NoteOff:     return "NOTE OFF";
        case Category::CC:          return "CC";
        case Category::PitchBend:   return "PITCH BEND";
        case Category::ChnPressure: return "CHN PRES";
        case Category::PolyPressure:return "POLY PRES";
        case Category::PerNotePB:   return "PER-NOTE PB";
        case Category::PerNoteCtrl: return "PER-NOTE CTL";
        case Category::Program:     return "PROGRAM";
        case Category::SysEx:       return "SYSEX";
        case Category::FlexData:    return "FLEX DATA";
        case Category::Other:       return "OTHER";
    }
    return "?";
}

uint16_t category_colour(Category c) {
    switch (c) {
        case Category::NoteOn:      return COL_CAT_NOTE_ON;
        case Category::NoteOff:     return COL_CAT_NOTE_OFF;
        case Category::CC:          return COL_CAT_CC;
        case Category::PitchBend:   return COL_CAT_PB;
        case Category::ChnPressure: return COL_CAT_CHNPRES;
        case Category::PolyPressure:return COL_CAT_POLYPRES;
        case Category::PerNotePB:   return COL_CAT_PERNOTEPB;
        case Category::PerNoteCtrl: return COL_CAT_PERNOTECTRL;
        case Category::Program:     return COL_CAT_PROGRAM;
        case Category::SysEx:       return COL_CAT_SYSEX;
        case Category::FlexData:    return COL_CAT_FLEXDATA;
        case Category::Other:       return COL_CAT_OTHER;
    }
    return COL_TEXT;
}

const char* category_short(Category c) {
    switch (c) {
        case Category::NoteOn:      return "On";
        case Category::NoteOff:     return "Off";
        case Category::CC:          return "CC";
        case Category::PitchBend:   return "PB";
        case Category::ChnPressure: return "ChP";
        case Category::PolyPressure:return "PoP";
        case Category::PerNotePB:   return "PNPB";
        case Category::PerNoteCtrl: return "PNC";
        case Category::Program:     return "Pgm";
        case Category::SysEx:       return "Sx";
        case Category::FlexData:    return "Fx";
        case Category::Other:       return "Ot";
    }
    return "?";
}

void auto_view_to(int midi) {
    int desired = (midi / 12) * 12 - 12;
    if (desired < VIEW_MIN) desired = VIEW_MIN;
    if (desired > VIEW_MAX) desired = VIEW_MAX;
    if (desired == g_view_start) return;
    g_view_start = desired;
}

void take_strings(char* out_status, char* out_dev,
                  uint32_t& out_muid, bool& out_muid_valid,
                  char* out_event, Category& out_cat) {
    // Block on the mutex (5 ms is plenty; set_* holders only memcpy a
    // ~64 byte string under the lock). The previous trylock + lockless
    // fallback was a race: under MIDI bursts the render task ended up
    // reading partially-written strings and the screen visibly flickered.
    if (g_str_mutex && xSemaphoreTake(g_str_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        std::strncpy(out_status, g_status, TXT_LEN); out_status[TXT_LEN-1] = 0;
        std::strncpy(out_dev,    g_dev_name, TXT_LEN); out_dev[TXT_LEN-1] = 0;
        out_muid = g_dev_muid;
        out_muid_valid = g_dev_muid_valid;
        std::strncpy(out_event,  g_event_body, TXT_LEN); out_event[TXT_LEN-1] = 0;
        out_cat = g_event_category;
        xSemaphoreGive(g_str_mutex);
    } else {
        // Mutex contention rare path. Render with empty strings rather
        // than read torn ones; next frame will pick up valid data.
        out_status[0] = 0;
        out_dev[0]    = 0;
        out_muid      = 0;
        out_muid_valid = false;
        out_event[0]  = 0;
        out_cat       = Category::Other;
    }
}

void draw_piano(LGFX_Sprite& s) {
    s.fillRect(0, PIANO_Y, SCREEN_W, PIANO_H, COL_KEY_BORDER);

    for (int n = g_view_start; n < g_view_start + KEYS_SPAN; n++) {
        int st = n % 12;
        if (SEMITONE_TO_WHITE[st] < 0) continue;
        int wi = ((n - g_view_start) / 12) * 7 + SEMITONE_TO_WHITE[st];
        if (wi < 0 || wi >= WHITE_KEYS) continue;
        int x = PIANO_X + wi * WHITE_KEY_W;
        uint16_t col = g_active[n] ? COL_WHITE_ACTIVE : COL_WHITE_NORMAL;
        s.fillRect(x, PIANO_Y + 1, WHITE_KEY_W - 1, WHITE_KEY_H - 2, col);
        s.drawFastHLine(x, PIANO_Y + WHITE_KEY_H - 2, WHITE_KEY_W - 1, COL_KEY_BORDER);
    }
    for (int n = g_view_start; n < g_view_start + KEYS_SPAN; n++) {
        int st = n % 12;
        if (SEMITONE_TO_WHITE[st] >= 0) continue;
        int nbSt = BLACK_LEFT_NEIGHBOR[st];
        int nbNote = (n / 12) * 12 + nbSt;
        int nbWi = ((nbNote - g_view_start) / 12) * 7 + SEMITONE_TO_WHITE[nbSt];
        int x = PIANO_X + nbWi * WHITE_KEY_W + WHITE_KEY_W - BLACK_KEY_W / 2;
        uint16_t col = g_active[n] ? COL_BLACK_ACTIVE : COL_BLACK_NORMAL;
        s.fillRect(x, PIANO_Y + 1, BLACK_KEY_W, BLACK_KEY_H, col);
        s.drawRect(x, PIANO_Y + 1, BLACK_KEY_W, BLACK_KEY_H, COL_KEY_BORDER);
    }
}

void draw_info_bar(LGFX_Sprite& s) {
    s.fillRect(0, 0, SCREEN_W, INFO_H, COL_INFO_BG);
    s.drawFastHLine(0, INFO_H - 1, SCREEN_W, COL_DIVIDER);

    char status_copy[TXT_LEN];
    char dev_copy[TXT_LEN];
    uint32_t muid;
    bool muid_valid;
    char event_copy[TXT_LEN];
    Category cat;
    take_strings(status_copy, dev_copy, muid, muid_valid, event_copy, cat);

    // Line 1, identity + status. Identity left, "MIDI 2.0 Host" middle dim,
    // status right (ACCENT colour). A small heartbeat dot at the very top
    // right pulses every 500 ms so the user can see the firmware is alive
    // even when nothing else on the bar is changing.
    s.setFont(&fonts::Font2);
    s.setTextColor(COL_HEADER, COL_INFO_BG);
    s.drawString("TDisplayS3", 4, LINE1_Y);
    s.setTextColor(COL_DIM, COL_INFO_BG);
    s.drawString("MIDI 2.0 Host", 100, LINE1_Y);
    if (status_copy[0]) {
        s.setTextColor(COL_ACCENT, COL_INFO_BG);
        int tw = s.textWidth(status_copy);
        int x = SCREEN_W - tw - 14;       // leave room for the heartbeat dot
        if (x < 200) x = 200;
        s.drawString(status_copy, x, LINE1_Y);
    }
    {
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        bool beat = ((now_ms / 500) & 1u) == 0;
        uint16_t beat_col = beat ? 0x07E0 /*green*/ : 0x0320 /*dim green*/;
        s.fillCircle(SCREEN_W - 6, LINE1_Y + 6, 4, beat_col);
    }

    // Line 2, device endpoint name + remote MUID.
    s.setFont(&fonts::Font2);
    s.setTextColor(COL_DIM, COL_INFO_BG);
    s.drawString("Dev:", 4, LINE2_Y);
    s.setTextColor(COL_TEXT, COL_INFO_BG);
    if (dev_copy[0]) {
        s.drawString(dev_copy, 38, LINE2_Y);
    } else {
        s.setTextColor(COL_DIM, COL_INFO_BG);
        s.drawString("(none)", 38, LINE2_Y);
    }
    if (muid_valid) {
        char muid_buf[24];
        std::snprintf(muid_buf, sizeof(muid_buf), "MUID 0x%07X",
                      (unsigned)(muid & 0x0FFFFFFFu));
        s.setTextColor(COL_DIM, COL_INFO_BG);
        int tw = s.textWidth(muid_buf);
        s.drawString(muid_buf, SCREEN_W - tw - 4, LINE2_Y);
    }

    // Line 3, latest MIDI event. Tag (category-coloured) + body.
    s.setFont(&fonts::Font2);
    if (event_copy[0]) {
        const char* tag = category_label(cat);
        uint16_t tag_col = category_colour(cat);
        s.setTextColor(tag_col, COL_INFO_BG);
        s.drawString(tag, 4, LINE3_Y);
        int tag_w = s.textWidth(tag);
        s.setTextColor(COL_TEXT, COL_INFO_BG);
        s.drawString(event_copy, 4 + tag_w + 6, LINE3_Y);
    } else {
        s.setTextColor(COL_DIM, COL_INFO_BG);
        s.drawString("waiting for MIDI traffic...", 4, LINE3_Y);
    }

    // Line 4, counters. Compact one-line summary across all categories.
    s.setFont(&fonts::Font0);
    s.setTextColor(COL_DIM, COL_INFO_BG);
    char buf[160];
    int n = std::snprintf(buf, sizeof(buf),
                          "On %lu Off %lu CC %lu PB %lu ChP %lu PoP %lu PNPB %lu PNC %lu Pgm %lu Sx %lu Fx %lu",
                          (unsigned long)g_count[(int)Category::NoteOn].load(),
                          (unsigned long)g_count[(int)Category::NoteOff].load(),
                          (unsigned long)g_count[(int)Category::CC].load(),
                          (unsigned long)g_count[(int)Category::PitchBend].load(),
                          (unsigned long)g_count[(int)Category::ChnPressure].load(),
                          (unsigned long)g_count[(int)Category::PolyPressure].load(),
                          (unsigned long)g_count[(int)Category::PerNotePB].load(),
                          (unsigned long)g_count[(int)Category::PerNoteCtrl].load(),
                          (unsigned long)g_count[(int)Category::Program].load(),
                          (unsigned long)g_count[(int)Category::SysEx].load(),
                          (unsigned long)g_count[(int)Category::FlexData].load());
    (void)n;
    s.drawString(buf, 4, LINE4_Y);

    // Line 5, view range + out-of-view triangles.
    int viewEnd = g_view_start + KEYS_SPAN - 1;
    char range[24];
    std::snprintf(range, sizeof(range), "%s%d-%s%d",
                  NOTE_NAMES[g_view_start % 12], (g_view_start / 12) - 1,
                  NOTE_NAMES[viewEnd       % 12], (viewEnd       / 12) - 1);
    s.setFont(&fonts::Font0);
    s.setTextColor(COL_DIM, COL_INFO_BG);
    s.drawString(range, 4, LINE5_Y);

    bool below = false, above = false;
    for (int nn = 0; nn < 128; nn++) {
        if (!g_active[nn]) continue;
        if (nn < g_view_start)              below = true;
        if (nn >= g_view_start + KEYS_SPAN) above = true;
    }
    if (below) {
        s.fillRoundRect(SCREEN_W - 30, LINE5_Y - 2, 12, 12, 2, COL_OFF_BELOW);
        s.setFont(&fonts::Font0);
        s.setTextColor(COL_TEXT, COL_OFF_BELOW);
        s.drawString("<", SCREEN_W - 27, LINE5_Y);
    }
    if (above) {
        s.fillRoundRect(SCREEN_W - 14, LINE5_Y - 2, 12, 12, 2, COL_OFF_ABOVE);
        s.setFont(&fonts::Font0);
        s.setTextColor(COL_TEXT, COL_OFF_ABOVE);
        s.drawString(">", SCREEN_W - 11, LINE5_Y);
    }
}

}  // namespace

void init() {
    if (g_initialised) return;

    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);

    g_lcd.init();
    g_lcd.setRotation(2);
    g_lcd.setBrightness(255);
    g_lcd.fillScreen(0x0000);

    g_sprite.setColorDepth(16);
    // Internal SRAM for the sprite, not PSRAM. PSRAM Octal is shared with
    // the ESP-IDF USB Host stack DMA buffers (the ESP32_Host_MIDI library
    // pulls UMP packets through it); under live USB traffic that
    // contention lets the pushSprite read with variable latency and the
    // panel tears visibly. SRAM has plenty of headroom for a 320x170
    // 16bpp sprite (~108 KB) and is not bus-shared with the host driver.
    g_sprite.setPsram(false);
    if (!g_sprite.createSprite(SCREEN_W, SCREEN_H)) {
        Serial.println("[piano] sprite alloc in SRAM failed (320x170 16bpp = ~108 KB), falling back to PSRAM");
        g_sprite.setPsram(true);
        if (!g_sprite.createSprite(SCREEN_W, SCREEN_H)) {
            Serial.println("[piano] sprite alloc failed in PSRAM too, giving up");
            return;
        }
    }
    g_sprite.setTextDatum(lgfx::top_left);

    g_str_mutex = xSemaphoreCreateMutex();

    g_initialised = true;
    Serial.println("[piano] ST7789 320x170 + sprite ready");
}

void set_note_active(uint8_t note, bool active) {
    if (note >= 128) return;
    g_active[note] = active;

    if (active) {
        if (note < g_view_start || note >= g_view_start + KEYS_SPAN) {
            auto_view_to(note);
        }
    }
}

void bump_counter(Category c) {
    g_count[(int)c].fetch_add(1, std::memory_order_relaxed);
}

void set_status(const char* text) {
    if (text == nullptr) text = "";
    if (g_str_mutex && xSemaphoreTake(g_str_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        std::strncpy(g_status, text, TXT_LEN);
        g_status[TXT_LEN-1] = '\0';
        xSemaphoreGive(g_str_mutex);
    }
}

void set_device_info(const char* endpoint_name, uint32_t muid, bool muid_valid) {
    if (g_str_mutex && xSemaphoreTake(g_str_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        if (endpoint_name && endpoint_name[0]) {
            std::strncpy(g_dev_name, endpoint_name, TXT_LEN);
            g_dev_name[TXT_LEN-1] = '\0';
        } else {
            g_dev_name[0] = '\0';
        }
        g_dev_muid       = muid;
        g_dev_muid_valid = muid_valid;
        xSemaphoreGive(g_str_mutex);
    }
}

void set_last_event(Category c, const char* body) {
    if (body == nullptr) body = "";
    if (g_str_mutex && xSemaphoreTake(g_str_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        g_event_category = c;
        std::strncpy(g_event_body, body, TXT_LEN);
        g_event_body[TXT_LEN-1] = '\0';
        xSemaphoreGive(g_str_mutex);
    }
    // Suppress unused-warning when category_short is not referenced elsewhere.
    (void)category_short;
}

void clear_active_notes() {
    for (int n = 0; n < 128; ++n) g_active[n] = false;
}

void render_frame() {
    if (!g_initialised) return;
    draw_piano(g_sprite);
    draw_info_bar(g_sprite);
    g_lcd.startWrite();
    g_sprite.pushSprite(0, 0);
    g_lcd.endWrite();
}

}  // namespace piano_display
