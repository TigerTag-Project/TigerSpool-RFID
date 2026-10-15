#include "backend_bambu.h"
#include "product_api.h"
#include "i18n.h"
#include "tigertag_cloud.h"
#include "bambu_cloud.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

namespace {
    // An empty slot name means the external spool (vt_tray) - the one label
    // that is a word rather than a position, so it is translated at draw time
    // rather than stored in the map.
    // A pushall from an X1 with four AMS units reaches about 50 KB, and a
    // message that does not fit is dropped whole, so the foreground printer -
    // the only one whose slots are ever drawn - gets room for that worst case.
    //
    // A background link is asked for one thing only: whether it is connected,
    // which is the dot in the list. That answer comes from the MQTT session and
    // not from the report, so 8 KB is enough for it. The 43 KB saved is PSRAM,
    // not internal RAM - see linkCost() in main.cpp, which was wrong about this
    // until it was measured. What limits how many Bambus can be open is the TLS
    // session, and this buffer has nothing to do with it.
    const uint16_t BAMBU_BUF_FULL = 51200;
    // 20 KB in the background, not 8.
    //
    // PubSubClient DROPS a message larger than its buffer, silently. A real
    // report from an X1C with two AMS and an AMS HT is 8.9 KB - measured, by
    // replaying a user's own dump onto a printer's report topic - so at 8192 a
    // printer with more than one AMS never updated its slots unless it was the
    // one on screen. The buffer goes to PSRAM at this size, which is what makes
    // the fix cheap: 20 KB of PSRAM per background printer against slots that
    // are simply wrong.
    const uint16_t BAMBU_BUF_BG   = 20480;
}

void BambuBackend::setDefaultMap() {
    static const BSlot def[5] = {
        { "", 255, 254 }, { "A1", 0, 0 }, { "A2", 0, 1 }, { "A3", 0, 2 }, { "A4", 0, 3 },
    };
    for (int i = 0; i < 5; i++) map_[i] = def[i];
    nSlots_ = 5;
}

namespace {
    // TigerTag material -> Bambu's generic { tray_info_idx, tray_type }.
    //
    // The FALLBACK now, not the source: a filament's Bambu id comes from the
    // TigerTag+ product endpoint or the material table (filament_resolve.cpp),
    // and this keyword guess only answers for a material neither knows.
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
// The AMS HT is unit 128, not unit 4.
//
// Reported by a user with two AMS and one AMS HT on an X1C, with the MQTT dump
// to prove it: the units arrive as id "0", id "1" and id "128", the HT holding
// a single tray. Everything above id 3 used to be dropped here, so the HT was
// invisible - the two AMS and the external spool appeared and the dryer did
// not. Bambu numbers the HT units from 128 upwards and gives each one tray;
// the printer's own tray_exist_bits agrees, setting bit 16 for the first of
// them rather than a bit inside the AMS range.
//
// The letters follow Bambu Studio for the AMS - A1..A4, B1..B4 - and the HT
// units are labelled HT1, HT2 and so on, because there is no letter for them
// that anybody could check against Studio's screen.
void BambuBackend::rebuildMap(JsonArrayConst amsArr) {
    const int HT_BASE = 128;          // first AMS HT unit id
    int ids[8], nu = 0;
    if (!amsArr.isNull())
        for (JsonObjectConst a : amsArr) {
            const int id = a["id"].as<int>();
            const bool ams = (id >= 0 && id <= 3);
            const bool ht  = (id >= HT_BASE && id <= HT_BASE + 3);
            if (!ams && !ht) continue;
            bool dup = false;
            for (int i = 0; i < nu; i++) if (ids[i] == id) dup = true;
            if (!dup && nu < 8) ids[nu++] = id;
        }
    for (int i = 0; i < nu; i++)
        for (int j = i + 1; j < nu; j++)
            if (ids[j] < ids[i]) { int t = ids[i]; ids[i] = ids[j]; ids[j] = t; }

    int n = 0;
    map_[n].name[0] = 0;   // external spool: translated at draw time
    map_[n].ams = 255; map_[n].tray = 254; n++;
    for (int u = 0; u < nu && n < BMAX; u++) {
        const bool ht = ids[u] >= HT_BASE;
        // An HT is one heated slot, not four.
        const int trays = ht ? 1 : 4;
        for (int tr = 0; tr < trays && n < BMAX; tr++) {
            if (ht) snprintf(map_[n].name, sizeof(map_[n].name), "HT%d", ids[u] - HT_BASE + 1);
            else    snprintf(map_[n].name, sizeof(map_[n].name), "%c%d", 'A' + ids[u], tr + 1);
            map_[n].ams  = ids[u];
            map_[n].tray = tr;
            n++;
        }
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
    uint8_t r = 90, g = 90, b = 90;
    parseCol(tc, r, g, b);
    // A change, named by the printer that received it. With one session
    // carrying several printers' reports this is the line that proves each
    // one landed on the right machine - a slot changed on one printer must
    // appear under that printer's serial and no other.
    if (s.type != tt || s.r != r || s.g != g || s.b != b)
        Serial.printf("[bambu] %s %s: %s #%02X%02X%02X\n", sn_.c_str(),
                      map_[si].name[0] ? map_[si].name : "Ext", *tt ? tt : "-", r, g, b);
    s.type  = tt;
    s.known = strlen(tt) > 0;
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
        rebuildMap(amsArr);                 // the real topology (AMS Lite / AMS / multi)
        for (JsonObjectConst a : amsArr) {
            int amsId = a["id"].as<int>();
            JsonArrayConst trays = a["tray"].as<JsonArrayConst>();
            if (trays.isNull()) continue;
            for (JsonObjectConst t : trays) {
                if (t["id"].isNull()) continue;
                applyTray(amsId, t["id"].as<int>(), t);
            }
        }
        status_ = "Bambu: slots updated";
    }
    // external spool
    JsonObjectConst vt = pr["vt_tray"];
    if (!vt.isNull()) applyTray(255, 254, vt);
}

void BambuBackend::pubRequest(const String& body) {
    Serial.printf("[bambu] %s -> %s\n", sn_.c_str(), body.c_str());
    if (cloud_) bambu_cloud::publish(sn_, body);
    else        mqtt_.publish(topRequest_.c_str(), body.c_str());
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
    setDefaultMap();                     // assumed until the first pushall
    for (int i = 0; i < BMAX; i++) slots_[i] = SlotState{};
    connected_ = false;
    status_ = "Bambu: connecting...";
    topReport_  = String("device/") + sn_ + "/report";
    topRequest_ = String("device/") + sn_ + "/request";

    // A cloud printer does not open a connection of its own. It joins the one
    // session every cloud printer on the account shares - see bambu_cloud.h -
    // and receives the reports on its own topic through it. That is the whole
    // difference between two cloud Bambus costing two TLS sessions and one.
    if (cloud_) {
        bambu_cloud::attach(sn_, [this](uint8_t* p, unsigned l) { onMqtt(p, l); });
        return;
    }

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
    // The TCP connect has its own limit, and it is not either of the two around
    // it: WiFiClientSecure waits THIRTY seconds for a host that does not
    // answer, which is exactly the loop watchdog. A printer that is switched
    // off then froze the panel for half a minute and restarted the box, again
    // and again, until somebody turned the printer on.
    net_.setTimeout(4);
    net_.setHandshakeTimeout(5);
    mqtt_.setCallback([this](char*, uint8_t* p, unsigned int l) { onMqtt(p, l); });
    lastTry_ = 0;
}

void BambuBackend::loop() {
    if (cloud_) {
        bambu_cloud::loop();
        const bool up = bambu_cloud::connected(sn_);
        if (up && !connected_) {
            connected_ = true;
            status_ = "Bambu: cloud";
            Serial.printf("[bambu] %s up on the shared cloud session\n", sn_.c_str());
            refresh();                        // its state, once, on arrival
        } else if (!up) {
            connected_ = false;
        }
        if (connected_ && millis() - lastPush_ > 300000) { lastPush_ = millis(); refresh(); }
        return;
    }
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
            status_ = "Bambu: connected";
            Serial.println("[bambu] connected and subscribed");
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
    // The shared session has one buffer, sized for the largest report, and it
    // is not this printer's to resize. Coming to the front still earns a fresh
    // state: that is the screen somebody is about to read.
    if (cloud_) { if (on) refresh(); return; }
    // Resize on the spot. PubSubClient reallocates, which is safe between
    // messages, and this is called from the link tick and never from a
    // callback.
    if (mqtt_.setBufferSize(on ? BAMBU_BUF_FULL : BAMBU_BUF_BG) && on) refresh();
}

void BambuBackend::stop() {
    // Leaving the shared session closes it only if this was the last printer
    // on it; the others keep receiving.
    if (cloud_) { bambu_cloud::detach(sn_); connected_ = false; status_ = "Bambu: stopped"; return; }
    mqtt_.disconnect();
    connected_ = false;
    status_ = "Bambu: stopped";
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

    // The same resolution as a Creality slot: temperatures from the TigerTag+
    // product endpoint, the chip, the material table or a default; Bambu's
    // filament id from the endpoint or the table; the type from the table's
    // material family. Only what nobody knows falls back to the keyword guess.
    const filament::ResolvedFilament f = product_api::resolveFor(t);
    const BMat guess = bambuMat(t.material);
    const bool idKnown = f.bambuId[0] != '\0';
    const char* trayIdx  = idKnown ? f.bambuId : guess.idx;
    const bool typeKnown = f.typeSrc == filament::SRC_DB;
    const char* trayType = typeKnown ? f.materialType : guess.type;
    char col[9]; snprintf(col, sizeof(col), "%02X%02X%02XFF", t.r, t.g, t.b);   // RRGGBBAA

    const char* label = map_[idx].name[0] ? map_[idx].name : "Ext";
    Serial.printf("[bambu] %s type=%s(%s) id=%s(%s) temp=%u/%u(%s)\n", label,
                  trayType, typeKnown ? "db" : "guess",
                  trayIdx, idKnown ? filament::sourceName(f.bambuSrc) : "guess",
                  f.nozMin, f.nozMax, filament::sourceName(f.tempSrc));

    JsonDocument d;
    JsonObject p = d["print"].to<JsonObject>();
    p["sequence_id"]    = String(seq_++);
    p["command"]        = "ams_filament_setting";
    // The external spool is addressed as ams 255, tray 0, slot 0 - not by the
    // 254 it is reported under. Measured on an X1C on firmware 01.12.00.00:
    // ams_filament_setting with tray_id 254 is accepted and IGNORED - the tray
    // kept its old filament for minutes, seen on this device's own session and
    // on a second client - while the same values with tray_id 0 and slot_id 0
    // land within seconds and hold. That is the addressing current Bambu
    // Studio sends. slot_id goes on AMS trays too, as Studio sends it; it
    // repeats tray_id there. The slot map keeps 255/254, which is what the
    // report uses to say which tray is the external one.
    const bool external = map_[idx].ams == 255;
    p["ams_id"]         = map_[idx].ams;
    p["tray_id"]        = external ? 0 : map_[idx].tray;
    p["slot_id"]        = external ? 0 : map_[idx].tray;
    p["tray_info_idx"]  = trayIdx;
    p["tray_color"]     = col;
    p["nozzle_temp_min"] = f.nozMin;
    p["nozzle_temp_max"] = f.nozMax;
    p["tray_type"]      = trayType;
    String b; serializeJson(d, b);
    pubRequest(b);

    status_ = String("sent -> ") + label + " " + trayType;
    delay(200);
    refresh();
    return true;
}
