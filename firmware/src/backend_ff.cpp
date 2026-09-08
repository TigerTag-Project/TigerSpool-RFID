#include "backend_ff.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace {
    // What is left here is stateless: tables and pure functions, shared by
    // every instance because they describe the PRINTER MODEL, not a printer.
    // Everything that describes one machine now lives in the object.
    //
    // The material station's four slots. Named as the printer names them:
    // station number then position, so a second station would be 2A..2D.
    const char* LABELS[4] = { "1A", "1B", "1C", "1D" };

    // TigerTag material -> the Creator 5's list of 21 materials
    String ffMaterial(const String& in) {
        String s = in; s.toUpperCase();
        auto has = [&](const char* k) { return s.indexOf(k) >= 0; };
        if (has("PPS"))                 return "PPS-CF";
        if (has("PAHT"))               return "PAHT-CF";
        if (has("PA") && has("CF"))    return "PA-CF";
        if (has("PET") && has("CF") && !has("PETG")) return "PET-CF";
        if (has("PETG") && has("CF"))  return "PETG-CF";
        if (has("PLA") && has("CF"))   return "PLA-CF";
        if (has("PC") && has("ABS"))   return "PC-ABS";
        if (has("TPU")) { if (has("64")) return "TPU-64D";
                          if (has("90")) return "TPU-90A"; return "TPU-95A"; }
        if (has("SILK"))               return "SILK";
        if (has("PETG"))               return "PETG";
        if (has("PLA"))                return "PLA";
        if (has("ABS"))                return "ABS";
        if (has("ASA"))                return "ASA";
        if (has("HIPS"))              return "HIPS";
        if (has("PVA"))                return "PVA";
        if (has("PC"))                 return "PC";
        if (has("PA"))                 return "PA";
        return "PLA";
    }

    // The Creator 5's own 24-colour palette. The printer accepts nothing else:
    // any other hex is silently ignored and the slot reverts to white, while the
    // call still answers success. The tag's colour is snapped to the nearest
    // entry here - see docs/PRINTER-COMPATIBILITY.md.
    struct FfColor { uint32_t rgb; const char* name; };
    const FfColor FF_PALETTE[24] = {
        { 0xFFFFFF, "White" },      { 0xFFF245, "Yellow" },     { 0xDEF578, "Light Green" },
        { 0x21CC3D, "Green" },      { 0x167A4B, "Dark Green" }, { 0x156682, "Teal" },
        { 0x24E4A0, "Cyan" },       { 0x7BD9F0, "Light Blue" }, { 0x4CAAF8, "Blue" },
        { 0x2E54DD, "Dark Blue" },  { 0x48358C, "Purple" },     { 0xA341F7, "Violet" },
        { 0xF435F6, "Magenta" },    { 0xD5B4DE, "Pink" },       { 0xFA6173, "Coral" },
        { 0xF82D29, "Red" },        { 0x805003, "Brown" },      { 0xF9903B, "Orange" },
        { 0xFCEBD7, "Cream" },      { 0xD5C5A1, "Tan" },        { 0xB17C38, "Dark Brown" },
        { 0x8C8C89, "Gray" },       { 0xBEBEBE, "Light Gray" }, { 0x1B1B1B, "Black" },
    };

    // Nearest colour in the palette ("redmean" distance, a good perceptual
    // approximation for the price)
    const FfColor& ffNearest(uint8_t r, uint8_t g, uint8_t b) {
        int best = 0; long bestD = 0x7fffffffL;
        for (int i = 0; i < 24; i++) {
            long pr = (FF_PALETTE[i].rgb >> 16) & 0xFF;
            long pg = (FF_PALETTE[i].rgb >> 8) & 0xFF;
            long pb =  FF_PALETTE[i].rgb        & 0xFF;
            long dr = (long)r - pr, dg = (long)g - pg, db = (long)b - pb;
            long rm = ((long)r + pr) / 2;
            long d  = (((512 + rm) * dr * dr) >> 8) + 4 * dg * dg + (((767 - rm) * db * db) >> 8);
            if (d < bestD) { bestD = d; best = i; }
        }
        return FF_PALETTE[best];
    }

    void parseHex(const char* s, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (!s || !*s) return;
        if (*s == '#') s++;
        long v = strtol(s, nullptr, 16);
        r = (v >> 16) & 0xFF; g = (v >> 8) & 0xFF; b = v & 0xFF;
    }
}

// http://host:8898<path>, JSON in and JSON out. The printer's firmware sends
// Content-Type "appliation/json" - their typo, not ours.
String FlashForgeC5Backend::post(const String& path, const String& body, int& httpCode) {
    if (WiFi.status() != WL_CONNECTED) { httpCode = -1; return ""; }
    WiFiClient client;
    HTTPClient http;
    String url = "http://" + host_ + ":8898" + path;
    if (!http.begin(client, url)) { httpCode = -2; return ""; }
    http.setTimeout(4000);
    http.addHeader("Content-Type", "application/json");
    httpCode = http.POST((uint8_t*)body.c_str(), body.length());
    String resp = (httpCode > 0) ? http.getString() : String();
    http.end();
    return resp;
}

String FlashForgeC5Backend::authBody() {
    JsonDocument d;
    d["serialNumber"] = sn_;
    d["checkCode"]    = cc_;
    String s; serializeJson(d, s); return s;
}

void FlashForgeC5Backend::begin(const PrinterCfg& cfg) {
    host_ = cfg.host; sn_ = cfg.sn; cc_ = cfg.cc;
    // FlashForge's API wants the serial prefixed with "SN"; the TigerTag import
    // hands it over without one
    if (sn_.length() && !sn_.startsWith("SN")) sn_ = "SN" + sn_;
    for (int i = 0; i < 4; i++) slots_[i] = SlotState{};
    auth_ = false;
    status_ = "FF: a validar...";

    tryAuth();
}

void FlashForgeC5Backend::stop() {
    auth_ = false;
    for (int i = 0; i < 4; i++) slots_[i] = SlotState{};
    status_ = "FF: stopped";
}

void FlashForgeC5Backend::tryAuth() {
    int code;
    String resp = post("/checkCode", authBody(), code);
    Serial.printf("[flashforge] /checkCode http=%d resp=%s\n", code, resp.c_str());
    JsonDocument d;
    if (code == 200 && !deserializeJson(d, resp)) {
        int c = d["code"] | -99;
        if (c == 0)       { auth_ = true;  status_ = "FF: autenticado"; }
        else if (c == -2) status_ = "FF: Modo LAN desligado";
        else if (c == 1)  status_ = "FF: access code errado";
        else if (c == 3)  status_ = "FF: not authorised";
        else if (c == 5)  status_ = "FF: serial errado";
        else              status_ = String("FF: checkCode code ") + c;
    } else {
        status_ = String("FF: no answer (") + code + ")";
    }
    lastAuth_ = millis();
    if (auth_) refresh();
}

void FlashForgeC5Backend::loop() {
    if (auth_) {
        if (millis() - lastReq_ > 6000) { lastReq_ = millis(); refresh(); }
    } else if (millis() - lastAuth_ > 20000) {   // re-authenticate every 20 s
        tryAuth();
    }
}

bool FlashForgeC5Backend::connected() { return auth_; }
String FlashForgeC5Backend::status()  { return status_; }
const char* FlashForgeC5Backend::slotLabel(int i) { return LABELS[i & 3]; }
const SlotState& FlashForgeC5Backend::slot(int i) { return slots_[i & 3]; }

void FlashForgeC5Backend::refresh() {
    int code;
    String resp = post("/detail", authBody(), code);
    if (code != 200) { status_ = String("FF: /detail http ") + code; return; }
    JsonDocument d;
    if (deserializeJson(d, resp)) { status_ = "FF: /detail json?"; return; }

    // Dump the material station (for debugging colour/material)
    { String ms; serializeJson(d["detail"]["matlStationInfo"], ms);
      Serial.printf("[flashforge] matlStationInfo: %.*s\n", (int)(ms.length() > 320 ? 320 : ms.length()), ms.c_str()); }

    JsonArrayConst si = d["detail"]["matlStationInfo"]["slotInfos"].as<JsonArrayConst>();
    if (si.isNull()) { status_ = "FF: no material station"; return; }
    for (JsonObjectConst s : si) {
        int id = s["slotId"] | 0;             // 1-based
        if (id < 1 || id > 4) continue;
        SlotState& st = slots_[id - 1];
        const char* mn = s["materialName"] | "";
        const char* mc = s["materialColor"] | "";
        st.type = mn;
        st.known = strlen(mn) > 0;
        uint8_t r = 90, g = 90, b = 90;
        parseHex(mc, r, g, b);
        st.r = r; st.g = g; st.b = b;
        Serial.printf("[flashforge] slot %d: '%s' color '%s'\n", id, mn, mc);
    }
    int cur = d["detail"]["matlStationInfo"]["currentSlot"] | 0;
    for (int i = 0; i < 4; i++) slots_[i].selected = (cur == i + 1);
    status_ = "FF: slots atualizados";
}

bool FlashForgeC5Backend::assign(int idx, const TagInfo& t) {
    if (idx < 0 || idx >= 4) return false;
    String mt = ffMaterial(t.material);

    // Snap the tag's colour to the nearest entry in the Creator 5's palette.
    // The C5 demands "#RRGGBB" in UPPERCASE and WITH the '#' (the public doc is
    // wrong. Any other value is silently ignored and the slot reverts to
    // #FFFFFF) even though it answers code:0.
    const FfColor& pc = ffNearest(t.r, t.g, t.b);
    char rgb[9]; snprintf(rgb, sizeof(rgb), "#%06X", pc.rgb);
    Serial.printf("[flashforge] cor tag #%02X%02X%02X -> paleta %s %s\n", t.r, t.g, t.b, pc.name, rgb);

    JsonDocument d;
    d["serialNumber"] = sn_;
    d["checkCode"]    = cc_;
    JsonObject pl = d["payload"].to<JsonObject>();
    pl["cmd"] = "msConfig_cmd";
    JsonObject a = pl["args"].to<JsonObject>();
    a["slot"] = idx + 1;                       // 1-based
    a["mt"]   = mt;
    a["rgb"]  = rgb;                           // "#RRGGBB", UPPERCASE, with the '#' (C5 palette)
    String body; serializeJson(d, body);
    Serial.printf("[flashforge] -> /control %s\n", body.c_str());

    int code;
    String resp = post("/control", body, code);
    Serial.printf("[flashforge] <- http=%d %s\n", code, resp.c_str());
    JsonDocument r;
    bool ok = (code == 200) && !deserializeJson(r, resp) && ((r["code"] | -1) == 0);
    status_ = ok ? (String("sent -> ") + LABELS[idx] + " " + mt) : "FF: send failed";
    if (ok) { delay(150); refresh(); }        // confirma relendo (cmd desconhecido e ACKed na mesma)
    return ok;
}
