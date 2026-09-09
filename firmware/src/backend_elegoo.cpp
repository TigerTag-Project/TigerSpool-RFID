#include "backend_elegoo.h"
#include "i18n.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

namespace {

// Internally: 0 is the mono extruder, 1..4 are the Canvas trays.
//
// What the UI SEES is one or the other, never both. On a Centauri Carbon 2 the
// hub and the single spool are the same feed path - plug the Canvas in and the
// external holder stops existing, unplug it and the four trays do. Showing an
// empty Ext. beside four full trays invents a fifth place a spool can be.
//
// So the exposed list is four trays when the hub is connected, and one external
// spool when it is not; mapUi() is the whole of the difference.
const char* const NAMES[ElegooBackend::ESLOTS] = { "", "S1", "S2", "S3", "S4" };

void applyTray(SlotState& s, JsonObjectConst t) {
    const char* type  = t["filament_type"] | "";
    const char* name  = t["filament_name"] | "";
    const char* brand = t["brand"] | "";
    const char* col   = t["filament_color"] | "";
    // `status` is 1 for an occupied slot, but a slot can carry a colour and a
    // type with status 0 on some firmwares - so the presence of a type is what
    // decides, and status only adds to it.
    s.known    = (t["status"] | 0) != 0 || strlen(type) > 0;
    // filament_name is the full "PLA Silk"; filament_type is the family. The
    // name is what a person reads off a spool, so it wins when it is there.
    s.type     = strlen(name) ? name : type;
    s.brand    = brand;
    s.selected = (t["status"] | 0) == 1;
    if (col && *col) {
        const char* h = (*col == '#') ? col + 1 : col;
        if (strlen(h) >= 6) {
            long v = strtol(h, nullptr, 16);
            s.r = (v >> 16) & 0xFF; s.g = (v >> 8) & 0xFF; s.b = v & 0xFF;
        }
    }
}

String hexColour(const TagInfo& t) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", t.r, t.g, t.b);
    return String(buf);
}

// The fields the printer actually stores, in the shape both write methods take.
// filament_code 0x0000 is the captured "unknown" value: the slicer sends a code
// from a fifty-entry table keyed by type and name, and a tag carries neither of
// those keys - sending the wrong code would label the spool as a filament it is
// not, where 0x0000 simply says nothing.
String writeParams(int canvasId, int trayId, const TagInfo& t) {
    JsonDocument d;
    d["canvas_id"] = canvasId;
    d["tray_id"] = trayId;
    d["brand"] = t.brand;
    d["filament_type"] = t.material;
    d["filament_name"] = t.material;
    d["filament_code"] = "0x0000";
    d["filament_color"] = hexColour(t);
    d["filament_min_temp"] = t.nozMin ? t.nozMin : 190;
    d["filament_max_temp"] = t.nozMax ? t.nozMax : 230;
    String out; serializeJson(d, out);
    return out;
}

}  // namespace

void ElegooBackend::publish(int method, const String& params) {
if (!mqtt_.connected()) return;
String out = String("{\"id\":") + (++msgId_) + ",\"method\":" + method +
             ",\"params\":" + params + "}";
Serial.printf("[elegoo] -> %d %s\n", method, params.c_str());
mqtt_.publish(topRequest_.c_str(), out.c_str());
}

void ElegooBackend::onResult(JsonObjectConst res, int method) {
if (method == 2005) {
    JsonObjectConst ci = res["canvas_info"];
    JsonArrayConst list = ci["canvas_list"];
    if (list.isNull() || list.size() == 0) { canvas_ = false; return; }
    JsonObjectConst c0 = list[0];
    // `connected` absent means an older firmware that only reports the hub
    // when it is there; a present-and-zero means it is unplugged, and the
    // four trays it still sends are all empty strings. Believing them would
    // wipe four slots on screen every poll.
    canvas_ = !c0["connected"].is<int>() || (c0["connected"] | 0) != 0;
    if (!canvas_) return;
    for (JsonObjectConst t : c0["tray_list"].as<JsonArrayConst>()) {
        int id = t["tray_id"] | -1;
        if (id < 0 || id > 3) continue;
        applyTray(slots_[1 + id], t);
    }
    status_ = "Elegoo: canvas";
    return;
}
if (method == 1061) {
    JsonObjectConst m = res["mono_filament_info"];
    if (!m.isNull()) {
        applyTray(slots_[0], m);
        status_ = "Elegoo: mono";
    }
    return;
}
}

void ElegooBackend::onMqtt(uint8_t* payload, unsigned int len) {
JsonDocument doc;
if (deserializeJson(doc, payload, len)) return;

// The register acknowledgement names its error field "error" and puts "ok"
// in it on success, which is worth knowing before reading it as a failure.
if (doc["error"].is<const char*>() && strcmp(doc["error"] | "", "ok") == 0) {
    Serial.println("[elegoo] registered");
    return;
}
int method = doc["method"] | 0;
JsonObjectConst res = doc["result"];
if (!res.isNull()) {
    int err = res["error_code"] | 0;
    if (err) Serial.printf("[elegoo] method %d error_code %d\n", method, err);
    onResult(res, method);
}
}

void ElegooBackend::begin(const PrinterCfg& cfg) {
    host_ = cfg.host;
    sn_   = cfg.sn;
    // "123456" is the factory access code and the one every printer nobody has
    // touched still has. An empty field in the account means "not changed",
    // not "no password".
    pass_ = cfg.cc.length() ? cfg.cc : String("123456");

    for (int i = 0; i < ESLOTS; i++) slots_[i] = SlotState{};
    connected_ = false;
    canvas_ = false;
    status_ = "Elegoo: connecting...";

    // A fresh client id per connection. The printer keys its unicast response
    // topic on it, so two boxes on one printer must not share one.
    cid_ = String("TTG_") + String(random(1000, 10000));
    rid_ = cid_ + "_req";
    topRequest_  = String("elegoo/") + sn_ + "/" + cid_ + "/api_request";
    topResponse_ = String("elegoo/") + sn_ + "/" + cid_ + "/api_response";
    topStatus_   = String("elegoo/") + sn_ + "/api_status";
    topRegister_ = String("elegoo/") + sn_ + "/api_register";
    topRegResp_  = String("elegoo/") + sn_ + "/" + rid_ + "/register_response";

    mqtt_.setServer(host_.c_str(), 1883);
    // A full 1002 snapshot with a file list runs to a few kilobytes. Nothing
    // here approaches Bambu's 50 KB pushall.
    mqtt_.setBufferSize(8192);
    mqtt_.setKeepAlive(60);
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
    mqtt_.setCallback([this](char*, uint8_t* p, unsigned int l) { onMqtt(p, l); });
    lastTry_ = 0;
}

void ElegooBackend::loop() {
    if (!mqtt_.connected()) {
        connected_ = false;
        if (millis() - lastTry_ < 4000) return;
        lastTry_ = millis();
        Serial.printf("[elegoo] connecting to %s:1883...\n", host_.c_str());
        if (mqtt_.connect(cid_.c_str(), "elegoo", pass_.c_str())) {
            mqtt_.subscribe(topStatus_.c_str());
            mqtt_.subscribe(topResponse_.c_str());
            mqtt_.subscribe(topRegResp_.c_str());
            String reg = String("{\"client_id\":\"") + cid_ +
                         "\",\"request_id\":\"" + rid_ + "\"}";
            mqtt_.publish(topRegister_.c_str(), reg.c_str());
            // The slicer announces itself before anything else and the printer
            // expects it; sending a query first is not refused, but this is the
            // order the official client uses and the one the captures cover.
            publish(1043, "{\"hostname\":\"TigerSpool\"}");
            connected_ = true;
            status_ = "Elegoo: connected";
            Serial.println("[elegoo] connected + subscribed");
            refresh();
        } else {
            status_ = String("Elegoo: MQTT rc=") + mqtt_.state() + " (access code?)";
            Serial.println(status_);
        }
        return;
    }
    mqtt_.loop();
    if (millis() - lastPoll_ > 8000) { lastPoll_ = millis(); refresh(); }
}

void ElegooBackend::stop() {
    mqtt_.disconnect();
    connected_ = false;
    status_ = "Elegoo: stopped";
}

bool ElegooBackend::connected() { return connected_; }
bool ElegooBackend::firstIsExternal() { return !canvas_; }
String ElegooBackend::status()  { return status_; }

// UI index -> internal index. With the hub: 0..3 are trays 1..4. Without it,
// the only slot is the mono spool at 0.
int ElegooBackend::mapUi(int i) const {
    if (!canvas_) return 0;
    return (i >= 0 && i < 4) ? i + 1 : 1;
}

int ElegooBackend::slotCount() { return canvas_ ? 4 : 1; }

const char* ElegooBackend::slotLabel(int i) {
    const char* n = NAMES[mapUi(i)];
    return *n ? n : i18n::T(S_HOLDER);
}

const SlotState& ElegooBackend::slot(int i) { return slots_[mapUi(i)]; }

void ElegooBackend::refresh() {
    // Both, every time. Which one answers with content is how the hub is
    // detected in the first place, and a Canvas can be unplugged between two
    // polls.
    publish(2005, "{}");
    publish(1061, "{}");
}

bool ElegooBackend::assign(int idx, const TagInfo& t) {
    if (idx < 0 || idx >= slotCount()) return false;
    if (!mqtt_.connected()) return false;

    if (!canvas_) {
        // 2003 without the hub answers error_code 1003 and changes nothing, so
        // the mono spool goes through 1055 - the method the official slicer
        // uses in exactly this situation.
        publish(1055, writeParams(0, 0, t));
    } else {
        publish(2003, writeParams(0, mapUi(idx) - 1, t));
    }
    status_ = String("Elegoo: sent -> ") + slotLabel(idx);
    // The printer sends no acknowledgement for the write itself, so the only
    // proof is reading the slot back. The caller does that too; this makes the
    // screen catch up without waiting for the eight-second poll.
    refresh();
    return true;
}
