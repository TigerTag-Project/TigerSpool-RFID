#pragma once
#include "printer.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

class CrealityBackend : public PrinterBackend {
public:
    void begin(const PrinterCfg& cfg) override;
    void loop() override;
    void stop() override;
    bool connected() override;
    int  slotCount() override { return 5; }        // external holder + 1A..1D
    const char* slotLabel(int i) override;
    const SlotState& slot(int i) override;
    bool assign(int i, const TagInfo& t) override;
    String status() override;
    void refresh() override;
private:
    void applyBoxsInfo(JsonObjectConst bi);
    void onMsg(uint8_t* payload, size_t len);
    void onEvent(WStype_t type, uint8_t* payload, size_t len);
    bool sendDoc(JsonDocument& d);

    // One socket per printer. It used to be a file static, which is exactly
    // what made a second K2 impossible: two printers shared one connection and
    // whichever was selected last owned it.
    WebSocketsClient ws_;
    bool      connected_ = false;
    String    status_ = "K2: connecting...";
    SlotState slots_[5];
    uint32_t  lastReq_ = 0;
};
