#pragma once
#include "printer.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Bambu Lab A1 / A1 mini / A2L / P1 / X1 / H2D over the LAN.
//
// MQTT on port 8883 with TLS. Authentication is the printer's own access code
// from its screen (LAN mode must be on), with user "bblp", and the serial
// number forms the topic: device/<serial>/{report,request}.
//
// Slots are discovered, not assumed. The `pushall` report carries
// print.ams.ams[], and the count runs from five (external spool plus one AMS
// Lite) to seventeen (external plus four units of four). Until the first report
// arrives an AMS Lite is assumed. Labels are "A".."D" with a single unit and
// "1A".."4D" with several.
//
// The report is large - a fully loaded X1 sends around 50 KB - so the MQTT
// receive buffer is sized for it and the JSON is parsed through a filter.
class BambuBackend : public PrinterBackend {
public:
    void begin(const PrinterCfg& cfg) override;
    void loop() override;
    void stop() override;
    void setForeground(bool on) override;
    bool connected() override;
    int  slotCount() override;                      // dynamic, 5..17
    const char* slotLabel(int i) override;
    const SlotState& slot(int i) override;
    bool assign(int i, const TagInfo& t) override;
    String status() override;
    void refresh() override;

    // external spool + 4 AMS units x 4 trays
    static const int BMAX = 17;
private:
    struct BSlot { char name[4]; int ams; int tray; };

    void setDefaultMap();
    void rebuildMap(JsonArrayConst amsArr);
    void applyTray(int ams, int trayId, JsonObjectConst t);
    void onMqtt(uint8_t* payload, unsigned int len);
    void pubRequest(const String& body);

    // One TLS session and one MQTT client per printer. Both were file statics,
    // which is what limited the device to a single Bambu at a time.
    WiFiClientSecure net_;
    PubSubClient     mqtt_{net_};

    BSlot     map_[BMAX] = { { "", 255, 254 },
                             { "A1", 0, 0 }, { "A2", 0, 1 }, { "A3", 0, 2 }, { "A4", 0, 3 } };
    int       nSlots_ = 5;                  // until the first report
    String    host_, sn_, cc_, user_;
    bool      cloud_ = false;
    uint16_t  port_ = 8883;
    String    topReport_, topRequest_;
    bool      connected_ = false;
    String    status_ = "Bambu: connecting...";
    SlotState slots_[BMAX];
    uint32_t  seq_ = 0;
    uint32_t  lastTry_ = 0, lastPush_ = 0;
    bool      foreground_ = false;
};
