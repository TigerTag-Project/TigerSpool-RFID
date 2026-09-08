#pragma once
#include "printer.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// Snapmaker (Artisan / J1 / J1s / U1) - Moonraker (Klipper) over WebSocket:
//   ws://<ip>:7125/websocket , JSON-RPC 2.0 , no authentication.
// 4 slots = 4 extruders (CONFIG_EXTRUDER 0..3). Assigning filament:
//   printer.gcode.script -> "SET_PRINT_FILAMENT_CONFIG CONFIG_EXTRUDER=n
//     VENDOR=.. FILAMENT_TYPE=.. FILAMENT_SUBTYPE= FILAMENT_COLOR_RGBA=RRGGBBFF"
// Slot state: the print_task_config object (filament_color_rgba[],
//   filament_type[], filament_vendor[]).  Source: TigerTag-Studio-Manager
//   renderer/printers/snapmaker/PROTOCOL.md.
class SnapmakerBackend : public PrinterBackend {
public:
    void begin(const PrinterCfg& cfg) override;
    void loop() override;
    void stop() override;
    bool connected() override;
    // Four extruders and nothing else. A U1 has no external spool, so E1 must
    // not be split onto a row of its own the way a Creality's Ext. is.
    bool firstIsExternal() override { return false; }
    int  slotCount() override { return 4; }
    const char* slotLabel(int i) override;
    const SlotState& slot(int i) override;
    bool assign(int i, const TagInfo& t) override;
    String status() override;
    void refresh() override;
private:
    void applyConfig(JsonObjectConst c);
    void onMsg(uint8_t* payload, size_t len);
    void onEvent(WStype_t type, uint8_t* payload, size_t len);
    bool sendRaw(String s);

    WebSocketsClient ws_;                 // one socket per printer
    bool      connected_ = false;
    String    status_ = "Snap: connecting...";
    SlotState slots_[4];
    uint32_t  lastReq_ = 0;
};
