#pragma once
#include "printer.h"

// Anycubic Kobra 3 V2 / Kobra X and their ACE units, over the LAN.
//
// MQTT on port 9883 with TLS against a self-signed certificate. Neither the
// credentials nor the topics can be discovered from the printer: /info on port
// 18910 exposes a rotating token and a dynamic code and nothing else, and the
// broker's device id, username and password exist only in AnycubicSlicerNext's
// own config file. Tiger Studio decodes them on the desktop and writes them to
// the TigerTag account, which is where this reads them. A printer that was
// never paired in that slicer cannot be reached by anything here.
//
// A printer is in LAN mode OR cloud mode, never both, and a cloud-mode printer
// opens no local port at all. This backend is the LAN half; the cloud half is a
// different service with signed requests and a different broker.
//
// Slots are discovered from the printer's own layout report. Box -1 is the
// external unit and is NOT one spool - an ACE Pro 2 reports it with four slots
// - so it is drawn as a unit like any other. Boxes 0..N are ACE units. Labels
// are A1..A4 for the first, B1..B4 for the second, matching Tiger Studio.
class AnycubicBackend : public PrinterBackend {
public:
    void begin(const PrinterCfg& cfg) override;
    void loop() override;
    void stop() override;
    bool connected() override;
    bool firstIsExternal() override { return false; }
    int  slotCount() override;
    const char* slotLabel(int i) override;
    const SlotState& slot(int i) override;
    bool assign(int i, const TagInfo& t) override;
    String status() override;
    void refresh() override;
};
