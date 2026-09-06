#pragma once
#include <Arduino.h>
#include "reader.h"

// Up to eight printers, which is what the account import can carry.
#define MAX_PRINTERS 8

enum PrinterType : uint8_t {
    PT_NONE = 0,
    PT_CREALITY  = 1,   // WebSocket :9999
    PT_FF_C5     = 2,   // HTTP :8898
    PT_BAMBU     = 3,   // MQTT/TLS :8883
    PT_SNAPMAKER = 4    // Moonraker WebSocket :7125
};

// One printer, as imported from the user's TigerTag account.
//
// The fixed sn/cc pair covers five of the six brands. It does NOT stretch to
// Anycubic, which needs a broker deviceId, a username, a password and a numeric
// model id that forms part of its MQTT topic. Before that backend is written
// this becomes a named credential bag the account layer fills and each backend
// reads by name - see docs/ACCOUNT-DATA.md.
struct PrinterCfg {
    PrinterType type = PT_NONE;
    String name;        // shown on the home screen
    String host;        // IP address on the LAN
    String sn;          // serial number (FlashForge, Bambu Lab)
    String cc;          // check code / access code (FlashForge, Bambu Lab, Elegoo)

    // Shown on the home screen. An account can hold ten printers while the
    // machine next to this box is one of them; hiding the rest is the
    // difference between a list you scan and a list you read.
    //
    // Defaults to true: a printer that appears in the account should appear on
    // the device, and a user who wants fewer says so once.
    bool visible = true;
};

// What the printer says is currently in one slot.
struct SlotState {
    bool    known = false;           // has the printer reported this slot yet?
    String  type;                    // "PLA", "PETG", ...
    String  brand;                   // "eSun", "Creality", ... empty if unknown
    uint8_t r = 90, g = 90, b = 90;  // grey until the printer says otherwise
    uint8_t percent = 0;
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
    virtual bool connected() = 0;

    virtual int  slotCount() = 0;                   // may change at runtime
    virtual const char* slotLabel(int i) = 0;       // "Ext", "1A", "T3", ...
    virtual const SlotState& slot(int i) = 0;
    virtual bool assign(int i, const TagInfo& t) = 0;

    virtual String status() = 0;
    // Ask the printer for its slot state again. Worth calling after assign():
    // at least one of these protocols acknowledges a command it ignored, so
    // re-reading is the only way to know what actually landed.
    virtual void refresh() = 0;
};
