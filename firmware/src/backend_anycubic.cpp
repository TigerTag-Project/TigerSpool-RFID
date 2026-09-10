#include "backend_anycubic.h"
#include "i18n.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>


void AnycubicBackend::rebuild(JsonArrayConst boxes) {
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
            snprintf(map_[n].name, sizeof(map_[n].name), "%c%d", 'A' + letter, ix + 1);
            map_[n].box = id;
            map_[n].index = ix;
            n++;
        }
        letter++;
    }
    if (n) nSlots_ = n;
}

int AnycubicBackend::findSlot(int box, int index) const {
    for (int i = 0; i < nSlots_; i++)
        if (map_[i].box == box && map_[i].index == index) return i;
    return -1;
}

void AnycubicBackend::applyLayout(JsonArrayConst boxes) {
    rebuild(boxes);
    for (JsonObjectConst b : boxes) {
        if (!b["id"].is<int>()) continue;
        const int id = b["id"];
        JsonArrayConst slots = b["slots"];
        if (slots.isNull()) continue;
        for (JsonObjectConst sl : slots) {
            const int i = findSlot(id, sl["index"] | 0);
            if (i < 0) continue;
            SlotState& s = slots_[i];
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

void AnycubicBackend::onMqtt(uint8_t* payload, unsigned int len) {
    JsonDocument doc;
    if (deserializeJson(doc, payload, len)) return;
    // Only a full layout carries slots. The partial actions - drying, feeding,
    // auto-feed - send box objects with no `slots` array at all, and treating
    // one as a layout would empty the grid.
    JsonArrayConst boxes = doc["data"]["multi_color_box"];
    if (boxes.isNull()) return;
    applyLayout(boxes);
    status_ = String("Anycubic: ") + nSlots_ + " slots";
    (void)len;
}

String AnycubicBackend::envelope(const char* action, const String& data) {
    // msgid is a plain counter rather than a uuid. The printer echoes it and
    // nothing here matches on it; a uuid would cost entropy for decoration.
    String out = String("{\"type\":\"multiColorBox\",\"action\":\"") + action +
                 "\",\"timestamp\":" + String((uint32_t)(millis())) +
                 ",\"msgid\":\"ts-" + String(++msg_) + "\"";
    if (data.length()) out += ",\"data\":" + data;
    out += "}";
    return out;
}

void AnycubicBackend::begin(const PrinterCfg& cfg) {
    host_  = cfg.host;
    devId_ = cfg.devId;
    user_  = cfg.user;
    pass_  = cfg.cc;
    model_ = cfg.model;

    for (int i = 0; i < AMAX; i++) slots_[i] = SlotState{};
    nSlots_ = 0;
    connected_ = false;
    ready_ = false;

    if (devId_.isEmpty() || user_.isEmpty() || model_.isEmpty()) {
        // Said once, plainly, rather than discovered as a connection that never
        // succeeds: these three cannot be read off the printer, so an empty one
        // means the printer was never paired in AnycubicSlicerNext.
        status_ = "Anycubic: not paired in the slicer";
        Serial.printf("[anycubic] cannot connect - missing%s%s%s from the account. "
                      "Pair the printer in AnycubicSlicerNext once, then sync.\n",
                      devId_.isEmpty() ? " deviceId" : "",
                      user_.isEmpty()  ? " username" : "",
                      model_.isEmpty() ? " acuModelId" : "");
        return;
    }

    topCmd_    = String("anycubic/anycubicCloud/v1/web/printer/") + model_ + "/" +
                  devId_ + "/multiColorBox";
    topReport_ = String("anycubic/anycubicCloud/v1/printer/public/") + model_ + "/" +
                  devId_ + "/multiColorBox/report";

    // Self-signed, on the local network, and there is no authority that could
    // vouch for it. Trust comes from credentials the desktop obtained, exactly
    // as it does for a Bambu printer in LAN mode. Calls that leave the network
    // are verified - see net/tls.h.
    ready_ = true;
    net_.setInsecure();
    mqtt_.setServer(host_.c_str(), 9883);
    // A twenty-slot layout report is a few kilobytes.
    mqtt_.setBufferSize(16384);
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
    status_ = "Anycubic: connecting...";
    lastTry_ = 0;
}

void AnycubicBackend::loop() {
    if (!ready_) return;                       // nothing to connect to
    if (!mqtt_.connected()) {
        if (connected_)
            Serial.printf("[anycubic] session lost, state %d\n", mqtt_.state());
        connected_ = false;
        if (millis() - lastTry_ < 4000) return;
        lastTry_ = millis();
        Serial.printf("[anycubic] connecting to %s:9883...\n", host_.c_str());
        // One id per printer: two ACE-equipped machines on one account would
        // otherwise kick each other off the broker in turn.
        String cid = "tigerspool-" + String((uint32_t)ESP.getEfuseMac(), HEX) + "-" + devId_;
        if (mqtt_.connect(cid.c_str(), user_.c_str(), pass_.c_str())) {
            mqtt_.subscribe(topReport_.c_str());
            connected_ = true;
            status_ = "Anycubic: connected";
            Serial.println("[anycubic] connected + subscribed");
            refresh();
        } else {
            status_ = String("Anycubic: MQTT rc=") + mqtt_.state();
            Serial.println(status_);
        }
        return;
    }
    mqtt_.loop();
    if (millis() - lastPoll_ > 8000) { lastPoll_ = millis(); refresh(); }
}

void AnycubicBackend::stop() {
    mqtt_.disconnect();
    connected_ = false;
    status_ = "Anycubic: stopped";
}

bool AnycubicBackend::connected() { return connected_; }
String AnycubicBackend::status()  { return status_; }

int AnycubicBackend::slotCount() { return nSlots_; }

const char* AnycubicBackend::slotLabel(int i) {
    if (i < 0 || i >= nSlots_) return "?";
    return map_[i].name;
}

const SlotState& AnycubicBackend::slot(int i) {
    return slots_[(i >= 0 && i < AMAX) ? i : 0];
}

void AnycubicBackend::refresh() {
    if (!mqtt_.connected()) return;
    mqtt_.publish(topCmd_.c_str(), envelope("getInfo", "").c_str());
}

bool AnycubicBackend::assign(int idx, const TagInfo& t) {
    if (idx < 0 || idx >= nSlots_ || !mqtt_.connected()) return false;

    // The printer honours only index, type and colour. Richer fields are
    // accepted with code 200 and silently dropped, so sending them would make
    // the log claim more than the machine did.
    JsonDocument d;
    JsonObject box = d["multi_color_box"].add<JsonObject>();
    box["id"] = map_[idx].box;
    JsonObject sl = box["slots"].add<JsonObject>();
    sl["index"] = map_[idx].index;
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
    if (!mqtt_.publish(topCmd_.c_str(), out.c_str())) return false;

    status_ = String("Anycubic: sent -> ") + slotLabel(idx);
    // There is no per-command acknowledgement. A getInfo round-trip is the only
    // thing that says what actually landed.
    refresh();
    return true;
}
