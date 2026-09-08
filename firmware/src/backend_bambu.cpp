#include "backend_bambu.h"
#include "i18n.h"
#include "tigertag_cloud.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

namespace {
    // --- slot map (UI index -> ams_id / tray_id), learned from the pushall ---
    static const int BMAX = 17;                 // external spool + 4 AMS units x 4 trays
    struct BSlot { char name[4]; int ams; int tray; };
    BSlot g_map[BMAX] = {
        // An empty name means the external spool (vt_tray) - the one slot
        // label that is a word rather than a position, so it is translated at
        // draw time rather than stored here.
        { "", 255, 254 },
        { "A1", 0, 0 }, { "A2", 0, 1 }, { "A3", 0, 2 }, { "A4", 0, 3 },  // assumed until the first report
    };
    int  g_nSlots = 5;                          // until the first report

    WiFiClientSecure net;
    PubSubClient     mqtt(net);
    const char* BAMBU_CLOUD_HOST = ".mqtt.bambulab.com";

    String           g_host, g_sn, g_cc, g_user;
    bool             g_cloud = false;
    uint16_t         g_port = 8883;
    String           g_topReport, g_topRequest;
    bool             g_connected = false;
    String           g_status = "Bambu: connecting...";
    SlotState        g_slots[BMAX];
    uint32_t         g_seq = 0;
    uint32_t         g_lastTry = 0, g_lastPush = 0;

    void setDefaultMap() {
        static const BSlot def[5] = {
            { "", 255, 254 }, { "A1", 0, 0 }, { "A2", 0, 1 }, { "A3", 0, 2 }, { "A4", 0, 3 },
        };
        for (int i = 0; i < 5; i++) g_map[i] = def[i];
        g_nSlots = 5;
    }

    // Rebuild g_map from the AMS units present in the report
    void rebuildMap(JsonArrayConst amsArr) {
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
        g_map[n].name[0] = 0;   // external spool: translated at draw time
        g_map[n].ams = 255; g_map[n].tray = 254; n++;
        for (int u = 0; u < nu && n < BMAX; u++)
            for (int tr = 0; tr < 4 && n < BMAX; tr++) {
                // The unit is a letter and the tray a number: A1..A4 for the
                // first AMS, B1..B4 for the second. That is what Bambu Studio
                // shows, and matching it is the difference between a user
                // reading the slot and having to work it out.
                snprintf(g_map[n].name, 4, "%c%d", 'A' + ids[u], tr + 1);
                g_map[n].ams = ids[u];
                g_map[n].tray = tr;
                n++;
            }
        if (n != g_nSlots) Serial.printf("[bambu] AMS: %d unit(s) -> %d slots\n", nu, n);
        g_nSlots = n;
    }

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

    void applyTray(int ams, int trayId, JsonObjectConst t) {
        int si = -1;
        for (int i = 0; i < g_nSlots; i++) if (g_map[i].ams == ams && g_map[i].tray == trayId) si = i;
        if (si < 0) return;
        SlotState& s = g_slots[si];
        const char* tt = t["tray_type"] | "";
        const char* tc = t["tray_color"] | "";
        s.type  = tt;
        s.known = strlen(tt) > 0;
        uint8_t r = 90, g = 90, b = 90;
        parseCol(tc, r, g, b);
        s.r = r; s.g = g; s.b = b;
    }

    void onMqtt(char* /*topic*/, uint8_t* payload, unsigned int len) {
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
            g_status = "Bambu: slots atualizados";
        }
        // external spool
        JsonObjectConst vt = pr["vt_tray"];
        if (!vt.isNull()) applyTray(255, 254, vt);
    }

    void pubRequest(const String& body) {
        Serial.printf("[bambu] -> %s\n", body.c_str());
        mqtt.publish(g_topRequest.c_str(), body.c_str());
    }
}

void BambuBackend::begin(const PrinterCfg& cfg) {
    g_host = cfg.host; g_sn = cfg.sn; g_cc = cfg.cc;   // user "bblp", pass = access code (Modo LAN)
    g_cloud = cfg.cloud;
    g_user = "bblp";
    g_port = 8883;

    // A cloud printer is the same protocol against a different broker. The
    // report is byte-for-byte the one the LAN path already parses - same
    // pushall, same print.ams - so everything below this line is unchanged.
    // What differs is where it connects and who it says it is: Bambu's own
    // regional broker, with a session the DESKTOP obtained and wrote into the
    // account. This device never signs in to Bambu.
    if (g_cloud) {
        String user, token, region;
        if (!ttcloud::bambuCloud(user, token, region)) {
            g_status = "Bambu: no cloud session";
            Serial.println("[bambu] no cloud session in the account - "
                           "sign in to Bambu in Tiger Studio");
            g_host = "";
            return;
        }
        g_host = region + BAMBU_CLOUD_HOST;
        g_user = user;
        g_cc   = token;
    }
    setDefaultMap();                     // assumed until the first pushall
    for (int i = 0; i < BMAX; i++) g_slots[i] = SlotState{};
    g_connected = false;
    g_status = "Bambu: connecting...";
    g_topReport  = String("device/") + g_sn + "/report";
    g_topRequest = String("device/") + g_sn + "/request";

    // setInsecure() is correct here and must stay. The printer is on the local
    // network and presents a self-signed certificate: there is no authority to
    // verify it against, and no certificate store would accept it. Trust comes
    // from the access code, which the user read off the printer's own screen.
    // Calls that leave the network are verified - see net/tls.h.
    net.setInsecure();
    mqtt.setServer(g_host.c_str(), g_port);
    // A pushall from an X1 with four AMS units reaches about 50 KB. If the
    // buffer cannot hold it the topology
    // (the unit count) is never detected. A generous buffer: the heap has room,
    // because the LVGL sprite lives in PSRAM).
    mqtt.setBufferSize(51200);
    mqtt.setKeepAlive(30);
    mqtt.setCallback(onMqtt);
    g_lastTry = 0;
}

void BambuBackend::loop() {
    if (!mqtt.connected()) {
        g_connected = false;
        if (millis() - g_lastTry < 4000) return;
        g_lastTry = millis();
        if (g_host.isEmpty()) return;          // cloud printer with no session
        Serial.printf("[bambu] connecting to %s:%u%s...\n", g_host.c_str(),
                      (unsigned)g_port, g_cloud ? " (cloud)" : "");
        // Unique per device on purpose: the same Bambu account may be open on a
        // phone and a desktop, and a shared client id makes the broker kick
        // whichever connected first.
        String cid = "tigerspool-" + String((uint32_t)ESP.getEfuseMac(), HEX);
        if (mqtt.connect(cid.c_str(), g_user.c_str(), g_cc.c_str())) {
            mqtt.subscribe(g_topReport.c_str());
            g_connected = true;
            g_status = "Bambu: ligado";
            Serial.println("[bambu] ligado + subscrito");
            refresh();
        } else {
            g_status = String("Bambu: MQTT rc=") + mqtt.state() + " (access code?)";
            Serial.println(g_status);
        }
        return;
    }
    mqtt.loop();
    if (millis() - g_lastPush > 8000) { g_lastPush = millis(); refresh(); }
}

void BambuBackend::stop() {
    mqtt.disconnect();
    g_connected = false;
    g_status = "Bambu: parado";
}

bool BambuBackend::connected() { return g_connected; }
String BambuBackend::status()  { return g_status; }
int  BambuBackend::slotCount() { return g_nSlots; }
const char* BambuBackend::slotLabel(int i) {
    const char* n = g_map[(i >= 0 && i < g_nSlots) ? i : 0].name;
    return (n && *n) ? n : i18n::T(S_HOLDER);
}
const SlotState& BambuBackend::slot(int i) { return g_slots[(i >= 0 && i < g_nSlots) ? i : 0]; }

void BambuBackend::refresh() {
    if (!g_connected) return;
    JsonDocument d;
    JsonObject p = d["pushing"].to<JsonObject>();
    p["sequence_id"] = String(g_seq++);
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
    if (g_cloud) { g_status = "Bambu: cloud is read only"; return false; }
    if (idx < 0 || idx >= g_nSlots || !g_connected) return false;
    BMat m = bambuMat(t.material);
    char col[9]; snprintf(col, sizeof(col), "%02X%02X%02XFF", t.r, t.g, t.b);   // RRGGBBAA

    JsonDocument d;
    JsonObject p = d["print"].to<JsonObject>();
    p["sequence_id"]    = String(g_seq++);
    p["command"]        = "ams_filament_setting";
    p["ams_id"]         = g_map[idx].ams;
    p["tray_id"]        = g_map[idx].tray;
    p["tray_info_idx"]  = m.idx;
    p["tray_color"]     = col;
    p["nozzle_temp_min"] = t.nozMin ? t.nozMin : 190;
    p["nozzle_temp_max"] = t.nozMax ? t.nozMax : 240;
    p["tray_type"]      = m.type;
    String b; serializeJson(d, b);
    pubRequest(b);

    g_status = String("sent -> ") + g_map[idx].name + " " + m.type;
    delay(200);
    refresh();
    return true;
}
