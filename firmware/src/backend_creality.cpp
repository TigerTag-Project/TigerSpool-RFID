#include "backend_creality.h"
#include "i18n.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// ---- K2 slot map (UI index -> boxId/slot) -------------------------------
namespace {
    struct CrealitySlot { const char* name; uint8_t box; uint8_t slot; };
    const CrealitySlot CREALITY_SLOTS[5] = {
        // A null name means the external holder - the spool that is not in the
        // CFS. Its label is the one slot name that is a word rather than a
        // position, so it comes from the translation table at draw time.
        { nullptr, 0, 0 }, { "1A", 1, 0 }, { "1B", 1, 1 }, { "1C", 1, 2 }, { "1D", 1, 3 },
    };

    int slotIndex(int box, int mat) {
        for (int i = 0; i < 5; i++)
            if (CREALITY_SLOTS[i].box == box && CREALITY_SLOTS[i].slot == mat) return i;
        return -1;
    }
    void parseColor(const char* s, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (!s || !*s) return;
        if (*s == '#') s++;
        if (strlen(s) == 7) s++;             // "#0RRGGBB" -> salta o 0
        long v = strtol(s, nullptr, 16);
        r = (v >> 16) & 0xFF; g = (v >> 8) & 0xFF; b = v & 0xFF;
    }
}

void CrealityBackend::applyBoxsInfo(JsonObjectConst bi) {
    JsonArrayConst boxes = bi["materialBoxs"];
    if (boxes.isNull()) return;
    for (JsonObjectConst box : boxes) {
        int boxId = box["id"] | -1;
        for (JsonObjectConst m : box["materials"].as<JsonArrayConst>()) {
            int si = slotIndex(boxId, m["id"] | -1);
            if (si < 0) continue;
            SlotState& s = slots_[si];
            const char* type = m["type"] | "";
            int st = m["state"] | 0;
            s.known = (st != 0) || strlen(type);
            s.type = type;
            s.brand = (const char*)(m["vendor"] | "");
            s.percent = m["percent"] | 0;
            s.selected = (m["selected"] | 0) != 0;
            uint8_t r = 90, g = 90, b = 90;
            parseColor(m["color"] | "", r, g, b);
            s.r = r; s.g = g; s.b = b;
        }
    }
    status_ = "K2: slots atualizados";
}
void CrealityBackend::onMsg(uint8_t* payload, size_t len) {
    const char* p = (const char*)payload;
    JsonDocument doc;
    if (deserializeJson(doc, payload, len)) return;
    if (doc["boxsInfo"].is<JsonObject>()) applyBoxsInfo(doc["boxsInfo"].as<JsonObjectConst>());
    if (doc["err"].is<JsonObject>()) {
        int ec = doc["err"]["errcode"] | 0;
        if (ec) { status_ = String("K2 error ") + ec; }
    }
    (void)p;
}
void CrealityBackend::onEvent(WStype_t type, uint8_t* payload, size_t len) {
    switch (type) {
        case WStype_CONNECTED:    connected_ = true;  status_ = "K2: ligado"; break;
        case WStype_DISCONNECTED: connected_ = false; status_ = "K2: desligado"; break;
        case WStype_TEXT:         onMsg(payload, len); break;
        default: break;
    }
}
bool CrealityBackend::sendDoc(JsonDocument& d) {
    if (!connected_) return false;
    String out; serializeJson(d, out);
    Serial.printf("[creality] -> %s\n", out.c_str());
    return ws_.sendTXT(out);
}

void CrealityBackend::begin(const PrinterCfg& cfg) {
    for (int i = 0; i < 5; i++) slots_[i] = SlotState{};
    connected_ = false;
    status_ = "K2: connecting...";
    ws_.begin(cfg.host, 9999, "/");
    ws_.onEvent([this](WStype_t t, uint8_t* p, size_t l) { onEvent(t, p, l); });
    ws_.setReconnectInterval(10000);           // printer offline -> stop hammering it
    ws_.enableHeartbeat(15000, 3000, 2);
}

void CrealityBackend::loop() {
    ws_.loop();
    if (connected_ && millis() - lastReq_ > 5000) { lastReq_ = millis(); refresh(); }
}

void CrealityBackend::stop() {
    ws_.disconnect();
    connected_ = false;
    status_ = "K2: parado";
}

bool CrealityBackend::connected() { return connected_; }
String CrealityBackend::status()  { return status_; }
const char* CrealityBackend::slotLabel(int i) {
    const char* n = CREALITY_SLOTS[i < 5 ? i : 0].name;
    return n ? n : i18n::T(S_HOLDER);
}
const SlotState& CrealityBackend::slot(int i) { return slots_[i < 5 ? i : 0]; }

void CrealityBackend::refresh() {
    JsonDocument d;
    d["method"] = "get";
    d["params"]["boxsInfo"] = 1;
    sendDoc(d);
}

bool CrealityBackend::assign(int idx, const TagInfo& t) {
    if (idx < 0 || idx >= 5) return false;
    JsonDocument d;
    d["method"] = "set";
    JsonObject m = d["params"]["modifyMaterial"].to<JsonObject>();
    m["boxId"]   = CREALITY_SLOTS[idx].box;
    m["id"]      = CREALITY_SLOTS[idx].slot;
    m["rfid"]    = "0";
    m["type"]    = t.material;
    m["vendor"]  = t.brand;
    m["name"]    = t.material;
    m["color"]   = t.colorHexCreality();
    m["minTemp"] = t.nozMin ? t.nozMin : 190;
    m["maxTemp"] = t.nozMax ? t.nozMax : 230;
    bool ok = sendDoc(d);
    // slotLabel, not the raw name: slot 0 is the external holder and its name
    // is deliberately null, so concatenating it here produced a String that
    // Arduino invalidates - an empty status line where a report should be.
    status_ = ok ? (String("sent -> ") + slotLabel(idx)) : "send failed";
    if (ok) refresh();
    return ok;
}
