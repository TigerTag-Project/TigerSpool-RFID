#pragma once
#include "printer.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

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
    // Only when the hub is unplugged is slot 0 the external spool. With the
    // Canvas connected the list is four trays and there is no external slot at
    // all, so nothing should be split onto a row of its own.
    bool firstIsExternal() override;
    int  slotCount() override;
    const char* slotLabel(int i) override;
    const SlotState& slot(int i) override;
    bool assign(int i, const TagInfo& t) override;
    String status() override;
    void refresh() override;

    static const int ESLOTS = 5;    // 0 = mono extruder, 1..4 = Canvas trays
private:

    void publish(int method, const String& params);
    void onResult(JsonObjectConst res, int method);
    void onMqtt(uint8_t* payload, unsigned int len);
    int  mapUi(int i) const;

    WiFiClient   net_;              // one socket per printer, not one per brand
    PubSubClient mqtt_{net_};

    String    host_, sn_, pass_;
    String    cid_, rid_;
    String    topRequest_, topStatus_, topResponse_, topRegister_, topRegResp_;
    bool      connected_ = false;
    bool      canvas_ = false;      // the hub is plugged in and reporting
    String    status_ = "Elegoo: connecting...";
    SlotState slots_[ESLOTS];
    uint32_t  lastTry_ = 0, lastPoll_ = 0;
    uint32_t  msgId_ = 0;
};
