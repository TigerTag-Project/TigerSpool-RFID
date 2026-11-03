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
// The limit was never a NUMBER of printers. It is internal RAM, and every
// printer takes the load slots its connection actually costs - one is one
// kilobyte - and the device refuses the one that would not fit, at the moment
// somebody switches it on and can see why, rather than leaving it red later
// with no explanation.
//
// HOW THE NUMBERS ARE SET. A printer's load slots are the WORST cost measured
// for it, rounded up to the next kilobyte - a fact about the connection, not a
// guess. The safety margin lives once, in the budget, not spread across brands.
//
// TLS IN PSRAM CHANGED EVERY NUMBER. Until 2026-10-05 mbedTLS allocated in
// internal RAM, and a TLS printer cost 40-50 KB of it: three TLS sessions was
// the ceiling, and the presence beat beside them failed its handshake.
// tls::usePsram() moves those buffers to PSRAM; a TLS printer now costs about
// what a WebSocket does. /api/memtest?all=1, 2026-10-05, bench board, every
// link closed, opened one at a time, read after ten settled seconds - one
// pass, all eight up at the end with 149 KB still free:
//
//   first Bambu in cloud mode    8.3 KB  ->  9   was 50
//   each further cloud Bambu     5.2 KB  ->  6   was 4
//   Bambu in LAN mode            5.3 KB  ->  6   was 50, never measured before
//   first Anycubic in cloud      5.3 KB  ->  6   was 50
//   Creality                     4.5 KB  ->  6   kept: 5.6 KB measured before
//   Snapmaker                    4.7 KB  ->  5
//   Elegoo                       0.7 KB  ->  3   kept: 2.5 KB measured before
//   FlashForge                   0.1 KB  ->  2   kept: 2.0 KB measured before
//
// Two figures are still reasoning, not readings: an Anycubic in LAN mode and a
// further cloud Anycubic. Both are charged as the TLS printer measured beside
// them; the first bench with one switched on should replace them.
//
// THE BUDGET. 183 KB of internal RAM free with no link open; 32 KB is the
// survival floor, which leaves about 151. 120 load slots keeps about 30 KB for
// what this model does not count - a web page served, a JSON document parsed,
// a one-off TLS request's 16 KB task stack (its buffers are in PSRAM now). The
// reserve is wide on purpose: these numbers come from one pass, not five.
//
// In practice the binding limit is now the link count, MAX_LINKS in main.cpp:
// ten links of typical printers are about 60 load slots.
//
// A brand missing from loadSlotsFor() costs 0 and is never refused: a new
// backend is not finished until it is measured and listed here.
namespace budget {

constexpr uint16_t LOAD_SLOTS = 120;

// Load slots for one printer. `sharesCloud` is true when a cloud printer of the
// same brand has already been counted, so this one rides on its session.
inline uint16_t loadSlotsFor(const PrinterCfg& p, bool sharesCloud) {
    switch (p.type) {
        case PT_BAMBU:     return p.cloud ? (sharesCloud ? 6 : 9) : 6;
        // Like Bambu, every cloud Anycubic of an account shares one session -
        // the broker allows one per account - so only the first pays for it.
        // The first cloud one is measured (5.3 KB). A further cloud one and a
        // LAN one are NOT: no bench account has had either. Both are charged as
        // the TLS printers measured beside them - a further printer adds a
        // subscription, a LAN one opens the same TLS client a LAN Bambu does.
        // Replace them with readings.
        case PT_ANYCUBIC:  return 6;
        case PT_SNAPMAKER: return 5;
        // Same client and server as Snapmaker; NOT measured - no Klipper printer
        // on the bench. Replace with a reading.
        case PT_KLIPPER:   return 5;
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
    // One flag per brand: a Bambu cloud session carries no Anycubic.
    bool bambuCloud = false, acuCloud = false;
    for (int i = 0; i < n; i++) {
        if (p[i].type == PT_NONE) continue;
        bool on = p[i].visible;
        if (i == flip) on = !on;
        if (!on && i != selected) continue;
        const bool isAcu = p[i].type == PT_ANYCUBIC;
        sum += loadSlotsFor(p[i], isAcu ? acuCloud : bambuCloud);
        if (p[i].cloud) (isAcu ? acuCloud : bambuCloud) = true;
    }
    return sum;
}

}  // namespace budget
