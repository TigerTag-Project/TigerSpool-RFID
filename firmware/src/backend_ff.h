#pragma once
#include "printer.h"

// FlashForge Creator 5 / 5 Pro  -  API HTTP REST na porta 8898.
// Auth is a check code: serialNumber + checkCode in the body of every request,
// no token and no session. LAN mode must be on.
class FlashForgeC5Backend : public PrinterBackend {
public:
    void begin(const PrinterCfg& cfg) override;
    void loop() override;
    // There is no socket to close - every request is its own HTTP call - but
    // there IS state to drop. Without this the base class's empty stop() left
    // the check code validated after the link had been handed to a different
    // printer, and connected() went on saying yes about a machine this object
    // was no longer talking to.
    void stop() override;
    bool connected() override;
    // Four station slots, 1A to 1D, and no external spool among them - so the
    // first must not be split onto a row of its own the way a Creality's Ext.
    // is. It was, and an AD5X showed 1A alone above 1B, 1C and 1D.
    bool firstIsExternal() override { return false; }
    int  slotCount() override { return 4; }        // material station, 1A..1D
    const char* slotLabel(int i) override;
    const SlotState& slot(int i) override;
    bool assign(int i, const TagInfo& t) override;
    String status() override;
    void refresh() override;
private:
    void tryAuth();
    // Both need this printer's own host and credentials, so neither can be a
    // free function any more.
    String post(const String& path, const String& body, int& httpCode);
    String authBody();

    String    host_, sn_, cc_;
    bool      auth_ = false;
    String    status_ = "FF: connecting...";
    SlotState slots_[4];
    uint32_t  lastReq_ = 0;
    uint32_t  lastAuth_ = 0;
};
