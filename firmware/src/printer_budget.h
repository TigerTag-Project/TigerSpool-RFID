#pragma once
#include "printer.h"

// How many printers the device can hold open, counted in LOAD SLOTS - like
// seats on a bus.
//
// "Load slot", never plain "slot": in this code a slot is a place a spool sits,
// an AMS tray or a station bay (SlotState, slotCount). One load slot is one
// kilobyte of internal RAM a printer's connection occupies. The full reasoning
// and every measurement behind these numbers is in docs/CONNECTION-BUDGET.md.
//
// The limit was never a NUMBER of printers. It is internal RAM, and a printer's
// share of it depends almost entirely on whether it speaks TLS: /api/memtest
// measured 40-48 KB for a Bambu or an Anycubic and 2-6 KB for everything else.
// Ten Elegoos fit where three Bambus do not. So instead of a count, every
// printer takes the load slots its connection actually costs - one is one
// kilobyte - and the device refuses the one that would not fit, at the moment
// somebody switches it on and can see why, rather than leaving it red later
// with no explanation.
//
// HOW THE NUMBERS ARE SET. A printer's load slots are the WORST cost measured
// for it, rounded up to the next kilobyte - a fact about the connection, not a
// guess.
// The safety margin is not spread across the brands; it lives once, in the
// budget. (It used to be spread, "rounded up by about a fifth", and that was
// not true: three brands sat at or above their count, and it only went
// unnoticed because the first Bambu over-counted by enough to hide them.)
//
// Five passes of /api/memtest on 2026-09-10, every link closed and the account
// sync held off, printers opened one at a time and read after ten settled
// seconds. Worst case per connection, internal RAM:
//
//   first Bambu in cloud mode   48.5 KB  -> 50   opens the TLS session every
//   each further cloud Bambu     3.6 KB  ->  4   cloud Bambu shares
//   Anycubic                    40.4 KB  -> 41   TLS
//   Creality                     5.6 KB  ->  6   a WebSocket
//   Snapmaker                    4.9 KB  ->  5   a WebSocket
//   Elegoo                       2.5 KB  ->  3   plain MQTT, no TLS
//   FlashForge                   2.0 KB  ->  2   HTTP, nothing held
//   Bambu in LAN mode           NOT MEASURED -> 50
//
// The LAN Bambu is the one assumption left: both on the bench were switched
// off at every pass. It is charged as one TLS session because that is what it
// opens - the same client, the same broker protocol - but that is reasoning,
// not a reading, and the first bench with one switched on should replace it.
//
// THE BUDGET. The device starts with 198-203 KB of internal RAM free; 32 KB is
// the survival floor, which leaves about 166. 160 load slots keeps 6 KB of that
// for what this model does not count - a web page being served, a JSON document
// being parsed. It was 150, with 16 KB kept; it was raised to 160 on purpose,
// trading ten kilobytes of that reserve for room for one more light printer or
// a LAN Bambu beside a full bench. The reserve is thin now, and that is safe
// only because it is not what stops a crash: the 32 KB floor and the sustained
// low-memory closer in main.cpp are. At worst a peak closes a background link
// for a moment.
//
// Checked against the aggregate as well as per brand: six printers on the
// bench are 109 load slots here, and measured 103.2 KB.
//
// WHAT IT DOES NOT SEE: fragmentation. A new TLS session needs one piece of
// about 40 KB, and after three sessions the largest free block has been seen as
// low as 18 KB. 160 load slots caps TLS sessions at three, which is also the
// ceiling observed: the cheapest four TLS sessions - one cloud Bambu and three
// Anycubics - come to 173, over the budget before a single light printer. The
// two agree by arithmetic rather than by design. A one-off TLS request - an
// account sync, an update check - is kept possible by the stand-down in
// main.cpp, not by this budget.
//
// A brand missing from loadSlotsFor() costs 0 and is never refused: a new
// backend is not finished until it is measured and listed here.
namespace budget {

constexpr uint16_t LOAD_SLOTS = 160;

// Load slots for one printer. `sharesCloud` is true when a cloud Bambu has
// already been counted, so this one rides on its session.
inline uint16_t loadSlotsFor(const PrinterCfg& p, bool sharesCloud) {
    switch (p.type) {
        case PT_BAMBU:     return p.cloud ? (sharesCloud ? 4 : 50) : 50;
        case PT_ANYCUBIC:  return 41;
        case PT_SNAPMAKER: return 5;
        case PT_CREALITY:  return 6;
        case PT_ELEGOO:    return 3;
        case PT_FF_C5:     return 2;
        default:           return 0;
    }
}

// Load slots taken by the printers that get a link: every one switched on, plus
// the selected one, which is linked whether it is switched on or not.
//
// `flip` asks "and if this one were toggled" - the question a switch asks
// before it is allowed to move. -1 for the current state.
inline uint16_t used(const PrinterCfg* p, int n, int selected, int flip = -1) {
    uint16_t sum = 0;
    bool cloudCounted = false;
    for (int i = 0; i < n; i++) {
        if (p[i].type == PT_NONE) continue;
        bool on = p[i].visible;
        if (i == flip) on = !on;
        if (!on && i != selected) continue;
        sum += loadSlotsFor(p[i], cloudCounted);
        if (p[i].type == PT_BAMBU && p[i].cloud) cloudCounted = true;
    }
    return sum;
}

}  // namespace budget
