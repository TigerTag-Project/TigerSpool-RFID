#include "tigertag_cloud.h"
#include "printer.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "net/tls.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <initializer_list>
#include <mbedtls/base64.h>

namespace {
    // TigerTag's public Firebase client config. Not a secret: it is served
    // without authentication at
    // https://tigertag-cdn.web.app/__/firebase/init.json)
    const char* API_KEY = "AIzaSyCkxPTs_Cv0KVLqsZj-UKWWqIY0OtfVpnw";
    const char* PROJECT = "tigertag-connect";
    const uint32_t SYNC_INTERVAL_MS = 5UL * 60 * 1000;   // 5 min
    // Pairing Cloud Functions, used by the QR/link flow (Google sign-in, so
    // no password ever reaches the device)
    const char* PAIR_START = "https://us-central1-tigertag-connect.cloudfunctions.net/pairStart";
    const char* PAIR_POLL  = "https://us-central1-tigertag-connect.cloudfunctions.net/pairPoll";

    Preferences pr;
    String   g_email, g_refresh, g_uid, g_idToken, g_name;
    uint32_t g_tokenAt = 0, g_lastSync = 0, g_bootAt = 0;
    bool     g_changed = false;
    String   g_lastResult = "";
    // When the account last answered anything at all - a token refresh, a
    // sync, a profile read. Green is a claim about THIS, so everything that
    // talks to the account touches it.
    uint32_t g_lastOkMs = 0;
    // Two missed syncs. Long enough that one dropped request is not an alarm,
    // short enough that a real outage shows while the user is still nearby.
    const uint32_t OK_TTL_MS = 12UL * 60 * 1000;

    // ---- HTTPS ----------------------------------------------------------------
    int httpsPOST(const String& url, const String& body, String& resp, const char* bearer = nullptr) {
        if (WiFi.status() != WL_CONNECTED) return -1;
        WiFiClientSecure c; tls::secure(c);
        HTTPClient h;
        if (!h.begin(c, url)) return -2;
        h.setTimeout(10000);
        h.addHeader("Content-Type", "application/json");
        if (bearer) h.addHeader("Authorization", String("Bearer ") + bearer);
        int code = h.POST(body);
        resp = (code > 0) ? h.getString() : String();
        h.end();
        return code;
    }
    int httpsGET(const String& url, String& resp, const char* bearer) {
        if (WiFi.status() != WL_CONNECTED) return -1;
        WiFiClientSecure c; tls::secure(c);
        HTTPClient h;
        if (!h.begin(c, url)) return -2;
        h.setTimeout(10000);
        h.addHeader("Authorization", String("Bearer ") + bearer);
        int code = h.GET();
        resp = (code > 0) ? h.getString() : String();
        h.end();
        return code;
    }

    // valor Firestore -> string (string / integer / boolean)
    String fsStr(JsonObjectConst f, const char* k) {
        JsonVariantConst v = f[k];
        if (v["stringValue"].is<const char*>())  return String((const char*)v["stringValue"]);
        if (v["integerValue"].is<const char*>()) return String((const char*)v["integerValue"]);
        if (v["doubleValue"].is<float>())        return String((double)v["doubleValue"], 0);
        if (v["booleanValue"].is<bool>())        return v["booleanValue"].as<bool>() ? "true" : "false";
        return "";
    }
    // First non-empty field from a list of possible names
    String fsAny(JsonObjectConst f, std::initializer_list<const char*> keys) {
        for (auto k : keys) { String s = fsStr(f, k); if (s.length()) return s; }
        return "";
    }
    // A Firestore sub-object (mapValue) -> its "fields"
    JsonObjectConst fsMap(JsonObjectConst f, const char* k) {
        return f[k]["mapValue"]["fields"].as<JsonObjectConst>();
    }
    // A Firestore boolean field: 1 = true, 0 = false, -1 = absent
    int fsBool(JsonObjectConst f, const char* k) {
        JsonVariantConst v = f[k];
        if (v["booleanValue"].is<bool>()) return v["booleanValue"].as<bool>() ? 1 : 0;
        String s = fsStr(f, k); s.toLowerCase();
        if (s == "true" || s == "1")  return 1;
        if (s == "false" || s == "0") return 0;
        return -1;
    }
    // Is this printer cloud-only? Those open no local ports, so there is nothing
    // on the network for this device to reach.
    // The shape TigerTag Studio actually writes: discovery.method = "lan-scan"
    // when the printer was found on the LAN; discovery.transport is one of
    // "ws-9999" / "http-8898" / "mqtt-8883". No discovery block but a
    // top-level ip means it was added by hand, which we treat as LAN.
    bool looksCloud(JsonObjectConst f, const String& ip) {
        JsonObjectConst disc = fsMap(f, "discovery");
        String probe = fsAny(f, { "mode", "connectionType", "connection", "network",
                                  "netMode", "link", "transport", "printerConnectionType" });
        if (!disc.isNull()) probe += " " + fsStr(disc, "method") + " " + fsStr(disc, "transport");
        probe.toLowerCase();
        if (probe.indexOf("cloud") >= 0 || probe.indexOf("remote") >= 0) return true;
        if (probe.indexOf("lan") >= 0 || probe.indexOf("local") >= 0) return false;
        if (fsBool(f, "cloud") == 1 || fsBool(f, "isCloud") == 1) return true;
        if (fsBool(f, "local") == 0 || fsBool(f, "isLocal") == 0 ||
            fsBool(f, "lan")   == 0 || fsBool(f, "isLan")   == 0) return true;
        if (ip.isEmpty()) return true;               // no LAN IP and no discovery mark -> cloud
        return false;
    }

    void saveSession() {
        pr.begin("tsaccount", false);
        pr.putString("email", g_email);
        pr.putString("name", g_name);
        pr.putString("refresh", g_refresh);
        pr.putString("uid", g_uid);
        pr.end();
    }

    bool refreshIdToken() {
        if (g_refresh.isEmpty()) return false;
        JsonDocument d;
        d["grant_type"] = "refresh_token";
        d["refresh_token"] = g_refresh;
        String body; serializeJson(d, body);
        String resp;
        int code = httpsPOST(String("https://securetoken.googleapis.com/v1/token?key=") + API_KEY, body, resp);
        if (code != 200) {
            Serial.printf("[account] refresh http=%d %s\n", code, resp.c_str());
            if (resp.indexOf("TOKEN_EXPIRED") >= 0 || resp.indexOf("USER_DISABLED") >= 0 ||
                resp.indexOf("INVALID_REFRESH_TOKEN") >= 0)
                ttcloud::forget();
            return false;
        }
        JsonDocument r;
        if (deserializeJson(r, resp)) return false;
        g_idToken = r["id_token"] | "";
        if (g_idToken.length()) g_lastOkMs = millis();
        String nr = r["refresh_token"] | "";
        if (nr.length() && nr != g_refresh) { g_refresh = nr; saveSession(); }
        String nu = r["user_id"] | "";
        if (nu.length()) g_uid = nu;
        g_tokenAt = millis();
        return g_idToken.length() > 0;
    }
    // Fill in the display name for a session that predates it being stored, or
    // one opened by QR pairing - signInWithCustomToken's answer has no
    // displayName field. Without this, every device already in the field would
    // keep showing an address until its owner happened to sign out and in.
    //
    // Cheap and idempotent: one POST, only when the name is missing, and a
    // failure is not an error - displayName() falls back to the address.
    void fetchProfileName() {
        if (g_name.length() || g_idToken.isEmpty()) return;
        JsonDocument d; d["idToken"] = g_idToken;
        String body; serializeJson(d, body);
        String resp;
        int code = httpsPOST(String("https://identitytoolkit.googleapis.com/v1/accounts:lookup?key=") + API_KEY,
                             body, resp);
        if (code != 200) return;
        JsonDocument r;
        if (deserializeJson(r, resp)) return;
        String n = String(r["users"][0]["displayName"] | "");
        if (n.isEmpty()) return;
        g_name = n;
        saveSession();
        Serial.printf("[account] display name '%s'\n", g_name.c_str());
    }

    bool ensureToken() {
        if (g_idToken.length() && millis() - g_tokenAt < 50UL * 60 * 1000) return true;
        return refreshIdToken();
    }

    // brand + printerModelId (TigerTag catalogue) -> supported backend
    PrinterType mapType(const String& brand, const String& modelId) {
        int m = modelId.toInt();
        // Creality: everything except the K1 / Ender family (ids 6..10). The
        // K2/Plus/Pro/SE (2..5), Hi (1), SparkX i7 (11) and later models all
        // speak the K2's Nebula WebSocket API.
        if (brand == "creality")   return (m >= 6 && m <= 10) ? PT_NONE : PT_CREALITY;
        // FlashForge: Creator 5 / 5 Pro are model ids 5 and 6. An AD5X reports 1 and
        // speaks the same msConfig_cmd - verified on hardware.
        if (brand == "flashforge") return (m == 5 || m == 6) ? PT_FF_C5 : PT_NONE;
        if (brand == "bambulab")   return PT_BAMBU;                                // protocolo LAN comum
        if (brand == "snapmaker")  return PT_SNAPMAKER;                             // Moonraker ws :7125
        return PT_NONE;
    }
}

void ttcloud::begin() {
    g_bootAt = millis();
    pr.begin("tsaccount", true);
    g_email   = pr.getString("email", "");
    g_name    = pr.getString("name", "");
    g_refresh = pr.getString("refresh", "");
    g_uid     = pr.getString("uid", "");
    pr.end();
}

bool   ttcloud::haveSession() { return g_refresh.length() > 0; }
String ttcloud::email()       { return g_email; }
String ttcloud::displayName() { return g_name.length() ? g_name : g_email; }
String ttcloud::lastResult()  { return g_lastResult; }

void ttcloud::forget() {
    g_email = ""; g_refresh = ""; g_uid = ""; g_idToken = ""; g_name = "";
    pr.begin("tsaccount", false); pr.clear(); pr.end();
}

bool ttcloud::signIn(const String& mail, const String& pass, String& err) {
    JsonDocument d;
    d["email"] = mail; d["password"] = pass; d["returnSecureToken"] = true;
    String body; serializeJson(d, body);
    String resp;
    int code = httpsPOST(String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=") + API_KEY,
                         body, resp);
    JsonDocument r; deserializeJson(r, resp);
    if (code != 200) {
        err = r["error"]["message"] | "sign-in failed";
        Serial.printf("[account] signIn http=%d %s\n", code, err.c_str());
        return false;
    }
    g_email   = mail;
    // Present on this endpoint when the account has one set. The pairing path
    // below has no equivalent field, which is why displayName() falls back
    // rather than assuming this is always filled.
    g_name    = String(r["displayName"] | "");
    g_idToken = r["idToken"] | "";
    g_refresh = r["refreshToken"] | "";
    g_uid     = r["localId"] | "";
    g_tokenAt = millis();
    if (g_uid.isEmpty() || g_refresh.isEmpty()) { err = "invalid answer"; return false; }
    saveSession();
    Serial.printf("[account] login OK uid=%s\n", g_uid.c_str());
    return true;
}

// --- Google account sign-in (pairing flow) --------------------------------

// signInWithCustomToken does not return localId, so the uid has to come out
// "user_id" out of the idToken payload (the base64url middle of the JWT).
static String uidFromIdToken(const String& jwt) {
    int a = jwt.indexOf('.');        if (a < 0) return "";
    int b = jwt.indexOf('.', a + 1); if (b < 0) return "";
    String p = jwt.substring(a + 1, b);
    p.replace('-', '+'); p.replace('_', '/');
    while (p.length() % 4) p += '=';
    if (p.length() > 2000) return "";
    unsigned char out[1536]; size_t got = 0;
    if (mbedtls_base64_decode(out, sizeof(out) - 1, &got,
                              (const unsigned char*)p.c_str(), p.length()) != 0) return "";
    out[got] = 0;
    JsonDocument filter; filter["user_id"] = true; filter["sub"] = true;
    JsonDocument doc;
    if (deserializeJson(doc, (const char*)out, DeserializationOption::Filter(filter))) return "";
    String u = doc["user_id"] | "";
    if (u.isEmpty()) u = String(doc["sub"] | "");
    return u;
}

bool ttcloud::pairStart(String& code, String& verifyUrl, String& pollToken,
                        int& intervalS, String& err) {
    JsonDocument d;
    d["device"] = String("tigertag-") + String((uint32_t)ESP.getEfuseMac(), HEX);
    d["model"]  = "TigerTag Bridge";
    d["kind"]   = "bridge";
    d["fw"]     = "cfs_ui";
    String body; serializeJson(d, body);
    String resp;
    int hc = httpsPOST(PAIR_START, body, resp);
    if (hc != 200) { err = String("pairStart http ") + hc; Serial.printf("[account] %s: %.200s\n", err.c_str(), resp.c_str()); return false; }
    JsonDocument r;
    if (deserializeJson(r, resp)) { err = "pairStart json"; return false; }
    code      = String(r["code"]       | "");
    verifyUrl = String(r["verify_url"] | "");
    pollToken = String(r["poll_token"] | "");
    intervalS = r["interval"] | 5;
    if (verifyUrl.isEmpty() || pollToken.isEmpty()) { err = "pairStart vazio"; return false; }
    Serial.printf("[account] pairStart ok code=%s\n", code.c_str());
    return true;
}

int ttcloud::pairPoll(const String& pollToken, String& customToken,
                      String& emailOut, String& err) {
    JsonDocument d; d["poll_token"] = pollToken;
    String body; serializeJson(d, body);
    String resp;
    int hc = httpsPOST(PAIR_POLL, body, resp);
    if (hc != 200) { err = String("pairPoll http ") + hc; return -1; }
    JsonDocument r;
    if (deserializeJson(r, resp)) { err = "pairPoll json"; return -1; }
    String st = r["status"] | "";
    if (st == "approved") {
        customToken = String(r["custom_token"] | "");
        emailOut    = String(r["email"] | "");
        return customToken.length() ? 1 : -1;
    }
    if (st == "denied")  return 2;
    if (st == "expired") return 3;
    return 0;   // pending
}

bool ttcloud::signInWithCustomToken(const String& customToken, const String& emailHint,
                                    String& err) {
    JsonDocument d;
    d["token"] = customToken; d["returnSecureToken"] = true;
    String body; serializeJson(d, body);
    String resp;
    int hc = httpsPOST(String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithCustomToken?key=") + API_KEY,
                       body, resp);
    JsonDocument r; deserializeJson(r, resp);
    if (hc != 200) {
        err = r["error"]["message"] | "custom token exchange failed";
        Serial.printf("[account] customToken http=%d %s\n", hc, err.c_str());
        return false;
    }
    g_idToken = String(r["idToken"]      | "");
    g_refresh = String(r["refreshToken"] | "");
    g_uid     = String(r["localId"]      | "");        // costuma vir vazio
    if (g_uid.isEmpty()) g_uid = uidFromIdToken(g_idToken);
    g_email   = emailHint;
    g_tokenAt = millis();
    if (g_uid.isEmpty() || g_refresh.isEmpty()) { err = "invalid answer"; return false; }
    saveSession();
    Serial.printf("[account] login Google OK uid=%s\n", g_uid.c_str());
    return true;
}

static bool g_syncedOk = false;

bool ttcloud::everSynced() { return g_lastSync != 0; }

int ttcloud::health() {
    if (!haveSession())    return 0;              // nothing to be connected to
    if (g_lastOkMs == 0)   return 1;              // linked, still working on it
    if (!g_syncedOk && g_lastSync) return 2;      // it answered once, then failed
    if (millis() - g_lastOkMs > OK_TTL_MS) return 2;
    return 3;
}

bool ttcloud::due() {
    if (!haveSession() || WiFi.status() != WL_CONNECTED) return false;
    if (g_lastSync == 0) return (millis() - g_bootAt > 8000);         // first sync about 8 s after boot
    if (!g_syncedOk)     return (millis() - g_lastSync > 60000);      // last one failed -> retry in a minute
    return (millis() - g_lastSync > SYNC_INTERVAL_MS);
}

bool ttcloud::syncNow(String& summary) {
    uint32_t tSync = millis();
    g_lastSync = millis();
    g_syncedOk = false;
    if (!ensureToken()) { summary = g_lastResult = "TigerTag: sessao invalida"; return false; }
    fetchProfileName();

    // All six brands are read. Two of them have no backend yet, and they are
    // fetched anyway so the log can say why they do not appear rather than
    // leaving the user to guess. See docs/PRINTER-COMPATIBILITY.md.
    const char* BRANDS[] = { "creality", "flashforge", "bambulab",
                             "snapmaker", "elegoo", "anycubic" };
    String base = String("https://firestore.googleapis.com/v1/projects/") + PROJECT +
                  "/databases/(default)/documents";
    // Fields asked of the server. 'discovery' is taken whole (it is small)
    // but NOT 'discovery.raw'; 'units' is never requested.
    static const char* MASK_FIELDS[] = {
        "printerName", "name", "ip", "broker", "ipAddress", "lanIp", "host",
        "mode", "connectionType", "connection", "network", "netMode",
        "link", "transport", "printerConnectionType",
        "cloud", "isCloud", "local", "isLocal", "lan", "isLan",
        "printerModelId", "modelId", "model",
        "serial", "serialNumber", "sn", "deviceId",
        "password", "dev_access_code", "accessCode", "access_code", "checkCode",
        "mqttPassword", "username", "acuModelId",
        "discovery.method", "discovery.transport", "discovery.ip",
        "discovery.deviceSn", "discovery.hostName", "discovery.acuModelId"
    };
    String MASK;
    for (auto f : MASK_FIELDS) { MASK += MASK.isEmpty() ? "?" : "&"; MASK += "mask.fieldPaths="; MASK += f; }
    PrinterCfg got[MAX_PRINTERS];
    int n = 0, ignored = 0, noip = 0, okBrands = 0, cloudN = 0;

    Serial.printf("[account] uid=%s  heap=%u\n", g_uid.c_str(), (unsigned)ESP.getFreeHeap());
    for (auto brand : BRANDS) {
        String resp;
        // A SERVER-side mask: Firestore sends only these fields. Without it the
        // answer carries discovery.raw (a full Moonraker system dump) and units
        // -> 47 KB for creality alone, downloaded and then thrown away at parse.
        String url = base + "/users/" + g_uid + "/printers/" + brand + "/devices" + MASK;
        uint32_t tGet = millis();
        int code = httpsGET(url, resp, g_idToken.c_str());
        Serial.printf("[account] GET %s/devices -> http=%d, %d bytes, %lu ms\n",
                      brand, code, resp.length(), (unsigned long)(millis() - tGet));
        if (code != 200) { Serial.printf("[account]   resp: %.300s\n", resp.c_str()); continue; }
        okBrands++;

        // Parse filter: only the fields that matter. A Firestore response is very
        // deep because of the mapValue wrappers; without a filter it hit "TooDeep")
        JsonDocument filter;
        JsonObject fd = filter["documents"].add<JsonObject>();
        fd["name"] = true;
        JsonObject ff = fd["fields"].to<JsonObject>();
        for (const char* k : { "printerName", "name", "ip", "broker", "ipAddress", "lanIp", "host",
                               "mode", "connectionType", "connection", "network", "netMode",
                               "link", "transport", "printerConnectionType",
                               "cloud", "isCloud", "local", "isLocal", "lan", "isLan",
                               "printerModelId", "modelId", "model",
                               "serial", "serialNumber", "sn", "deviceId",
                               "password", "dev_access_code", "accessCode", "access_code", "checkCode" })
            ff[k] = true;
        // discovery sub-object: only the useful fields, never discovery.raw - that
        // one carries a full system dump the firmware will never read
        JsonObject dff = ff["discovery"]["mapValue"]["fields"].to<JsonObject>();
        for (const char* k : { "method", "transport", "ip", "deviceSn", "hostName" })
            dff[k] = true;

        JsonDocument d;
        DeserializationError e = deserializeJson(d, resp,
                                    DeserializationOption::Filter(filter),
                                    DeserializationOption::NestingLimit(40));
        if (e) { Serial.printf("[account]   json err: %s\n", e.c_str()); continue; }
        JsonArrayConst docs = d["documents"].as<JsonArrayConst>();
        Serial.printf("[account]   %d device doc(s)\n", (int)docs.size());
        for (JsonObjectConst doc : docs) {
            JsonObjectConst f = doc["fields"];
            String nm  = doc["name"] | "";
            String dev = nm.substring(nm.lastIndexOf('/') + 1);
            JsonObjectConst disc = fsMap(f, "discovery");
            String ip  = fsAny(f, { "ip", "broker", "ipAddress", "lanIp", "host" });
            if (ip.isEmpty() && !disc.isNull()) ip = fsStr(disc, "ip");
            String transport = disc.isNull() ? String() : fsStr(disc, "transport");
            String mid = fsAny(f, { "printerModelId", "modelId", "model" });
            bool   cloud = looksCloud(f, ip);
            PrinterType t = mapType(brand, mid);
            // discovery.transport is a more reliable signal than the model:
            // ws-9999 = API K2 | http-8898 = FlashForge C5 | mqtt-8883 = Bambu
            if (t == PT_NONE) {
                if (transport.startsWith("ws-9999"))   t = PT_CREALITY;
                else if (transport.startsWith("http-8898")) t = PT_FF_C5;
                else if (transport.startsWith("mqtt-8883")) t = PT_BAMBU;
            }
            Serial.printf("[account]   dev=%s ip='%s' transport='%s' cloud=%d modelId='%s' -> type %d\n",
                          dev.c_str(), ip.c_str(), transport.c_str(), cloud, mid.c_str(), t);
            if (cloud) { Serial.println("[account]     ignorado: modo cloud"); cloudN++; continue; }
            if (t == PT_NONE) {
                Serial.printf("[account]     skipped: %s has no backend / unsupported model\n", brand);
                ignored++; continue;
            }
            if (n >= MAX_PRINTERS) { Serial.println("[account]     ignorado: limite MAX_PRINTERS"); ignored++; continue; }
            if (ip.isEmpty()) { noip++; Serial.println("[account]     no IP - imported anyway (fill it in on the form)"); }

            PrinterCfg& p = got[n];
            p.type = t;
            p.name = fsStr(f, "printerName");
            if (p.name.isEmpty()) p.name = String(brand) + " " + dev.substring(0, 6);
            p.host = ip;
            {
                String serialField = fsAny(f, { "serial", "serialNumber", "sn" });
                if (serialField.isEmpty() && !disc.isNull()) serialField = fsStr(disc, "deviceSn");
                if (t == PT_CREALITY) {
                    // The Creality backend does not use the serial, but the LAN
                    // sweep does: it is how a moved printer is matched back to
                    // its own entry instead of a sibling's.
                    p.sn = serialField;
                } else {
                    String deviceIdField = fsStr(f, "deviceId");
                    p.sn = serialField.length() ? serialField
                         : (deviceIdField.length() ? deviceIdField : dev);
                    // access code (FF checkCode / Bambu LAN) no campo "password"
                    p.cc = fsAny(f, { "password", "dev_access_code", "accessCode", "access_code", "checkCode",
                                  "mqttPassword" });
                }
            }
            // The same printer can appear twice in an account - two FlashForge
            // documents for one IP and serial, for instance. Do not import both.
            bool dup = false;
            for (int j = 0; j < n; j++)
                if (got[j].type == p.type && got[j].host == p.host &&
                    got[j].sn == p.sn && p.host.length()) dup = true;
            if (dup) { Serial.println("[account]     ignorado: duplicada (mesmo IP/serial)"); ignored++; continue; }

            String ccShow = p.cc.length() > 20 ? (String("[") + p.cc.length() + "b]") : p.cc;
            Serial.printf("[account] + %s '%s' @ %s  sn='%s' cc='%s' (type %d)\n",
                          brand, p.name.c_str(), p.host.c_str(), p.sn.c_str(), ccShow.c_str(), t);
            n++;
        }
    }

    // Write to NVS only if something actually changed: flash has a finite
    // number of erase cycles and this runs every few minutes.
    Preferences k; k.begin("tigerspool", false);
    bool diff = false;
    for (int i = 0; i < MAX_PRINTERS; i++) {
        char key[6];
        int    ct; String cn, ch, cs, cc;
        snprintf(key, sizeof(key), "p%dt", i); ct = k.getInt(key, 0);
        snprintf(key, sizeof(key), "p%dn", i); cn = k.getString(key, "");
        snprintf(key, sizeof(key), "p%dh", i); ch = k.getString(key, "");
        snprintf(key, sizeof(key), "p%ds", i); cs = k.getString(key, "");
        snprintf(key, sizeof(key), "p%dc", i); cc = k.getString(key, "");
        int    nt = (i < n) ? (int)got[i].type : 0;
        String nn = (i < n) ? got[i].name : String();
        String nh = (i < n) ? got[i].host : String();
        String ns = (i < n) ? got[i].sn   : String();
        String nc = (i < n) ? got[i].cc   : String();
        // The import fills gaps, it does not overwrite. A value the user typed by
        // hand survives a sync that does not know it - which also means a stale
        // one is not corrected automatically. Clearing the field is how you
        // force a refresh, and the web form says so.
        if (i < n && nt == ct) {
            if (nn.isEmpty()) nn = cn;
            if (ns.isEmpty()) ns = cs;
            // IP and check/access code: the LOCAL value wins. It may have been
            // corrected by LAN discovery or by hand in the portal. Firebase
            // only fills in when the local field is empty
            if (ch.length()) nh = ch;
            if (cc.length()) nc = cc; else if (nc.isEmpty()) nc = cc;
        }
        if (nt != ct || nn != cn || nh != ch || ns != cs || nc != cc) {
            diff = true;
            snprintf(key, sizeof(key), "p%dt", i); k.putInt(key, nt);
            snprintf(key, sizeof(key), "p%dn", i); k.putString(key, nn);
            snprintf(key, sizeof(key), "p%dh", i); k.putString(key, nh);
            snprintf(key, sizeof(key), "p%ds", i); k.putString(key, ns);
            snprintf(key, sizeof(key), "p%dc", i); k.putString(key, nc);
        }
    }
    k.end();

    g_syncedOk = (okBrands > 0);
    if (g_syncedOk) g_lastOkMs = millis();   // green is a claim about this
    if (diff) g_changed = true;
    Serial.printf("[account] sync total %lu ms\n", (unsigned long)(millis() - tSync));
    if (!g_syncedOk) { summary = g_lastResult = "TigerTag: no answer (TLS/network)"; return false; }
    summary = String("TigerTag: ") + n + " LAN" +
              (cloudN  ? (String(", ") + cloudN + " cloud")   : "") +
              (noip    ? (String(", ") + noip + " without IP")    : "") +
              (ignored ? (String(", ") + ignored + " ignored") : "") +
              (diff ? " - updated" : " - no change");
    g_lastResult = summary;
    Serial.printf("[account] sync: %s\n", summary.c_str());
    return true;
}

bool ttcloud::consumeChanged() { bool v = g_changed; g_changed = false; return v; }

// ---------------------------------------------------------------------------
//  Asynchronous sync: the home screen must NEVER wait on the network.
//  The task does nothing but network I/O and NVS writes; it is the UI loop
//  that reloads printers[] through loadCfg() once asyncDone() turns true.
// ---------------------------------------------------------------------------
static volatile bool g_asyncBusy = false;
static volatile bool g_asyncDone = false;
static String        g_asyncSummary;

static void syncTaskFn(void*) {
    String s;
    ttcloud::syncNow(s);
    g_asyncSummary = s;
    g_asyncBusy = false;
    g_asyncDone = true;
    vTaskDelete(nullptr);
}

bool ttcloud::startAsyncSync() {
    if (g_asyncBusy) return false;
    g_asyncBusy = true; g_asyncDone = false;
    // 16 KB: mbedTLS needs room, and the JSON parsing runs on this stack too.
    if (xTaskCreatePinnedToCore(syncTaskFn, "ttSync", 16384, nullptr, 1, nullptr, 1) != pdPASS) {
        g_asyncBusy = false;
        Serial.println("[account] xTaskCreate failed - sync skipped");
        return false;
    }
    return true;
}
bool ttcloud::asyncBusy() { return g_asyncBusy; }
// ---------------------------------------------------------------------------
//  pairStart, off the UI thread.
// ---------------------------------------------------------------------------
static volatile bool s_pairBusy = false;
static volatile int  s_pairResult = 0;      // 0 running, 1 ok, -1 failed
static String s_pCode, s_pUrl, s_pToken, s_pErr;
static int    s_pInterval = 5;

static void pairTaskFn(void*) {
    String code, url, token, err;
    int iv = 5;
    bool ok = ttcloud::pairStart(code, url, token, iv, err);
    s_pCode = code; s_pUrl = url; s_pToken = token; s_pInterval = iv; s_pErr = err;
    s_pairResult = ok ? 1 : -1;
    s_pairBusy = false;
    vTaskDelete(nullptr);
}

bool ttcloud::startPairAsync() {
    if (s_pairBusy) return false;
    s_pairBusy = true; s_pairResult = 0;
    if (xTaskCreatePinnedToCore(pairTaskFn, "ttPair", 12288, nullptr, 1, nullptr, 1) != pdPASS) {
        s_pairBusy = false; s_pairResult = -1;
        s_pErr = "task";
        return false;
    }
    return true;
}
bool ttcloud::pairAsyncBusy() { return s_pairBusy; }
int  ttcloud::pairAsyncTake(String& code, String& verifyUrl, String& pollToken,
                            int& intervalS, String& err) {
    if (s_pairBusy || s_pairResult == 0) return 0;
    int r = s_pairResult; s_pairResult = 0;
    code = s_pCode; verifyUrl = s_pUrl; pollToken = s_pToken;
    intervalS = s_pInterval; err = s_pErr;
    return r;
}

bool ttcloud::asyncTake(String& summary) {
    if (!g_asyncDone) return false;
    g_asyncDone = false;
    summary = g_asyncSummary;
    return true;
}
