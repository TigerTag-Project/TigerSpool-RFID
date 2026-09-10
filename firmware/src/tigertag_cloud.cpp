#include "tigertag_cloud.h"

#include <memory>
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
        if (code <= 0) {
            // A TLS session needs one large contiguous block, so the number
            // that matters when this fails is the LARGEST FREE BLOCK, not the
            // total. Printing both is what tells a fragmented heap apart from
            // an exhausted one - and both apart from a network fault.
            char err[96] = { 0 };
            c.lastError(err, sizeof(err));
            Serial.printf("[account]   https fail %d  heap=%u maxblk=%u  err='%s'\n",
                          code, (unsigned)ESP.getFreeHeap(),
                          (unsigned)ESP.getMaxAllocHeap(), err);
        }
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
    // A time field as milliseconds since 1970, whichever way it was stored.
    //
    // Most documents carry updatedAt as an integer of milliseconds, and some as
    // a Firestore timestamp - "2026-09-10T21:20:06.421Z". The AD5X document
    // Tiger Studio rewrote on the bench read as no date at all, which made it
    // the OLDEST of two documents for one printer: the stale one at the old
    // address would have won. 0 when there is nothing readable.
    int64_t fsMillis(JsonObjectConst f, const char* k) {
        JsonVariantConst v = f[k];
        if (v["integerValue"].is<const char*>())
            return strtoll((const char*)v["integerValue"], nullptr, 10);
        if (v["doubleValue"].is<double>()) return (int64_t)v["doubleValue"].as<double>();
        const char* ts = v["timestampValue"] | "";
        int Y, M, D, h, m, sec;
        if (sscanf(ts, "%4d-%2d-%2dT%2d:%2d:%2d", &Y, &M, &D, &h, &m, &sec) != 6) return 0;
        int ms = 0;
        if (const char* dot = strchr(ts, '.')) {
            int digits = 0;
            for (const char* c = dot + 1; *c >= '0' && *c <= '9' && digits < 3; c++, digits++)
                ms = ms * 10 + (*c - '0');
            while (digits++ < 3) ms *= 10;
        }
        // Days from 1970-01-01 to Y-M-D, proleptic Gregorian (Howard Hinnant's
        // days_from_civil). Firestore always writes UTC, with a Z.
        Y -= M <= 2;
        const int era = (Y >= 0 ? Y : Y - 399) / 400;
        const unsigned yoe = (unsigned)(Y - era * 400);
        const unsigned doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        const int64_t days = (int64_t)era * 146097 + (int64_t)doe - 719468;
        return ((days * 24 + h) * 60 + m) * 60000LL + sec * 1000LL + ms;
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
        // Elegoo speaks one protocol across the range - the Centauri Carbon
        // family and everything after it - so there is no model to check.
        if (brand == "elegoo")     return PT_ELEGOO;                                // MQTT :1883
        // Anycubic in LAN mode. A cloud-mode printer is filtered out before
        // this by looksCloud(): it has no open local port to connect to, and
        // reaching it needs Anycubic's own service rather than this backend.
        if (brand == "anycubic")   return PT_ANYCUBIC;                              // MQTT/TLS :9883
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

// Bambu Lab's cloud credentials, cached from NVS on first use. Not a printer
// and not per printer: one session per account, written by Tiger Studio.
static String g_bblUser, g_bblToken, g_bblRegion;

// True from the first line of syncNow to its last, whichever way it leaves.
//
// Without it the account icon turned orange every five minutes for as long as a
// sync took, on every screen. syncNow clears g_syncedOk when it STARTS - it has
// not succeeded yet, which is true - and health() read that as "it answered once
// and then failed". The indicator was reporting work in progress as a fault. A
// sync in flight is not news; only a sync that finished badly is.
static volatile bool g_syncInFlight = false;
struct SyncFlight {
    SyncFlight()  { g_syncInFlight = true; }
    ~SyncFlight() { g_syncInFlight = false; }
};

bool ttcloud::everSynced() { return g_lastSync != 0; }

int ttcloud::health() {
    if (!haveSession())    return 0;              // nothing to be connected to
    if (g_lastOkMs == 0)   return 1;              // linked, still working on it
    // A sync that is running is judged on the last one that finished, never on
    // its own unfinished state.
    if (!g_syncInFlight && !g_syncedOk && g_lastSync) return 2;
    if (millis() - g_lastOkMs > OK_TTL_MS) return 2;
    return 3;
}

bool ttcloud::due() {
    if (!haveSession() || WiFi.status() != WL_CONNECTED) return false;
    if (g_lastSync == 0) return (millis() - g_bootAt > 8000);         // first sync about 8 s after boot
    if (!g_syncedOk)     return (millis() - g_lastSync > 60000);      // last one failed -> retry in a minute
    return (millis() - g_lastSync > SYNC_INTERVAL_MS);
}

// The serial a printer is recognised by, across documents and across syncs.
//
// Tiger Studio writes a FlashForge serial with and without the "SN" the
// printer's own screen prints in front of it: the same Creator 5 Pro arrived as
// SNMUPF9511513 in one sync and MUPF9511513 in the next, and the same AD5X is
// MQQE9501368 in one document and SNMQQE9501368 in another. The printer takes
// either (measured: its /checkCode ignores the serial altogether), so the two
// spellings are one machine. No other brand's serials start that way.
static String idSerial(const PrinterCfg& p) {
    if (p.type == PT_FF_C5 && p.sn.startsWith("SN")) return p.sn.substring(2);
    return p.sn;
}

// Two records describe the same machine.
//
// With a serial: the same serial, and either the same address or the same
// mode. The address covers an A1 whose LAN and cloud documents both carry its
// own IP; the mode covers a printer that moved - an AD5X whose old document
// still says 192.168.20.131 beside a new one that says 192.168.40.105. A LAN
// document and a cloud document at DIFFERENT addresses stay two entries: the
// X1C is imported both ways on purpose, one to write to and one to read.
//
// Without a serial - a Creality K2 whose account holds none - the address is
// all there is.
static bool samePrinter(const PrinterCfg& a, const PrinterCfg& b) {
    if (a.type != b.type) return false;
    const String sa = idSerial(a), sb = idSerial(b);
    if (sa.length() || sb.length()) {
        if (sa != sb) return false;
        return a.host == b.host || a.cloud == b.cloud;
    }
    return a.host.length() && a.host == b.host;
}

bool ttcloud::syncNow(String& summary) {
    SyncFlight inFlight;
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
    // Each brand maps to exactly one backend type, which is what makes it
    // possible to say "these stored printers came from that brand" when its
    // request fails. Kept beside the list above so the two cannot drift.
    const PrinterType BRAND_TYPE[] = { PT_CREALITY, PT_FF_C5, PT_BAMBU,
                                       PT_SNAPMAKER, PT_ELEGOO, PT_ANYCUBIC };
    String base = String("https://firestore.googleapis.com/v1/projects/") + PROJECT +
                  "/databases/(default)/documents";
    // The fields this import reads, in ONE list.
    //
    // It used to be two, forty lines apart: a server-side mask that decided
    // what Firestore sent, and a client-side ArduinoJson filter that decided
    // what the parser kept. Nothing made them agree, and they stopped agreeing
    // the moment three fields were added to the first and not the second -
    // mqttPassword, username and acuModelId. Firestore sent all three; the
    // filter dropped them before anything could read them. On the bench that
    // looked like an account with no Elegoo access code and an Anycubic with
    // no username, when the account had every one of them.
    //
    // The two do different jobs - the mask cuts bytes on the wire, the filter
    // cuts nesting depth in the parser - but they answer to the same question:
    // which fields does this import care about. One list, asked twice.
    static const char* MASK_FIELDS[] = {
        "printerName", "name", "ip", "broker", "ipAddress", "lanIp", "host",
        "updatedAt",
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
    // Each imported printer's updatedAt, kept beside the array rather than in
    // PrinterCfg: it decides which of two documents for one machine wins, and
    // it is of no use to anything once the import is over.
    int64_t gotUpd[MAX_PRINTERS] = { 0 };
    // Carried from storage because its brand did not answer: its host is the
    // device's, not the account's, and must not be recorded as the account's.
    bool gotKept[MAX_PRINTERS] = { false };
    int n = 0, ignored = 0, noip = 0, okBrands = 0, cloudN = 0, kept = 0;

    // What the device already knows, kept open for the whole import.
    //
    // WHY. A sync that reached one brand out of six used to be written as if it
    // were the whole account: five requests failed on a bench with six links
    // open, and a list of thirteen printers became a list of three - the user's
    // Bambu, FlashForge, Snapmaker, Elegoo and Anycubic machines all gone from
    // a device that had simply been unable to ask about them. The failure was
    // real and worth fixing on its own, but no failure should ever be able to
    // delete an account's printers. A brand that did not answer contributes
    // what was stored for it last time, in its own place in the list.
    Preferences kp; kp.begin("tigerspool", true);

    Serial.printf("[account] uid=%s  heap=%u\n", g_uid.c_str(), (unsigned)ESP.getFreeHeap());
    for (int bi = 0; bi < (int)(sizeof(BRANDS) / sizeof(BRANDS[0])); bi++) {
        const char* brand = BRANDS[bi];
        String resp;
        // A SERVER-side mask: Firestore sends only these fields. Without it the
        // answer carries discovery.raw (a full Moonraker system dump) and units
        // -> 47 KB for creality alone, downloaded and then thrown away at parse.
        String url = base + "/users/" + g_uid + "/printers/" + brand + "/devices" + MASK;
        uint32_t tGet = millis();
        int code = httpsGET(url, resp, g_idToken.c_str());
        Serial.printf("[account] GET %s/devices -> http=%d, %d bytes, %lu ms\n",
                      brand, code, resp.length(), (unsigned long)(millis() - tGet));
        if (code != 200) {
            Serial.printf("[account]   resp: %.300s\n", resp.c_str());
            // Not an empty brand - an unanswered question. Re-read what this
            // brand contributed to the stored list and carry it through.
            for (int i = 0; i < MAX_PRINTERS && n < MAX_PRINTERS; i++) {
                char key[6];
                snprintf(key, sizeof(key), "p%dt", i);
                if (kp.getInt(key, 0) != (int)BRAND_TYPE[bi]) continue;
                PrinterCfg& q = got[n];
                q = PrinterCfg{};
                q.type = BRAND_TYPE[bi];
                snprintf(key, sizeof(key), "p%dn", i); q.name  = kp.getString(key, "");
                snprintf(key, sizeof(key), "p%dh", i); q.host  = kp.getString(key, "");
                snprintf(key, sizeof(key), "p%ds", i); q.sn    = kp.getString(key, "");
                snprintf(key, sizeof(key), "p%dc", i); q.cc    = kp.getString(key, "");
                snprintf(key, sizeof(key), "p%dd", i); q.devId = kp.getString(key, "");
                snprintf(key, sizeof(key), "p%du", i); q.user  = kp.getString(key, "");
                snprintf(key, sizeof(key), "p%dm", i); q.model = kp.getString(key, "");
                snprintf(key, sizeof(key), "p%dk", i); q.cloud = kp.getBool(key, false);
                Serial.printf("[account]   kept '%s' (no answer for %s)\n",
                              q.name.c_str(), brand);
                gotUpd[n] = 0;
                gotKept[n] = true;
                n++; kept++;
            }
            continue;
        }
        okBrands++;

        // Parse filter: only the fields that matter. A Firestore response is very
        // deep because of the mapValue wrappers; without a filter it hit "TooDeep")
        JsonDocument filter;
        JsonObject fd = filter["documents"].add<JsonObject>();
        fd["name"] = true;
        JsonObject ff = fd["fields"].to<JsonObject>();
        // The same list the mask was built from. A field the server sends and
        // the filter does not name is discarded here, silently, and reads back
        // as an empty credential later.
        for (auto k : MASK_FIELDS)
            if (!strchr(k, '.')) ff[k] = true;      // dotted paths are the sub-object below
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
                else if (transport.startsWith("mqtt-1883")) t = PT_ELEGOO;
                else if (transport.startsWith("mqtts-9883")) t = PT_ANYCUBIC;
            }
            const int64_t updMs = fsMillis(f, "updatedAt");
            const String upd = updMs ? String((long long)updMs) : String();
            Serial.printf("[account]   dev=%s ip='%s' transport='%s' cloud=%d modelId='%s' upd=%s -> type %d\n",
                          dev.c_str(), ip.c_str(), transport.c_str(), cloud, mid.c_str(),
                          upd.length() ? upd.c_str() : "-", t);
            // Cloud printers used to be dropped here. They are imported now -
            // a printer the user owns should appear on the device even when
            // the device cannot write to it, and being told why is better than
            // wondering where it went. Only Bambu Lab, though: it is the one
            // maker whose cloud this firmware can read.
            if (cloud) {
                cloudN++;
                if (t != PT_BAMBU) {
                    Serial.println("[account]     ignored: cloud mode, no reader for this brand");
                    ignored++; continue;
                }
                Serial.println("[account]     cloud mode: imported read-only");
            }
            if (t == PT_NONE) {
                Serial.printf("[account]     skipped: %s has no backend / unsupported model\n", brand);
                ignored++; continue;
            }
            if (n >= MAX_PRINTERS) { Serial.println("[account]     ignored: MAX_PRINTERS reached"); ignored++; continue; }
            if (ip.isEmpty()) { noip++; Serial.println("[account]     no IP - imported anyway (fill it in on the form)"); }

            PrinterCfg& p = got[n];
            p.type = t;
            p.cloud = cloud;
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
                if (t == PT_ANYCUBIC) {
                    // Logged in full because all three come from the account and
                    // none can be read off the printer: when one is missing, the
                    // only place the answer exists is this line.
                    // acuModelId, not modelId: the first is Anycubic's own
                    // numeric model and half of every topic, the second is the
                    // TigerTag catalogue id that chose this backend. Confusing
                    // them connects to a broker that answers nothing.
                    p.devId = fsStr(f, "deviceId");
                    p.user  = fsStr(f, "username");
                    p.model = fsAny(f, { "acuModelId" });
                    Serial.printf("[account]     anycubic devId='%s' user='%s' acuModel='%s'\n",
                                  p.devId.c_str(), p.user.c_str(), p.model.c_str());
                }
            }
            // The same printer can appear twice in an account - two FlashForge
            // documents for one serial, an old one left at the address the
            // printer had before it moved, or an old LAN document beside the
            // cloud one Tiger Studio writes when the printer is switched over.
            // samePrinter() says which pairs are one machine.
            //
            // Keeping the FIRST of the two kept the stale one. An A1 switched
            // to cloud mode stayed "LAN" on this device, with the access code
            // the printer had rotated on the way - so it answered rc=5 for
            // ever, and its slots could not be read at all. updatedAt is the
            // one field that says which document describes the printer as it
            // is now, so the newer one replaces the older, in place: the list
            // keeps its order and nothing else moves. A document without one
            // counts as the oldest.
            //
            // Two AD5X documents did exactly that on the bench, one at the old
            // address and one at the new: both were imported, the stale one
            // first, and it was the one on screen - dialling an address the
            // printer had left.
            const int64_t mine = updMs;
            int dup = -1;
            for (int j = 0; j < n; j++)
                if (samePrinter(got[j], p)) { dup = j; break; }
            if (dup >= 0) {
                if (mine > gotUpd[dup]) {
                    Serial.printf("[account]     newer document for this printer - replaces '%s'\n",
                                  got[dup].name.c_str());
                    got[dup] = p;
                    gotUpd[dup] = mine;
                    gotKept[dup] = false;
                } else {
                    Serial.println("[account]     ignored: duplicate of the same printer, older");
                }
                ignored++; continue;
            }
            gotUpd[n] = mine;

            String ccShow = p.cc.length() > 20 ? (String("[") + p.cc.length() + "b]") : p.cc;
            Serial.printf("[account] + %s '%s' @ %s  sn='%s' cc='%s' (type %d)\n",
                          brand, p.name.c_str(), p.host.c_str(), p.sn.c_str(), ccShow.c_str(), t);
            n++;
        }
    }

    kp.end();

    // What the device holds, read in full before a single key is written.
    //
    // A printer's stored fields are keyed by POSITION - p3h is "the host of
    // entry 3" - and the import used to merge by position too: entry i of the
    // new list against entry i of the old, with the stored host and access code
    // winning whenever the brand matched. So when the list shifted, every
    // printer after the shift took its neighbour's address and code. On the
    // bench a new AD5X document in the account moved every Bambu down by one,
    // and X1C Home (Lan) dialled the A1's address with the A1's code for as
    // long as the device ran - rc=5, for ever, and nothing said why. The switch
    // a user had turned on stayed on the position too, so it moved to another
    // machine.
    //
    // Now each imported printer is matched to the stored one that IS it -
    // samePrinter(), wherever it sat - and what the device holds of its own
    // (the switch, a host found by LAN discovery) moves with it. The keys stay
    // positional, so nothing already stored needs migrating.
    struct Stored {
        int t; String n, h, s, c, d, u, m, a; bool k, v;
        PrinterCfg cfg() const {
            PrinterCfg p; p.type = (PrinterType)t; p.name = n; p.host = h; p.sn = s;
            p.cloud = k; return p;
        }
    };
    // On the heap for the length of this function: 24 of these is 3 KB, too
    // much for the sync task's stack beside got[], and nothing to keep after.
    std::unique_ptr<Stored[]> old(new Stored[MAX_PRINTERS]);
    Preferences k; k.begin("tigerspool", false);
    for (int i = 0; i < MAX_PRINTERS; i++) {
        char key[6];
        Stored& o = old[i];
        snprintf(key, sizeof(key), "p%dt", i); o.t = k.getInt(key, 0);
        snprintf(key, sizeof(key), "p%dn", i); o.n = k.getString(key, "");
        snprintf(key, sizeof(key), "p%dh", i); o.h = k.getString(key, "");
        snprintf(key, sizeof(key), "p%ds", i); o.s = k.getString(key, "");
        snprintf(key, sizeof(key), "p%dc", i); o.c = k.getString(key, "");
        snprintf(key, sizeof(key), "p%dd", i); o.d = k.getString(key, "");
        snprintf(key, sizeof(key), "p%du", i); o.u = k.getString(key, "");
        snprintf(key, sizeof(key), "p%dm", i); o.m = k.getString(key, "");
        snprintf(key, sizeof(key), "p%da", i); o.a = k.getString(key, "");
        snprintf(key, sizeof(key), "p%dk", i); o.k = k.getBool(key, false);
        snprintf(key, sizeof(key), "p%dv", i); o.v = k.getBool(key, true);
    }
    const int oldSel = k.getInt("printerIdx", 0);

    // Which stored entry each imported printer is. -1: new to this device.
    int from[MAX_PRINTERS];
    bool taken[MAX_PRINTERS] = { false };
    for (int i = 0; i < n; i++) {
        from[i] = -1;
        for (int o = 0; o < MAX_PRINTERS; o++) {
            if (taken[o] || old[o].t == PT_NONE) continue;
            if (!samePrinter(old[o].cfg(), got[i])) continue;
            from[i] = o; taken[o] = true; break;
        }
    }

    // Write to NVS only if something actually changed: flash has a finite
    // number of erase cycles and this runs every few minutes.
    bool diff = false;
    int newSel = -1;
    for (int i = 0; i < MAX_PRINTERS; i++) {
        char key[6];
        const Stored& at = old[i];            // what is stored at this position now
        const int o = (i < n) ? from[i] : -1;
        const Stored* was = (o >= 0) ? &old[o] : nullptr;   // this printer, as stored
        if (o >= 0 && o == oldSel) newSel = i;

        int    nt = (i < n) ? (int)got[i].type : 0;
        String nn = (i < n) ? got[i].name : String();
        String nh = (i < n) ? got[i].host : String();
        String ns = (i < n) ? got[i].sn   : String();
        String nc = (i < n) ? got[i].cc   : String();
        String nd = (i < n) ? got[i].devId : String();
        String nu = (i < n) ? got[i].user  : String();
        String nm2 = (i < n) ? got[i].model : String();
        bool   nk  = (i < n) ? got[i].cloud : false;
        // The address the ACCOUNT gave, remembered so the next sync can tell
        // "the account moved this printer" from "the account still says what
        // it said, and the device has since found better".
        String na  = (i < n) ? (gotKept[i] && was ? was->a : got[i].host) : String();
        bool   nv  = was ? was->v : false;
        if (was) {
            // The account owns a printer's name, serial, access code and the
            // three Anycubic fields: nothing on the device writes them any
            // more - the form that did is gone. A field the account left empty
            // keeps what the device had rather than being blanked by it.
            if (nn.isEmpty())  nn  = was->n;
            if (ns.isEmpty())  ns  = was->s;
            if (nc.isEmpty())  nc  = was->c;
            if (nd.isEmpty())  nd  = was->d;
            if (nu.isEmpty())  nu  = was->u;
            if (nm2.isEmpty()) nm2 = was->m;
            // The host is the one field the device can also write: LAN
            // discovery corrects a K2 that moved (main.cpp, reconcile()). That
            // correction stands while the account keeps saying the same thing;
            // the moment the account says something NEW, the account wins.
            // Taking the stored host unconditionally is how a printer that
            // moved stayed at its old address on this device for good.
            if (nh.isEmpty() || (was->a.length() && nh == was->a)) {
                if (was->h.length()) nh = was->h;
            }
        }
        // A printer new to this device arrives switched off, as on the first
        // boot: it would otherwise take load slots nobody chose to spend.
        if (i < n && !was)
            Serial.printf("[account]   new on this device: '%s' - arrives switched off\n",
                          nn.c_str());

        if (nt != at.t || nn != at.n || nh != at.h || ns != at.s || nc != at.c ||
            nd != at.d || nu != at.u || nm2 != at.m || nk != at.k || na != at.a ||
            (i < n && nv != at.v)) {
            diff = true;
            if (i < n && o != i)
                Serial.printf("[account]   '%s' now at %d (was %d)\n", nn.c_str(), i, o);
            snprintf(key, sizeof(key), "p%dt", i); k.putInt(key, nt);
            snprintf(key, sizeof(key), "p%dn", i); k.putString(key, nn);
            snprintf(key, sizeof(key), "p%dh", i); k.putString(key, nh);
            snprintf(key, sizeof(key), "p%ds", i); k.putString(key, ns);
            snprintf(key, sizeof(key), "p%dc", i); k.putString(key, nc);
            snprintf(key, sizeof(key), "p%dd", i); k.putString(key, nd);
            snprintf(key, sizeof(key), "p%du", i); k.putString(key, nu);
            snprintf(key, sizeof(key), "p%dm", i); k.putString(key, nm2);
            snprintf(key, sizeof(key), "p%da", i); k.putString(key, na);
            snprintf(key, sizeof(key), "p%dk", i); k.putBool(key, nk);
            // The switch is written only when it belongs to a different
            // printer than before. Rewriting it every time would undo a switch
            // the user flipped during the fifteen seconds this import runs.
            if (i < n && nv != at.v && o != i) {
                snprintf(key, sizeof(key), "p%dv", i); k.putBool(key, nv);
            }
        }
    }
    // The selected printer follows its printer, not its position.
    if (newSel >= 0 && newSel != oldSel) {
        k.putInt("printerIdx", newSel);
        diff = true;
    }
    k.end();

    // One more document: Bambu Lab's cloud session. It is not a printer, it is
    // the credential that lets the device READ printers it cannot reach on the
    // LAN, and it lives under the brand rather than under a machine because it
    // is per account.
    {
        String resp;
        const String url = base + "/users/" + g_uid +
                           "/printers/bambulab/secrets/cloud_session";
        const int code = httpsGET(url, resp, g_idToken.c_str());
        if (code == 200) {
            JsonDocument d;
            if (!deserializeJson(d, resp)) {
                JsonObjectConst f = d["fields"];
                const String u = fsAny(f, { "mqttUsername" });
                const String t = fsAny(f, { "accessToken" });
                const String r = fsAny(f, { "region" });
                Preferences k; k.begin("tsaccount", false);
                k.putString("bblUser", u);
                k.putString("bblToken", t);
                k.putString("bblRegion", r.length() ? r : String("us"));
                k.end();
                g_bblUser = u; g_bblToken = t; g_bblRegion = r;
                Serial.printf("[account] bambu cloud session: user='%s' region='%s' token=%s\n",
                              u.c_str(), r.c_str(), t.length() ? "yes" : "MISSING");
            }
        } else if (code == 404) {
            Serial.println("[account] no bambu cloud session - sign in to Bambu in Tiger Studio");
        } else {
            Serial.printf("[account] bambu cloud session: http=%d\n", code);
        }
    }

    g_syncedOk = (okBrands > 0);
    if (g_syncedOk) g_lastOkMs = millis();   // green is a claim about this
    if (diff) g_changed = true;
    Serial.printf("[account] sync total %lu ms\n", (unsigned long)(millis() - tSync));
    if (!g_syncedOk) { summary = g_lastResult = "TigerTag: no answer (TLS/network)"; return false; }
    summary = String("TigerTag: ") + (n - kept) + " LAN" +
              (kept    ? (String(", ") + kept + " kept")      : "") +
              (cloudN  ? (String(", ") + cloudN + " cloud")   : "") +
              (noip    ? (String(", ") + noip + " without IP")    : "") +
              (ignored ? (String(", ") + ignored + " ignored") : "") +
              (diff ? " - updated" : " - no change");
    g_lastResult = summary;
    Serial.printf("[account] sync: %s\n", summary.c_str());
    return true;
}

bool ttcloud::bambuCloud(String& mqttUser, String& token, String& region) {
    if (g_bblUser.isEmpty() || g_bblToken.isEmpty()) {
        Preferences k; k.begin("tsaccount", true);
        g_bblUser   = k.getString("bblUser", "");
        g_bblToken  = k.getString("bblToken", "");
        g_bblRegion = k.getString("bblRegion", "us");
        k.end();
    }
    mqttUser = g_bblUser; token = g_bblToken;
    region = g_bblRegion.length() ? g_bblRegion : String("us");
    return mqttUser.length() && token.length();
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
    //
    // CORE 0, not core 1. A full sync is fifteen seconds of TLS and JSON, and
    // on core 1 it shares that core with the Arduino loop at the same priority
    // - so the interface got half the processor for a quarter of a minute,
    // every five minutes, and every screen felt like treacle while it ran.
    // Core 0 already carries Wi-Fi and the reachability probe; this belongs
    // with them, next to the radio it is talking through.
    if (xTaskCreatePinnedToCore(syncTaskFn, "ttSync", 16384, nullptr, 1, nullptr, 0) != pdPASS) {
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
