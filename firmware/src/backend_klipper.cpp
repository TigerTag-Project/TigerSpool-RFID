#include "backend_klipper.h"
#include "version.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <algorithm>

namespace {
    // Request ids, so an answer is routed by what it answers.
    const int ID_QUERY = 1001, ID_LIST = 1002, ID_IDENTIFY = 1003, ID_SCRIPT = 201;

    // "#RRGGBB", "RRGGBB" or "RRGGBBAA". Happy Hare may also hold a colour NAME
    // ("red"); like Tiger Studio, that is left unread rather than guessed at.
    bool hexRGB(const char* s, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (!s) return false;
        if (*s == '#') s++;
        size_t n = strlen(s);
        if (n != 6 && n != 8) return false;
        for (size_t i = 0; i < n; i++) if (!isxdigit((unsigned char)s[i])) return false;
        auto hx = [](char c) -> int { return isdigit((unsigned char)c) ? c - '0' : (c | 0x20) - 'a' + 10; };
        r = hx(s[0]) * 16 + hx(s[1]);
        g = hx(s[2]) * 16 + hx(s[3]);
        b = hx(s[4]) * 16 + hx(s[5]);
        return true;
    }

    // A G-code parameter value may not carry spaces, newlines or ';' - Tiger
    // Studio's gv(): each run of them becomes one '_', and it stops at 64.
    String gv(const String& in) {
        String o; o.reserve(in.length());
        bool gap = false;
        for (size_t i = 0; i < in.length() && o.length() < 64; i++) {
            const char c = in[i];
            if (isspace((unsigned char)c) || c == ';') { if (!gap) o += '_'; gap = true; }
            else { o += c; gap = false; }
        }
        return o;
    }

    // What a slot shows: gv() turned the label's spaces into '_' on the way
    // out, and the printer hands it back that way. "PLA High Speed" is what
    // the spool says and what the screen should say.
    String shown(const char* mat) {
        String s(mat ? mat : "");
        s.replace('_', ' ');
        return s;
    }

    // Moonraker pushes notify_proc_stat_update to every client once a second,
    // and a busy printer adds more. None of them is read here, so they are
    // recognised before anything is parsed.
    bool isNotify(const uint8_t* p, size_t len) {
        static const char NEEDLE[] = "\"method\":\"notify_";
        const size_t n = sizeof(NEEDLE) - 1;
        const size_t lim = len < 64 ? len : 64;          // it is near the start
        for (size_t i = 0; i + n <= lim; i++)
            if (!memcmp(p + i, NEEDLE, n)) return true;
        return false;
    }
}

void KlipperBackend::detect(JsonArrayConst objects) {
    bool mmu = false;
    int extruders[KMAX]; int nExt = 0;
    int nLane = 0;
    for (JsonVariantConst v : objects) {
        const char* name = v | "";
        if (!strcmp(name, "mmu")) mmu = true;
        // One slot per AFC lane. A unit can publish the same lane twice, as
        // `AFC_stepper <lane>` and `AFC_lane <lane>` (a CANVAS on an OpenCentauri
        // CC1 does): the lane is listed once, in the order first met, and read
        // from the `AFC_lane` object when there is one.
        const bool isLane = !strncmp(name, "AFC_lane ", 9), isStep = !strncmp(name, "AFC_stepper ", 12);
        if (isLane || isStep) {
            const String lane = String(name + (isLane ? 9 : 12));
            if (lane.isEmpty() || lane.indexOf(' ') >= 0) continue;
            int at = -1;
            for (int i = 0; i < nLane; i++) if (lane_[i] == lane) { at = i; break; }
            if (at < 0 && nLane < KMAX) { at = nLane++; lane_[at] = lane; laneObj_[at] = name; }
            else if (at >= 0 && isLane) laneObj_[at] = name;
        }
        // extruder, extruder1, extruder2...: plain extruders, by number.
        if (!strncmp(name, "extruder", 8) && nExt < KMAX) {
            const char* d = name + 8;
            bool digits = true;
            for (const char* c = d; *c; c++) if (!isdigit((unsigned char)*c)) { digits = false; break; }
            if (digits) extruders[nExt++] = *d ? atoi(d) : 0;
        }
    }

    for (int i = 0; i < KMAX; i++) slots_[i] = SlotState{};
    if (mmu) {
        // Gates are counted from the status, not the object list.
        sys_ = SYS_MMU;
        nSlots_ = 0;
    } else if (nLane) {
        sys_ = SYS_AFC;
        nSlots_ = nLane;
        for (int i = 0; i < nLane; i++) snprintf(labels_[i], sizeof(labels_[i]), "%d", i + 1);
    } else {
        sys_ = SYS_EXTRUDERS;
        if (!nExt) extruders[nExt++] = 0;
        std::sort(extruders, extruders + nExt);
        nSlots_ = nExt;
        for (int i = 0; i < nExt; i++) {
            lane_[i] = extruders[i] ? String("extruder") + extruders[i] : String("extruder");
            if (nExt > 1) snprintf(labels_[i], sizeof(labels_[i]), "T%d", i);
            else          snprintf(labels_[i], sizeof(labels_[i]), "E1");
        }
    }
    Serial.printf("[klipper] %s, %d slot(s)\n",
                  sys_ == SYS_MMU ? "Happy Hare" : sys_ == SYS_AFC ? "AFC" : "plain extruders", nSlots_);
    status_ = sys_ == SYS_EXTRUDERS ? "Klipper: no AFC or Happy Hare - slots are read-only"
                                    : "Klipper: connected";
}

void KlipperBackend::applyStatus(JsonObjectConst st) {
    if (sys_ == SYS_MMU) {
        JsonObjectConst m = st["mmu"];
        if (m.isNull()) return;
        JsonArrayConst gs = m["gate_status"], gm = m["gate_material"], gc = m["gate_color"];
        int n = m["num_gates"] | (int)gs.size();
        if (n > KMAX) n = KMAX;
        if (n != nSlots_) {
            nSlots_ = n;
            for (int g = 0; g < n; g++) snprintf(labels_[g], sizeof(labels_[g]), "%d", g);
        }
        const int cur = m["gate"] | -1;
        for (int g = 0; g < n; g++) {
            SlotState& s = slots_[g];
            // gate_status: 0 empty, 1 / 2 loaded, -1 unknown.
            const bool empty = (gs[g] | -1) == 0;
            const char* mat = gm[g] | "";
            s.type  = empty ? String() : shown(mat);
            s.known = !empty && *mat;
            s.r = s.g = s.b = 90;
            if (!empty) hexRGB(gc[g] | "", s.r, s.g, s.b);
            s.selected = cur == g;
        }
        status_ = "Klipper: slots updated";
    } else if (sys_ == SYS_AFC) {
        const char* cur = st["AFC"]["current_lane"] | "";
        for (int i = 0; i < nSlots_; i++) {
            JsonObjectConst o = st[laneObj_[i]];
            if (o.isNull()) continue;
            SlotState& s = slots_[i];
            const bool empty = o["load"].is<bool>() && o["prep"].is<bool>()
                            && !o["load"].as<bool>() && !o["prep"].as<bool>();
            const char* mat = o["material"] | "";
            s.type  = empty ? String() : shown(mat);
            s.known = !empty && *mat;
            s.r = s.g = s.b = 90;
            if (!empty) hexRGB(o["color"] | "", s.r, s.g, s.b);
            s.selected = lane_[i] == cur || (o["tool_loaded"] | false);
        }
        status_ = "Klipper: slots updated";
    } else if (sys_ == SYS_EXTRUDERS) {
        const char* active = st["toolhead"]["extruder"] | "";
        for (int i = 0; i < nSlots_; i++) slots_[i].selected = lane_[i] == active;
    }
}

void KlipperBackend::onMsg(uint8_t* payload, size_t len) {
    if (isNotify(payload, len)) return;
    JsonDocument d;
    if (deserializeJson(d, payload, len)) return;
    const int id = d["id"] | 0;

    // A refused request comes back as an error object. Saying so is the only
    // way to tell "sent" from "done" - and a Klippy that is restarting answers
    // every query this way until it is back.
    if (!d["error"].isNull()) {
        const char* m = d["error"]["message"] | "error";
        Serial.printf("[klipper] error (request %d): %s\n", id, m);
        if (id == ID_IDENTIFY) status_ = "Klipper: API key refused";
        else status_ = String("Klipper: ") + m;
        // The object list may have changed: detect again when Klippy is back.
        if (id == ID_QUERY) sys_ = SYS_UNKNOWN;
        return;
    }
    if (id == ID_IDENTIFY) {
        lastReq_ = 0;                       // accepted: ask for the objects now
        return;
    }
    if (id == ID_LIST) {
        detect(d["result"]["objects"]);
        lastReq_ = 0;                       // read the slots straight away
        return;
    }
    JsonObjectConst st = d["result"]["status"];
    if (!st.isNull()) applyStatus(st);
}

void KlipperBackend::onEvent(WStype_t type, uint8_t* payload, size_t len) {
    switch (type) {
        case WStype_CONNECTED: {
            connected_ = true;
            status_ = "Klipper: connected";
            Serial.println("[klipper] websocket connected");
            if (apiKey_.length()) {
                JsonDocument p;
                p["client_name"] = "TigerSpool";
                p["version"] = TIGERSPOOL_FW_VERSION;
                p["type"] = "other";
                p["url"] = "https://github.com/TigerTag-Project/TigerSpool-RFID";
                p["api_key"] = apiKey_;
                rpc("server.connection.identify", p, ID_IDENTIFY);
                // Nothing else until the key is accepted: a request sent
                // before the answer is refused as unauthorised.
                lastReq_ = millis();
            } else {
                lastReq_ = 0;
            }
            break;
        }
        case WStype_DISCONNECTED:
            if (connected_) Serial.println("[klipper] websocket closed");
            connected_ = false;
            sys_ = SYS_UNKNOWN;
            status_ = "Klipper: disconnected";
            break;
        case WStype_TEXT: onMsg(payload, len); break;
        case WStype_ERROR:
            Serial.printf("[klipper] websocket error: %.*s\n", (int)len,
                          payload ? (const char*)payload : "");
            break;
        default: break;
    }
}

bool KlipperBackend::rpc(const char* method, JsonDocument& params, int id) {
    if (!connected_) return false;
    JsonDocument d;
    d["jsonrpc"] = "2.0";
    d["method"]  = method;
    if (!params.isNull()) d["params"] = params;
    d["id"] = id;
    String out; serializeJson(d, out);
    // Never the API key in the log.
    if (id != ID_IDENTIFY && id != ID_QUERY) Serial.printf("[klipper] -> %s\n", out.c_str());
    return ws_.sendTXT(out);
}

void KlipperBackend::begin(const PrinterCfg& cfg) {
    for (int i = 0; i < KMAX; i++) { slots_[i] = SlotState{}; lane_[i] = laneObj_[i] = String(); }
    nSlots_ = 0;
    sys_ = SYS_UNKNOWN;
    connected_ = false;
    apiKey_ = cfg.cc;
    status_ = "Klipper: connecting...";
    // No Origin header and no subprotocol: Moonraker refuses the library's
    // default `Origin: file://` with a 403 and the client then fires no event
    // at all. See SnapmakerBackend::begin, where this was found.
    ws_.setExtraHeaders("");
    ws_.begin(hostOnly(cfg.host), portOf(cfg.host, 7125), "/websocket", "");
    ws_.onEvent([this](WStype_t t, uint8_t* p, size_t l) { onEvent(t, p, l); });
    ws_.setReconnectInterval(10000);
    ws_.enableHeartbeat(15000, 3000, 2);
}

void KlipperBackend::loop() {
    ws_.loop();
    if (connected_ && (lastReq_ == 0 || millis() - lastReq_ > 5000)) { lastReq_ = millis(); refresh(); }
}

void KlipperBackend::stop() {
    ws_.disconnect();
    connected_ = false;
    sys_ = SYS_UNKNOWN;
    status_ = "Klipper: stopped";
}

bool KlipperBackend::connected() { return connected_; }
String KlipperBackend::status()  { return status_; }
const char* KlipperBackend::slotLabel(int i) { return (i >= 0 && i < nSlots_) ? labels_[i] : "?"; }
const SlotState& KlipperBackend::slot(int i) { return slots_[(i >= 0 && i < KMAX) ? i : 0]; }

void KlipperBackend::refresh() {
    JsonDocument p;
    if (sys_ == SYS_UNKNOWN) {
        rpc("printer.objects.list", p, ID_LIST);
        return;
    }
    JsonObject o = p["objects"].to<JsonObject>();
    if (sys_ == SYS_MMU) {
        JsonArray f = o["mmu"].to<JsonArray>();
        for (const char* k : { "num_gates", "gate", "gate_status", "gate_material", "gate_color" }) f.add(k);
    } else if (sys_ == SYS_AFC) {
        o["AFC"].to<JsonArray>().add("current_lane");
        for (int i = 0; i < nSlots_; i++) {
            JsonArray f = o[laneObj_[i]].to<JsonArray>();
            for (const char* k : { "material", "color", "load", "prep", "tool_loaded" }) f.add(k);
        }
    } else {
        o["toolhead"].to<JsonArray>().add("extruder");
    }
    rpc("printer.objects.query", p, ID_QUERY);
}

bool KlipperBackend::assign(int idx, const TagInfo& t) {
    if (idx < 0 || idx >= nSlots_ || !connected_) return false;
    if (sys_ == SYS_EXTRUDERS || sys_ == SYS_UNKNOWN) {
        // Klipper keeps no filament identity on a plain extruder: there is no
        // command that would store this spool anywhere on the printer.
        status_ = "Klipper: no AFC or Happy Hare - nothing to write to";
        return false;
    }
    char col[7];
    snprintf(col, sizeof(col), "%02X%02X%02X", t.r, t.g, t.b);
    const String mat = gv(t.material);

    String script;
    if (sys_ == SYS_MMU) {
        script = String("MMU_GATE_MAP GATE=") + idx;
        if (mat.length()) script += " MATERIAL=" + mat;
        script += String(" COLOR=") + col;
    } else {
        const String lane = gv(lane_[idx]);
        if (mat.length()) script = "SET_MATERIAL LANE=" + lane + " MATERIAL=" + mat + "\n";
        script += "SET_COLOR LANE=" + lane + " COLOR=" + col;
    }

    JsonDocument p;
    p["script"] = script;
    if (!rpc("printer.gcode.script", p, ID_SCRIPT)) { status_ = "Klipper: send failed"; return false; }
    status_ = String("Klipper: sent -> ") + labels_[idx];
    // Read it back half a second from now rather than waiting here: what the
    // printer reports is the only proof that the macro did what it was asked.
    lastReq_ = millis() - 4500;
    return true;
}
