#pragma once
#include "printer.h"

// Elegoo Centauri Carbon and family over the LAN.
//
// MQTT on port 1883 with NO TLS at all - the simplest transport of the six
// brands. Authentication is a fixed username, "elegoo", and a password that is
// "123456" on a printer nobody has changed it on; the serial number forms every
// topic. Both come from the TigerTag account.
//
// Two protocols in one printer, and which one applies depends on a cable. With
// the Canvas multi-filament hub plugged in, four trays are read with method
// 2005 and written with 2003. With it unplugged, the printer reports a single
// spool through 1061 and is written through 1055 - and 2003 answers
// error_code 1003 rather than doing anything. The backend tracks which is live
// and routes each write accordingly.
//
// Slots follow the layout the rest of this firmware uses: index 0 is the
// external spool - here the mono extruder - and 1..4 are the Canvas trays,
// labelled S1..S4 as Elegoo's own interface and Tiger Studio both label them.
class ElegooBackend : public PrinterBackend {
public:
    void begin(const PrinterCfg& cfg) override;
    void loop() override;
    void stop() override;
    bool connected() override;
    int  slotCount() override;
    const char* slotLabel(int i) override;
    const SlotState& slot(int i) override;
    bool assign(int i, const TagInfo& t) override;
    String status() override;
    void refresh() override;
};
