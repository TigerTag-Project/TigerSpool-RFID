#include "backend_snapmaker.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

namespace {
    // One extruder per slot. The Moonraker script addresses them as
    // CONFIG_EXTRUDER 0..3, and the machine's own UI calls them E1..E4.
    const char* SLOTS[4] = { "E1", "E2", "E3", "E4" };

    void hexRGBA(const char* s, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (!s) return;
        if (*s == '#') s++;
        if (strlen(s) < 6) return;
        auto hx = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            c |= 0x20; return (c >= 'a' && c <= 'f') ? c - 'a' + 10 : 0;
        };
        r = hx(s[0]) * 16 + hx(s[1]);
        g = hx(s[2]) * 16 + hx(s[3]);
        b = hx(s[4]) * 16 + hx(s[5]);
    }

    // G-code arguments are separated by spaces, so strip spaces out of the
    // vendor and type
    String noSpace(const String& in) {
        String o; o.reserve(in.length());
        for (size_t i = 0; i < in.length(); i++) o += (in[i] == ' ' ? '_' : in[i]);
        return o;
    }
}

// print_task_config: 4 parallel arrays (color_rgba / type / vendor)
void SnapmakerBackend::applyConfig(JsonObjectConst c) {
    JsonArrayConst col = c["filament_color_rgba"];
    JsonArrayConst typ = c["filament_type"];
    JsonArrayConst ven = c["filament_vendor"];
    JsonArrayConst sub = c["filament_sub_type"];
    if (col.isNull() && typ.isNull()) return;
    for (int i = 0; i < 4; i++) {
        SlotState& s = slots_[i];
        const char* t  = typ.isNull() ? "" : (const char*)(typ[i] | "");
        const char* cc = col.isNull() ? "" : (const char*)(col[i] | "");
        // The vendor was in every report and never read, so four spools
        // that Moonraker names R3D, Generic, Generic and Snapmaker all
        // showed a dash. The sub-type joins the family the way the
        // printer's own screen writes it: "PLA Silk", not "PLA".
        const char* v  = ven.isNull() ? "" : (const char*)(ven[i] | "");
        const char* st = sub.isNull() ? "" : (const char*)(sub[i] | "");
        s.type  = strlen(st) ? (String(t) + " " + st) : String(t);
        s.brand = v;
        s.known = strlen(t) > 0;
        uint8_t r = 90, g = 90, b = 90;
        hexRGBA(cc, r, g, b);
        s.r = r; s.g = g; s.b = b;
    }
    status_ = "Snap: slots updated";
}

void SnapmakerBackend::onMsg(uint8_t* payload, size_t len) {
    JsonDocument d;
    if (deserializeJson(d, payload, len)) return;
    // query answer -> result.status.print_task_config
    JsonObjectConst r1 = d["result"]["status"]["print_task_config"];
    if (!r1.isNull()) { applyConfig(r1); return; }
    // push -> {"method":"notify_status_update","params":[{print_task_config:{...}}, t]}
    if (String(d["method"] | "") == "notify_status_update") {
        JsonObjectConst r2 = d["params"][0]["print_task_config"];
        if (!r2.isNull()) applyConfig(r2);
    }
}

void SnapmakerBackend::onEvent(WStype_t type, uint8_t* payload, size_t len) {
    switch (type) {
        case WStype_CONNECTED:
            connected_ = true;  status_ = "Snap: connected";
            Serial.println("[snap] websocket connected");
            break;
        case WStype_DISCONNECTED:
            if (connected_) Serial.println("[snap] websocket closed");
            connected_ = false; status_ = "Snap: disconnected";
            break;
        case WStype_TEXT: onMsg(payload, len); break;
        case WStype_ERROR:
            // Silence here was the whole problem: a Moonraker that was up
            // and answering over HTTP looked identical to one that was off,
            // because nothing said which of the two had happened.
            Serial.printf("[snap] websocket error: %.*s\n", (int)len,
                          payload ? (const char*)payload : "");
            break;
        default: break;
    }
}

bool SnapmakerBackend::sendRaw(String s) {
    if (!connected_) return false;
    Serial.printf("[snap] -> %s\n", s.c_str());
    return ws_.sendTXT(s);
}

void SnapmakerBackend::begin(const PrinterCfg& cfg) {
    for (int i = 0; i < 4; i++) slots_[i] = SlotState{};
    connected_ = false;
    status_ = "Snap: connecting...";
    // No Origin header, and no subprotocol.
    //
    // Moonraker answered 403 Forbidden at the upgrade - not a timeout, not a
    // closed port, a flat refusal - and the client fired NO EVENT AT ALL, so a
    // printer answering perfectly over HTTP looked exactly like one switched
    // off. It took turning on the library's own trace to see the 403.
    //
    // The cause is a default nobody would look for: this library sends
    // `Origin: file://` on every handshake, and Moonraker's authorization
    // component refuses an origin that is not in cors_domains. The same
    // upgrade from curl, with no Origin, is accepted with 101. setExtraHeaders
    // with an empty string is what removes it; the library skips the line when
    // the string is empty. "arduino" goes with it for the same reason.
    ws_.setExtraHeaders("");
    ws_.begin(cfg.host, 7125, "/websocket", "");
    ws_.onEvent([this](WStype_t t, uint8_t* p, size_t l) { onEvent(t, p, l); });
    ws_.setReconnectInterval(10000);
    ws_.enableHeartbeat(15000, 3000, 2);
}

void SnapmakerBackend::loop() {
    ws_.loop();
    if (connected_ && millis() - lastReq_ > 5000) { lastReq_ = millis(); refresh(); }
}

void SnapmakerBackend::stop() {
    ws_.disconnect();
    connected_ = false;
    status_ = "Snap: parado";
}

bool SnapmakerBackend::connected() { return connected_; }
String SnapmakerBackend::status()  { return status_; }
const char* SnapmakerBackend::slotLabel(int i) { return SLOTS[(i >= 0 && i < 4) ? i : 0]; }
const SlotState& SnapmakerBackend::slot(int i) { return slots_[(i >= 0 && i < 4) ? i : 0]; }

void SnapmakerBackend::refresh() {
    sendRaw("{\"jsonrpc\":\"2.0\",\"method\":\"printer.objects.query\","
            "\"params\":{\"objects\":{\"print_task_config\":null}},\"id\":1001}");
}

bool SnapmakerBackend::assign(int idx, const TagInfo& t) {
    if (idx < 0 || idx >= 4 || !connected_) return false;
    // The printer keeps the family and the variant in two fields, and its own
    // screen shows them as "PLA Basic" and "PLA Silk". A tag carries one
    // string, "PLA High Speed", so it is split at the first space: family into
    // FILAMENT_TYPE, the rest into FILAMENT_SUBTYPE. Sending it whole wrote
    // PLA_High_Speed into the type field with underscores in it - accepted,
    // and wrong on the printer's display ever after.
    String vend = noSpace(t.brand.length() ? t.brand : String("Generic"));
    String full = t.material.length() ? t.material : String("PLA");
    int sp = full.indexOf(' ');
    String mat = noSpace(sp > 0 ? full.substring(0, sp) : full);
    String sub = noSpace(sp > 0 ? full.substring(sp + 1) : String());
    char script[220];
    snprintf(script, sizeof(script),
        "SET_PRINT_FILAMENT_CONFIG CONFIG_EXTRUDER=%d VENDOR=%s FILAMENT_TYPE=%s "
        "FILAMENT_SUBTYPE=%s FILAMENT_COLOR_RGBA=%02X%02X%02XFF",
        idx, vend.c_str(), mat.c_str(), sub.c_str(), t.r, t.g, t.b);

    JsonDocument d;
    d["jsonrpc"] = "2.0";
    d["method"]  = "printer.gcode.script";
    d["params"]["script"] = script;
    d["id"] = 201;
    String out; serializeJson(d, out);

    bool ok = sendRaw(out);
    status_ = ok ? (String("sent -> ") + SLOTS[idx]) : "send failed";
    if (ok) { delay(200); refresh(); }
    return ok;
}
