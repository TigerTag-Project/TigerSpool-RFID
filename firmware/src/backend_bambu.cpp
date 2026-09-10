#include "backend_bambu.h"
#include "i18n.h"
#include "tigertag_cloud.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

namespace {
    // An empty slot name means the external spool (vt_tray) - the one label
    // that is a word rather than a position, so it is translated at draw time
    // rather than stored in the map.
    const char* BAMBU_CLOUD_HOST = ".mqtt.bambulab.com";
    // A pushall from an X1 with four AMS units reaches about 50 KB, and a
    // message that does not fit is dropped whole, so the foreground printer -
    // the only one whose slots are ever drawn - gets room for that worst case.
    //
    // A background link is asked for one thing only: whether it is connected,
    // which is the dot in the list. That answer comes from the MQTT session and
    // not from the report, so 8 KB is enough for it, and the 43 KB saved per
    // printer is the difference between three Bambus open at once and one.
    const uint16_t BAMBU_BUF_FULL = 51200;
    const uint16_t BAMBU_BUF_BG   = 8192;
}

void BambuBackend::setDefaultMap() {
    static const BSlot def[5] = {
        { "", 255, 254 }, { "A1", 0, 0 }, { "A2", 0, 1 }, { "A3", 0, 2 }, { "A4", 0, 3 },
    };
    for (int i = 0; i < 5; i++) map_[i] = def[i];
    nSlots_ = 5;
}

namespace {
    // TigerTag material -> Bambu's generic { tray_info_idx, tray_type }
    struct BMat { const char* idx; const char* type; };
    BMat bambuMat(const String& in) {
        String s = in; s.toUpperCase();
        auto has = [&](const char* k) { return s.indexOf(k) >= 0; };
        if (has("PLA") && has("CF"))  return { "GFL98", "PLA-CF" };
        if (has("PETG"))              return { "GFG99", "PETG" };
        if (has("PLA"))               return { "GFL99", "PLA" };
        if (has("ASA"))               return { "GFB98", "ASA" };
        if (has("ABS"))               return { "GFB99", "ABS" };
        if (has("TPU"))               return { "GFU99", "TPU" };
        if (has("PVA"))               return { "GFS99", "PVA" };
        if (has("PC"))                return { "GFC99", "PC" };
        if (has("PA") && has("CF"))   return { "GFN98", "PA-CF" };
        if (has("PA"))                return { "GFN99", "PA" };
        return { "GFL99", "PLA" };
    }

    void parseCol(const char* s, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (!s || strlen(s) < 6) return;
        auto hx = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            c |= 0x20; return (c >= 'a' && c <= 'f') ? c - 'a' + 10 : 0;
        };
        r = hx(s[0]) * 16 + hx(s[1]);
        g = hx(s[2]) * 16 + hx(s[3]);
        b = hx(s[4]) * 16 + hx(s[5]);
    }
}

// Rebuild map_ from the AMS units present in the report
void BambuBackend::rebuildMap(JsonArrayConst amsArr) {
    int ids[4], nu = 0;
    if (!amsArr.isNull())
        for (JsonObjectConst a : amsArr) {
            int id = a["id"].as<int>();
            if (id < 0 || id > 3) continue;
            bool dup = false;
            for (int i = 0; i < nu; i++) if (ids[i] == id) dup = true;
            if (!dup && nu < 4) ids[nu++] = id;
        }
    for (int i = 0; i < nu; i++)
        for (int j = i + 1; j < nu; j++)
            if (ids[j] < ids[i]) { int t = ids[i]; ids[i] = ids[j]; ids[j] = t; }

    int n = 0;
    map_[n].name[0] = 0;   // external spool: translated at draw time
    map_[n].ams = 255; map_[n].tray = 254; n++;
    for (int u = 0; u < nu && n < BMAX; u++)
        for (int tr = 0; tr < 4 && n < BMAX; tr++) {
            // The unit is a letter and the tray a number: A1..A4 for the
            // first AMS, B1..B4 for the second. That is what Bambu Studio
            // shows, and matching it is the difference between a user
            // reading the slot and having to work it out.
            snprintf(map_[n].name, 4, "%c%d", 'A' + ids[u], tr + 1);
            map_[n].ams = ids[u];
            map_[n].tray = tr;
            n++;
        }
    if (n != nSlots_) Serial.printf("[bambu] AMS: %d unit(s) -> %d slots\n", nu, n);
    nSlots_ = n;
}

void BambuBackend::applyTray(int ams, int trayId, JsonObjectConst t) {
    int si = -1;
    for (int i = 0; i < nSlots_; i++) if (map_[i].ams == ams && map_[i].tray == trayId) si = i;
    if (si < 0) return;
    SlotState& s = slots_[si];
    const char* tt = t["tray_type"] | "";
    const char* tc = t["tray_color"] | "";
    s.type  = tt;
    s.known = strlen(tt) > 0;
    uint8_t r = 90, g = 90, b = 90;
    parseCol(tc, r, g, b);
    s.r = r; s.g = g; s.b = b;
}

void BambuBackend::onMqtt(uint8_t* payload, unsigned int len) {

    // Filter: the slot fields only. A full pushall runs past 50 KB.
    JsonDocument filter;
    {
        JsonObject tr = filter["print"]["ams"]["ams"][0].to<JsonObject>();
        tr["id"] = true;
        JsonObject ty = tr["tray"][0].to<JsonObject>();
        ty["id"] = true; ty["tray_type"] = true; ty["tray_color"] = true;
        JsonObject vt = filter["print"]["vt_tray"].to<JsonObject>();
        vt["tray_type"] = true; vt["tray_color"] = true;
    }
    JsonDocument doc;
    if (deserializeJson(doc, payload, len,
                        DeserializationOption::Filter(filter),
                        DeserializationOption::NestingLimit(20))) return;
    JsonObjectConst pr = doc["print"];
    if (pr.isNull()) return;

    // AMS (the "id" arrives as the string "0" or as a number)
    JsonArrayConst amsArr = pr["ams"]["ams"].as<JsonArrayConst>();
    if (!amsArr.isNull()) {
        rebuildMap(amsArr);                 // topologia real (AMS Lite / AMS / multi)
        for (JsonObjectConst a : amsArr) {
            int amsId = a["id"].as<int>();
            JsonArrayConst trays = a["tray"].as<JsonArrayConst>();
            if (trays.isNull()) continue;
            for (JsonObjectConst t : trays) {
                if (t["id"].isNull()) continue;
                applyTray(amsId, t["id"].as<int>(), t);
            }
        }
        status_ = "Bambu: slots atualizados";
    }
    // external spool
    JsonObjectConst vt = pr["vt_tray"];
    if (!vt.isNull()) applyTray(255, 254, vt);
}

void BambuBackend::pubRequest(const String& body) {
    Serial.printf("[bambu] -> %s\n", body.c_str());
    mqtt_.publish(topRequest_.c_str(), body.c_str());
}

void BambuBackend::begin(const PrinterCfg& cfg) {
    host_ = cfg.host; sn_ = cfg.sn; cc_ = cfg.cc;   // user "bblp", pass = access code (Modo LAN)
    cloud_ = cfg.cloud;
    user_ = "bblp";
    port_ = 8883;

    // A cloud printer is the same protocol against a different broker. The
    // report is byte-for-byte the one the LAN path already parses - same
    // pushall, same print.ams - so everything below this line is unchanged.
    // What differs is where it connects and who it says it is: Bambu's own
    // regional broker, with a session the DESKTOP obtained and wrote into the
    // account. This device never signs in to Bambu.
    if (cloud_) {
        String user, token, region;
        if (!ttcloud::bambuCloud(user, token, region)) {
            status_ = "Bambu: no cloud session";
            Serial.println("[bambu] no cloud session in the account - "
                           "sign in to Bambu in Tiger Studio");
            host_ = "";
            return;
        }
        host_ = region + BAMBU_CLOUD_HOST;
        user_ = user;
        cc_   = token;
    }
    setDefaultMap();                     // assumed until the first pushall
    for (int i = 0; i < BMAX; i++) slots_[i] = SlotState{};
    connected_ = false;
    status_ = "Bambu: connecting...";
    topReport_  = String("device/") + sn_ + "/report";
    topRequest_ = String("device/") + sn_ + "/request";

    // setInsecure() is correct here and must stay. The printer is on the local
    // network and presents a self-signed certificate: there is no authority to
    // verify it against, and no certificate store would accept it. Trust comes
    // from the access code, which the user read off the printer's own screen.
    // Calls that leave the network are verified - see net/tls.h.
    net_.setInsecure();
    mqtt_.setServer(host_.c_str(), port_);
    // A pushall from an X1 with four AMS units reaches about 50 KB. If the
    // buffer cannot hold it the topology
    // (the unit count) is never detected. A generous buffer: the heap has room,
    // because the LVGL sprite lives in PSRAM).
    mqtt_.setBufferSize(foreground_ ? BAMBU_BUF_FULL : BAMBU_BUF_BG);
    mqtt_.setKeepAlive(30);
    // A bounded wait, because this runs in the main loop.
    //
    // PubSubClient's socket timeout is FIFTEEN SECONDS by default, and a TLS
    // handshake in the Arduino client waits two minutes. With one printer per
    // backend that is now six of these in a row: when the network goes away -
    // and at -78 dBm it does - the loop stopped for long enough that the panel
    // was lit and completely unresponsive, which is what a frozen device looks
    // like to the person holding it. Nothing here is worth more than a few
    // seconds; a failed attempt is retried anyway.
    mqtt_.setSocketTimeout(4);
    net_.setHandshakeTimeout(5);
    mqtt_.setCallback([this](char*, uint8_t* p, unsigned int l) { onMqtt(p, l); });
    lastTry_ = 0;
}

void BambuBackend::loop() {
    if (!mqtt_.connected()) {
        // Why the session ended, in PubSubClient's own words. -4 is its read
        // timeout, which is SELF-INFLICTED: setSocketTimeout bounds every read,
        // not just the connect, so a large report over a weak link makes the
        // client hang up on a printer that was answering perfectly.
        if (connected_)
            Serial.printf("[bambu] session lost, state %d\n", mqtt_.state());
        connected_ = false;
        if (millis() - lastTry_ < 4000) return;
        lastTry_ = millis();
        if (host_.isEmpty()) return;          // cloud printer with no session
        Serial.printf("[bambu] connecting to %s:%u%s...\n", host_.c_str(),
                      (unsigned)port_, cloud_ ? " (cloud)" : "");
        // Unique per PRINTER, not per device. The same Bambu account may be
        // open on a phone and a desktop, and a shared client id makes the
        // broker kick whichever connected first - which is exactly what two of
        // these objects did to each other the moment there was more than one:
        // an X1C and a P1P on the cloud broker took turns disconnecting, both
        // reporting "up" then dropping, for ever. The serial is what makes the
        // id belong to one machine.
        String cid = "tigerspool-" + String((uint32_t)ESP.getEfuseMac(), HEX) + "-" + sn_;
        if (mqtt_.connect(cid.c_str(), user_.c_str(), cc_.c_str())) {
            mqtt_.subscribe(topReport_.c_str());
            connected_ = true;
            status_ = "Bambu: ligado";
            Serial.println("[bambu] ligado + subscrito");
            refresh();
        } else {
            status_ = String("Bambu: MQTT rc=") + mqtt_.state() + " (access code?)";
            Serial.println(status_);
        }
        return;
    }
    mqtt_.loop();
    // NOT every eight seconds. Once at connect, and then only as a safety net.
    //
    // Measured on two printers over 150 seconds: forty pushall answers, 4.9 KB
    // each, 81 KB a minute in total - for trays that had not moved. The whole
    // machine state, re-sent, to learn that nothing changed.
    //
    // It buys nothing, because the printer PUBLISHES an AMS report by itself
    // the moment a spool changes; between those it sends 70-byte telemetry and
    // nothing else. So the state is current without asking, and asking is
    // 81 KB a minute of a Wi-Fi link that is not always strong.
    //
    // Five minutes is the backstop: a report missed while the socket was down
    // must not leave a wrong grid on screen for ever. Opening a printer's
    // screen also forces one - see setForeground().
    if (millis() - lastPush_ > 300000) { lastPush_ = millis(); refresh(); }
}

void BambuBackend::setForeground(bool on) {
    if (on == foreground_) return;
    foreground_ = on;
    // Resize on the spot. PubSubClient reallocates, which is safe between
    // messages, and this is called from the link tick and never from a
    // callback.
    if (mqtt_.setBufferSize(on ? BAMBU_BUF_FULL : BAMBU_BUF_BG) && on) refresh();
}

void BambuBackend::stop() {
    mqtt_.disconnect();
    connected_ = false;
    status_ = "Bambu: parado";
}

bool BambuBackend::connected() { return connected_; }
String BambuBackend::status()  { return status_; }
int  BambuBackend::slotCount() { return nSlots_; }
const char* BambuBackend::slotLabel(int i) {
    const char* n = map_[(i >= 0 && i < nSlots_) ? i : 0].name;
    return (n && *n) ? n : i18n::T(S_HOLDER);
}
const SlotState& BambuBackend::slot(int i) { return slots_[(i >= 0 && i < nSlots_) ? i : 0]; }

void BambuBackend::refresh() {
    if (!connected_) return;
    JsonDocument d;
    JsonObject p = d["pushing"].to<JsonObject>();
    p["sequence_id"] = String(seq_++);
    p["command"] = "pushall";
    p["version"] = 1;
    p["push_target"] = 1;
    String b; serializeJson(d, b);
    pubRequest(b);
}

bool BambuBackend::assign(int idx, const TagInfo& t) {
    // Bambu's cloud broker takes a report subscription and refuses the command
    // that sets a tray. Saying so here rather than sending it means the screen
    // never claims a write that the service was always going to drop.
    if (cloud_) { status_ = "Bambu: cloud is read only"; return false; }
    if (idx < 0 || idx >= nSlots_ || !connected_) return false;
    BMat m = bambuMat(t.material);
    char col[9]; snprintf(col, sizeof(col), "%02X%02X%02XFF", t.r, t.g, t.b);   // RRGGBBAA

    JsonDocument d;
    JsonObject p = d["print"].to<JsonObject>();
    p["sequence_id"]    = String(seq_++);
    p["command"]        = "ams_filament_setting";
    p["ams_id"]         = map_[idx].ams;
    p["tray_id"]        = map_[idx].tray;
    p["tray_info_idx"]  = m.idx;
    p["tray_color"]     = col;
    p["nozzle_temp_min"] = t.nozMin ? t.nozMin : 190;
    p["nozzle_temp_max"] = t.nozMax ? t.nozMax : 240;
    p["tray_type"]      = m.type;
    String b; serializeJson(d, b);
    pubRequest(b);

    status_ = String("sent -> ") + map_[idx].name + " " + m.type;
    delay(200);
    refresh();
    return true;
}
