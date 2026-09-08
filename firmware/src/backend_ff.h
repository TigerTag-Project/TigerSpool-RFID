#pragma once
#include "printer.h"

// FlashForge Creator 5 / 5 Pro  -  API HTTP REST na porta 8898.
// Auth is a check code: serialNumber + checkCode in the body of every request,
// no token and no session. LAN mode must be on.
class FlashForgeC5Backend : public PrinterBackend {
public:
    void begin(const PrinterCfg& cfg) override;
    void loop() override;
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
};
