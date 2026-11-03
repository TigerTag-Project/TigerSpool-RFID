#pragma once
#include "printer.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// Any printer running Klipper + Moonraker - Voron, RatRig, Sovol, Qidi, a
// FlashForge Creator 5 Pro on OpenCreator... - over Moonraker's WebSocket:
//   ws://<host>:<port>/websocket , JSON-RPC 2.0. Port 7125 unless the account
//   says otherwise ("host:port" in its `ip`, the way Tiger Studio stores it).
//
// Klipper itself keeps no filament identity, so the slots come from the
// multi-material system the printer runs, detected once from
// printer.objects.list, exactly as Tiger Studio's Klipper driver does
// (renderer/printers/klipper/core.js, PROTOCOL.md section 7):
//
//   Happy Hare  (object `mmu`)       one slot per gate, labelled 0..n-1,
//                                    written with MMU_GATE_MAP GATE=g MATERIAL= COLOR=
//   AFC         (`AFC_lane <name>`)  one slot per lane, labelled 1..n,
//                                    written with SET_MATERIAL / SET_COLOR LANE=<name>
//   neither                          one slot per extruder (E1, or T0..Tn),
//                                    shown and never written: there is nothing
//                                    on the printer to write the filament into.
//
// Commands are the systems' own: nothing has to be installed on the printer.
// An optional Moonraker API key comes from the account's `apiKey` (carried in
// PrinterCfg::cc) and is presented with server.connection.identify.
class KlipperBackend : public PrinterBackend {
public:
    void begin(const PrinterCfg& cfg) override;
    void loop() override;
    void stop() override;
    bool connected() override;
    // AFC and Happy Hare number their lanes and gates; none of them is an
    // external spool to split onto a row of its own.
    bool firstIsExternal() override { return false; }
    int  slotCount() override { return nSlots_; }
    const char* slotLabel(int i) override;
    const SlotState& slot(int i) override;
    bool assign(int i, const TagInfo& t) override;
    String status() override;
    void refresh() override;

    // Twelve gates is a large ERCF, four Box Turtles are sixteen lanes; twenty
    // leaves room without letting a broken report grow the grid without end.
    static const int KMAX = 20;
private:
    enum System : uint8_t { SYS_UNKNOWN, SYS_MMU, SYS_AFC, SYS_EXTRUDERS };

    void detect(JsonArrayConst objects);
    void applyStatus(JsonObjectConst st);
    void onMsg(uint8_t* payload, size_t len);
    void onEvent(WStype_t type, uint8_t* payload, size_t len);
    bool rpc(const char* method, JsonDocument& params, int id);

    WebSocketsClient ws_;            // one socket per printer
    String    apiKey_;
    bool      connected_ = false;
    System    sys_ = SYS_UNKNOWN;
    String    status_ = "Klipper: connecting...";

    int       nSlots_ = 0;
    SlotState slots_[KMAX];
    char      labels_[KMAX][4];
    String    lane_[KMAX];           // AFC lane name, or the extruder object
    String    laneObj_[KMAX];        // AFC: the object the lane is read from
    uint32_t  lastReq_ = 0;
};
