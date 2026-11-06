#pragma once
#include <Arduino.h>
#include "reader.h"

// How many printers the device carries from the account.
//
// It was eight, and eight was wrong for a reason that only shows up on a real
// account: the import walks the brands in a fixed order and stops when it
// is full, so an account with more printers than this does not lose a random
// eight - it loses the LAST BRANDS ALPHABETICALLY IN THAT LIST, every time.
// Elegoo and Anycubic are last. A user who owned one of each would have found
// the two newest backends supporting printers that never appeared.
//
// Twenty-four covers every account anyone has, and the cost is bounded: an
// unused entry is seven empty Strings and an enum, and the NVS keys for it are
// never written. What is NOT free is the settings list, which draws them all -
// it scrolls, and that is the whole reason it scrolls.
#define MAX_PRINTERS 24

enum PrinterType : uint8_t {
    PT_NONE = 0,
    PT_CREALITY  = 1,   // WebSocket :9999
    PT_FF_C5     = 2,   // HTTP :8898
    PT_BAMBU     = 3,   // MQTT/TLS :8883
    PT_SNAPMAKER = 4,   // Moonraker WebSocket :7125
    PT_ELEGOO    = 5,   // MQTT :1883, no TLS
    PT_ANYCUBIC  = 6,   // MQTT/TLS :9883, self-signed
    PT_KLIPPER   = 7    // Moonraker WebSocket :7125 unless the account names a port
};

// One printer, as imported from the user's TigerTag account.
//
// The sn/cc pair covers five brands. Anycubic needed three more: its broker
// wants a device id and a username alongside the password, and its MQTT topics
// carry the printer's numeric model id. They are named fields rather than
// another overloaded pair, because the sixth brand proved the pair does not
// generalise - and a seventh will want something else again.
//
// Every one of these comes from the TigerTag account. None of them can be read
// off an Anycubic printer: /info exposes neither, and they cannot be derived
// from what it does expose. They exist only in AnycubicSlicerNext's config,
// which Tiger Studio decodes on the desktop. A printer never paired there has
// no credentials anywhere, and no amount of work on this device changes that.
struct PrinterCfg {
    PrinterType type = PT_NONE;
    String name;        // shown on the home screen
    String host;        // IP address on the LAN
    String sn;          // serial number (FlashForge, Bambu Lab)
    String cc;          // check code / access code / broker password / Moonraker API key
    // Reachable only through the maker's cloud, not on this network. Bambu
    // Lab's cloud is READ-ONLY by design: the broker accepts a report
    // subscription and refuses the commands that set a tray, so a slot on such
    // a printer can be shown and must not be offered as something to write.
    bool   cloud = false;
    String devId;       // Anycubic: 32-hex broker device id, part of its topics
    String user;        // Anycubic: broker username
    String model;       // Anycubic: numeric model id, also part of its topics

    // Shown on the home screen. An account can hold ten printers while the
    // machine next to this box is one of them; hiding the rest is the
    // difference between a list you scan and a list you read.
    //
    // Defaults to true: a printer that appears in the account should appear on
    // the device, and a user who wants fewer says so once.
    bool visible = true;
};

// A printer this device can read and must not write: a Bambu Lab in cloud mode,
// whose broker refuses the tray commands. An Anycubic in cloud mode is not one
// of them - its cloud broker takes the same writes as its LAN one.
inline bool readOnly(const PrinterCfg& p) { return p.cloud && p.type == PT_BAMBU; }

// A host may carry its port, "192.168.1.50:7126": Tiger Studio stores a
// Klipper printer's non-default Moonraker port inside `ip` that way, and
// several Moonraker instances on one host differ only by it.
inline String hostOnly(const String& host) {
    const int c = host.lastIndexOf(':');
    return (c > 0 && host.indexOf(':') == c) ? host.substring(0, c) : host;
}
inline uint16_t portOf(const String& host, uint16_t fallback) {
    const int c = host.lastIndexOf(':');
    if (c <= 0 || host.indexOf(':') != c) return fallback;
    const long p = host.substring(c + 1).toInt();
    return (p > 0 && p < 65536) ? (uint16_t)p : fallback;
}

// What the printer says is currently in one slot.
struct SlotState {
    bool    known = false;           // has the printer reported this slot yet?
    String  type;                    // "PLA", "PETG", ...
    String  brand;                   // "eSun", "Creality", ... empty if unknown
    uint8_t r = 90, g = 90, b = 90;  // grey until the printer says otherwise
    uint8_t percent = 0;
    // `percent` is a real figure, not the default 0. Only then does the slot
    // grid fill the colour block to that level.
    bool    percentKnown = false;
    // Filament left on the spool, in grams, as the printer reports it; -1 when
    // it reports none. Drawn inside the colour block when known.
    int16_t grams = -1;
    bool    selected = false;        // loaded / active in the printer
};

// The one interface every printer speaks.
//
// It has held across four protocols - MQTT over TLS, HTTP polling and two
// different WebSocket dialects - without changing, which is the strongest
// evidence available that the split is in the right place.
//
// Slots are a flat, 0-based list here. Real printers disagree about addressing:
// Creality has box+slot pairs, Bambu Lab has AMS unit + tray with sentinel
// values for the external spool, FlashForge has 1-based ids, Snapmaker has
// extruder indices, Anycubic has a four-slot box numbered -1. Every one of
// those mappings stays inside its own backend, and the UI never learns the
// difference.
class PrinterBackend {
public:
    virtual ~PrinterBackend() {}

    virtual void begin(const PrinterCfg& cfg) = 0;
    virtual void loop() = 0;                        // pump the transport
    virtual void stop() {}                          // close connections, free heap
    // Is this the printer the user is looking at?
    //
    // Only the foreground backend needs a buffer sized for the worst case: a
    // Bambu pushall runs to about 50 KB on an X1 with four AMS units, and only
    // the printer on screen has its slots drawn. Background links get a small
    // one. Measured, the saving is in PSRAM rather than internal RAM - buffers
    // over 4 KB are placed there by the framework - so it matters far less than
    // it was first thought to; it stays because 42 KB of PSRAM per printer is
    // still 42 KB. Backends with nothing to size ignore this.
    virtual void setForeground(bool) {}
    virtual bool connected() = 0;

    virtual int  slotCount() = 0;                   // may change at runtime
    virtual const char* slotLabel(int i) = 0;       // "Ext", "1A", "T3", ...
    virtual const SlotState& slot(int i) = 0;
    virtual bool assign(int i, const TagInfo& t) = 0;
    // The last assign() succeeded but left the slot's weight as it was, because
    // the figure it needed did not arrive in time. A user who sees only a green
    // tick believes the weight moved too. False for every backend that sends no
    // weight at all - there is nothing there to have missed.
    virtual bool weightSkipped() { return false; }
    // The grams the last assign() wrote to the slot, for the result screen to
    // show; -1 when it wrote none.
    virtual int sentGrams() { return -1; }
    // Where the next assign() takes the spool's weight from: the account (the
    // figure a TigerScale keeps up to date) or the chip (what was written on
    // it, but needs no network). The user's choice, held by main.cpp and handed
    // over before every assign. Ignored by backends that send no weight.
    virtual void setWeightFromCloud(bool) {}

    // Is slot 0 the printer's single external spool, drawn on a row of its
    // own? True for every brand that has one - and false for Anycubic, whose
    // box -1 is a four-slot unit rather than one spool, so splitting its first
    // slot off leaves one cell alone above three.
    virtual bool firstIsExternal() { return true; }

    virtual String status() = 0;
    // Ask the printer for its slot state again. Worth calling after assign():
    // at least one of these protocols acknowledges a command it ignored, so
    // re-reading is the only way to know what actually landed.
    virtual void refresh() = 0;
};
