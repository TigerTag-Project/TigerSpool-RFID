#include "backend_anycubic.h"
#include "i18n.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

namespace {

// Five boxes of four is what a Kobra X with four ACE units plus the external
// unit reports - twenty slots, and the largest layout the protocol notes
// describe.
const int AMAX = 20;

struct ASlot { char name[4]; int box; int index; };
ASlot     g_map[AMAX];
int       g_nSlots = 0;
SlotState g_slots[AMAX];

WiFiClientSecure net;
PubSubClient     mqtt(net);

String   g_host, g_devId, g_user, g_pass, g_model;
String   g_topCmd, g_topReport;
bool     g_connected = false;
String   g_status = "Anycubic: connecting...";
uint32_t g_lastTry = 0, g_lastPoll = 0, g_msg = 0;
// Set only when all four credentials are present and the client is configured.
// The guard used to test the device id alone, so a record carrying that but no
// username kept dialling a broker whose address had never been set - printing
// "connecting..." for ever beside the line explaining why it could not.
bool g_ready = false;

// Box -1 first, then 0, 1, 2... and a letter per box in that order. The
// external unit is a unit here, not a single spool: an ACE Pro 2 reports box
// -1 with four slots, and collapsing it to one cell would lose three of them.
void rebuild(JsonArrayConst boxes) {
    int n = 0;
    int letter = 0;
    for (JsonObjectConst b : boxes) {
        if (!b["id"].is<int>()) continue;
        const int id = b["id"];
        JsonArrayConst slots = b["slots"];
        if (slots.isNull()) continue;              // a partial report, not a layout
        for (JsonObjectConst sl : slots) {
            if (n >= AMAX) break;
            const int ix = sl["index"] | 0;
            snprintf(g_map[n].name, sizeof(g_map[n].name), "%c%d", 'A' + letter, ix + 1);
            g_map[n].box = id;
            g_map[n].index = ix;
            n++;
        }
        letter++;
    }
    if (n) g_nSlots = n;
}

int findSlot(int box, int index) {
    for (int i = 0; i < g_nSlots; i++)
        if (g_map[i].box == box && g_map[i].index == index) return i;
    return -1;
}

void applyLayout(JsonArrayConst boxes) {
    rebuild(boxes);
    for (JsonObjectConst b : boxes) {
        if (!b["id"].is<int>()) continue;
        const int id = b["id"];
        JsonArrayConst slots = b["slots"];
        if (slots.isNull()) continue;
        for (JsonObjectConst sl : slots) {
            const int i = findSlot(id, sl["index"] | 0);
            if (i < 0) continue;
            SlotState& s = g_slots[i];
            const char* type = sl["type"] | "";
            s.type  = type;
            s.brand = (const char*)(sl["sku"] | "");
            s.known = strlen(type) > 0;
            JsonArrayConst c = sl["color"];
            if (!c.isNull() && c.size() >= 3) {
                s.r = c[0] | 0; s.g = c[1] | 0; s.b = c[2] | 0;
            }
        }
    }
}

void onMqtt(char* topic, uint8_t* payload, unsigned int len) {
    JsonDocument doc;
    if (deserializeJson(doc, payload, len)) return;
    // Only a full layout carries slots. The partial actions - drying, feeding,
    // auto-feed - send box objects with no `slots` array at all, and treating
    // one as a layout would empty the grid.
    JsonArrayConst boxes = doc["data"]["multi_color_box"];
    if (boxes.isNull()) return;
    applyLayout(boxes);
    g_status = String("Anycubic: ") + g_nSlots + " slots";
    (void)topic; (void)len;
}

String envelope(const char* action, const String& data) {
    // msgid is a plain counter rather than a uuid. The printer echoes it and
    // nothing here matches on it; a uuid would cost entropy for decoration.
    String out = String("{\"type\":\"multiColorBox\",\"action\":\"") + action +
                 "\",\"timestamp\":" + String((uint32_t)(millis())) +
                 ",\"msgid\":\"ts-" + String(++g_msg) + "\"";
    if (data.length()) out += ",\"data\":" + data;
    out += "}";
    return out;
}

}  // namespace

void AnycubicBackend::begin(const PrinterCfg& cfg) {
    g_host  = cfg.host;
    g_devId = cfg.devId;
    g_user  = cfg.user;
    g_pass  = cfg.cc;
    g_model = cfg.model;

    for (int i = 0; i < AMAX; i++) g_slots[i] = SlotState{};
    g_nSlots = 0;
    g_connected = false;
    g_ready = false;

    if (g_devId.isEmpty() || g_user.isEmpty() || g_model.isEmpty()) {
        // Said once, plainly, rather than discovered as a connection that never
        // succeeds: these three cannot be read off the printer, so an empty one
        // means the printer was never paired in AnycubicSlicerNext.
        g_status = "Anycubic: not paired in the slicer";
        Serial.printf("[anycubic] cannot connect - missing%s%s%s from the account. "
                      "Pair the printer in AnycubicSlicerNext once, then sync.\n",
                      g_devId.isEmpty() ? " deviceId" : "",
                      g_user.isEmpty()  ? " username" : "",
                      g_model.isEmpty() ? " acuModelId" : "");
        return;
    }

    g_topCmd    = String("anycubic/anycubicCloud/v1/web/printer/") + g_model + "/" +
                  g_devId + "/multiColorBox";
    g_topReport = String("anycubic/anycubicCloud/v1/printer/public/") + g_model + "/" +
                  g_devId + "/multiColorBox/report";

    // Self-signed, on the local network, and there is no authority that could
    // vouch for it. Trust comes from credentials the desktop obtained, exactly
    // as it does for a Bambu printer in LAN mode. Calls that leave the network
    // are verified - see net/tls.h.
    g_ready = true;
    net.setInsecure();
    mqtt.setServer(g_host.c_str(), 9883);
    // A twenty-slot layout report is a few kilobytes.
    mqtt.setBufferSize(16384);
    mqtt.setKeepAlive(30);
    mqtt.setCallback(onMqtt);
    g_status = "Anycubic: connecting...";
    g_lastTry = 0;
}

void AnycubicBackend::loop() {
    if (!g_ready) return;                       // nothing to connect to
    if (!mqtt.connected()) {
        g_connected = false;
        if (millis() - g_lastTry < 4000) return;
        g_lastTry = millis();
        Serial.printf("[anycubic] connecting to %s:9883...\n", g_host.c_str());
        String cid = "tigerspool-" + String((uint32_t)ESP.getEfuseMac(), HEX);
        if (mqtt.connect(cid.c_str(), g_user.c_str(), g_pass.c_str())) {
            mqtt.subscribe(g_topReport.c_str());
            g_connected = true;
            g_status = "Anycubic: connected";
            Serial.println("[anycubic] connected + subscribed");
            refresh();
        } else {
            g_status = String("Anycubic: MQTT rc=") + mqtt.state();
            Serial.println(g_status);
        }
        return;
    }
    mqtt.loop();
    if (millis() - g_lastPoll > 8000) { g_lastPoll = millis(); refresh(); }
}

void AnycubicBackend::stop() {
    mqtt.disconnect();
    g_connected = false;
    g_status = "Anycubic: stopped";
}

bool AnycubicBackend::connected() { return g_connected; }
String AnycubicBackend::status()  { return g_status; }

int AnycubicBackend::slotCount() { return g_nSlots; }

const char* AnycubicBackend::slotLabel(int i) {
    if (i < 0 || i >= g_nSlots) return "?";
    return g_map[i].name;
}

const SlotState& AnycubicBackend::slot(int i) {
    return g_slots[(i >= 0 && i < AMAX) ? i : 0];
}

void AnycubicBackend::refresh() {
    if (!mqtt.connected()) return;
    mqtt.publish(g_topCmd.c_str(), envelope("getInfo", "").c_str());
}

bool AnycubicBackend::assign(int idx, const TagInfo& t) {
    if (idx < 0 || idx >= g_nSlots || !mqtt.connected()) return false;

    // The printer honours only index, type and colour. Richer fields are
    // accepted with code 200 and silently dropped, so sending them would make
    // the log claim more than the machine did.
    JsonDocument d;
    JsonObject box = d["multi_color_box"].add<JsonObject>();
    box["id"] = g_map[idx].box;
    JsonObject sl = box["slots"].add<JsonObject>();
    sl["index"] = g_map[idx].index;
    sl["type"] = t.material;
    JsonArray col = sl["color"].to<JsonArray>();
    // Pure black renders as empty on the ACE display, so a black spool would
    // look like no spool. The slicer nudges it to 1,1,1 and so does this.
    const bool black = !t.r && !t.g && !t.b;
    col.add(black ? 1 : t.r);
    col.add(black ? 1 : t.g);
    col.add(black ? 1 : t.b);

    String data; serializeJson(d, data);
    const String out = envelope("setInfo", data);
    Serial.printf("[anycubic] -> %s\n", out.c_str());
    if (!mqtt.publish(g_topCmd.c_str(), out.c_str())) return false;

    g_status = String("Anycubic: sent -> ") + slotLabel(idx);
    // There is no per-command acknowledgement. A getInfo round-trip is the only
    // thing that says what actually landed.
    refresh();
    return true;
}
