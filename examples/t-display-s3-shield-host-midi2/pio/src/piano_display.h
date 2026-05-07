/*
 * piano_display.h, public API for the on-board ST7789 piano roll +
 * MIDI 2.0 message log on the LilyGo T-Display S3 (Arduino /
 * PlatformIO build).
 *
 * Layout:
 *   info bar (top, ~90px)
 *     line 1, identity (TDisplayS3 | MIDI 2.0 Host) + status
 *     line 2, device (EndpointName + MUID)
 *     line 3, latest MIDI event (category-coloured tag + payload)
 *     line 4, per-category counters (On/Off/CC/PB/ChnPres/PolyPres/PNPB/PNC/Pgm/Sx/Fx/Oth)
 *     line 5, range label + out-of-view triangles
 *   piano roll (bottom, ~80px), 25 keys, 15 white + 10 black,
 *     auto-shifts octave when a note arrives outside the current view.
 *
 * Thread safety: every set_* and bump_* call is safe to call from any
 * task; the active-note buffer is plain bool[128] (atomic byte writes
 * on Xtensa), counters are std::atomic, the status / event / device
 * strings are short and protected by a small mutex.
 */
#pragma once

#include <cstdint>

namespace piano_display {

// Categories. Match the m2host typed callbacks 1:1 plus an "Other"
// catch-all. The render layer maps each category to a colour for the
// "latest event" line.
enum class Category : uint8_t {
    NoteOn,
    NoteOff,
    CC,
    PitchBend,
    ChnPressure,
    PolyPressure,
    PerNotePB,
    PerNoteCtrl,
    Program,
    SysEx,
    FlexData,
    Other,
};

void init();

// Mark a MIDI note (0..127) as currently active or released. The piano
// auto-shifts octave to bring the note into view if it would fall
// outside the current 25-key window.
void set_note_active(uint8_t note, bool active);

// Increment the matching counter shown on the counters line.
void bump_counter(Category c);

// Update the short status string in the top-right of line 1.
// Limited to ~32 chars.
void set_status(const char* text);

// Update the device-side identity line (line 2). Pass nullptr / empty
// to clear (e.g. after disconnect). MUID is the device-discovered
// remote MUID once CI Discovery has completed (28-bit).
void set_device_info(const char* endpoint_name, uint32_t muid, bool muid_valid);

// Update the latest-event line (line 3). The category drives the colour
// of the leading tag; the body is a free-form string built by main.cpp
// (e.g. "ch=1 note=60 vel=0xFFFF (16-bit)").
void set_last_event(Category c, const char* body);

// Clear all currently-lit notes (used on disconnect to drop stuck keys).
void clear_active_notes();

// Render one full frame to the display. Call from a dedicated task at
// the desired refresh rate; ~60 fps is comfortable.
void render_frame();

}  // namespace piano_display
