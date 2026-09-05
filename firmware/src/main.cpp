// ============================================================================
//  TigerSpool RFID - main
//
//  Reads a TigerTag NFC chip and writes the filament into a printer slot.
//
//  State machine:
//    LANG -> WIFI -> (PROVISION) -> PRINTERS -> GRID -> SCAN -> REVIEW -> RESULT
//
//  Everything drawn lives in src/ui/ and is built from LVGL widgets. This file
//  owns the transitions and the data the screens read; it draws nothing itself.
//
//  Printers are imported from the user's TigerTag account into NVS namespace
//  "tigerspool" - they are never typed on this screen. See
//  docs/ARCHITECTURE.md and docs/ACCOUNT-DATA.md.
// ============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LovyanGFX.hpp>
#include "LGFX_ESP32_S3_Touch_LCD_2.h"
#include "config.h"
#include "i18n.h"
#include "reader.h"
#include "printer.h"
#include "backend_creality.h"
#include "backend_ff.h"
#include "backend_bambu.h"
#include "backend_snapmaker.h"
#include "webcfg.h"
#include "net/ota.h"
#include "imu.h"
#include "tigertag_cloud.h"
#include "ui/lvgl_port.h"
#include "ui/screen_home.h"
#include "ui/screen_setup.h"
#include "ui/screen_slots.h"
#include "ui/screen_scan.h"
#include "ui/screen_settings.h"
#include "version.h"

// ---- cores -------------------------------------------------------------
#define C_BG    0x0000
#define C_HDR   0x2104
#define C_TXT   0xFFFF
#define C_DIM   0x8410
#define C_SEL   0xFFE0
#define C_OKG   0x2606
#define C_ERR   0xF800
#define C_BTNR  0x9000
#define C_BTNG  0x0480
#define C_BTNB  0x1A4B

LGFX        lcd;
LGFX_Sprite canvas(&lcd);
bool        canvasReady = false;

Preferences nvs;
String  wifiSsid, wifiPass;
PrinterCfg printers[MAX_PRINTERS];
int     selectedPrinter = 0;

PrinterBackend* backend = nullptr;
CrealityBackend            crealityBackend;
FlashForgeC5Backend  flashForgeBackend;
BambuBackend         bambuBackend;
SnapmakerBackend     snapmakerBackend;
bool webStarted = false;

enum State { ST_LANG, ST_WIFI, ST_AP, ST_ACCOUNT, ST_SETTINGS, ST_PICK, ST_SET_WIFI, ST_SET_ACCOUNT, ST_SET_SCREEN,
             ST_SET_UPDATE, ST_SET_RESTART, ST_SET_FACTORY, ST_PRINTER, ST_GRID, ST_SCAN, ST_REVIEW, ST_RESULT,
             ST_WEB_PAIR, ST_UPDATE_NOTICE };
State   state = ST_LANG;
// Whether the language screen was opened from Settings rather than reached on
// first boot. It decides two things: that the screen offers a way back, and
// where picking a language returns to. Inferring it from WiFi.isConnected()
// used to do both, and sent anyone whose network had dropped through first-boot
// setup again.
bool    langFromSettings = false;
bool    nfcReady = false;
uint32_t nfcLastTry = 0;
int     selSlot = -1;
TagInfo tag;
bool    sendOk = false;
String  resultMsg;
uint32_t stateSince = 0;

static int printerCount() {
    int n = 0;
    for (int i = 0; i < MAX_PRINTERS; i++) if (printers[i].type != PT_NONE) n++;
    return n;
}
static const char* typeTag(PrinterType t) {
    return t == PT_CREALITY ? "K2" : t == PT_FF_C5 ? "FF C5" : t == PT_BAMBU ? "Bambu"
         : t == PT_SNAPMAKER ? "Snap" : "--";
}

// ---- "is it online?" probe (TCP connect to the control port) ------------
// K2 = ws :9999 | FlashForge C5 = HTTP :8898 | Bambu = MQTT :8883
// Records when the last OK answer came in. A failed probe does NOT hide
// A single failed probe does not hide a printer: a TCP connect fails on a cold
// ARP cache or a congested link often enough that treating one miss as "gone"
// would make the list flicker.
static uint32_t pLastSeen[MAX_PRINTERS] = { 0 };
static uint32_t pProbeAt = 0;
static int      pProbeIdx = 0;
static const uint32_t ONLINE_TTL_MS = 25000;   // stays "online" for 25 s without an answer

static uint16_t ctrlPort(PrinterType t) {
    switch (t) {
        case PT_FF_C5: return 8898;
        case PT_BAMBU: return 8883;
        case PT_SNAPMAKER: return 7125;
        default:       return 9999;       // K2
    }
}
static bool probeOne(const PrinterCfg& p) {
    if (p.type == PT_NONE || p.host.isEmpty()) return false;
    WiFiClient c;
    bool ok = c.connect(p.host.c_str(), ctrlPort(p.type), 900);
    c.stop();
    return ok;
}
static bool isOnline(int i) {
    return printers[i].type != PT_NONE && pLastSeen[i] != 0
        && (millis() - pLastSeen[i] < ONLINE_TTL_MS);
}
static int onlineCount();                   // fwd
static void probeTick() {                   // one printer per call, round-robin
    // When nothing answers at all - wrong network, everything switched off -
    // slow right down. Otherwise the log fills with connect() errors every
    // second and hides anything useful.
    uint32_t gap = (onlineCount() > 0) ? 1200 : 6000;
    if (millis() - pProbeAt < gap) return;
    pProbeAt = millis();
    for (int k = 0; k < MAX_PRINTERS; k++) {
        int i = (pProbeIdx + k) % MAX_PRINTERS;
        if (printers[i].type == PT_NONE) continue;
        if (probeOne(printers[i])) pLastSeen[i] = millis() ? millis() : 1;
        pProbeIdx = (i + 1) % MAX_PRINTERS;
        return;
    }
}
static int onlineCount() {
    int n = 0;
    for (int i = 0; i < MAX_PRINTERS; i++) if (isOnline(i)) n++;
    return n;
}
// Configured printers are ALWAYS listed. Reachability is an indicator, not a
// filter: the TCP probe is not reliable enough to hide anything, and a printer
// anything: a K2 or FlashForge with LAN Mode off answers RST on that port.
static bool printerVisible(int i) {
    return printers[i].type != PT_NONE && printers[i].visible;
}

// ---- Creality discovery on the LAN (auto-corrects wrong IPs) ------------
// Creality printers do not announce themselves over mDNS, so the only way to
// find one that moved is to sweep the subnet for port 9999 and complete a
// WebSocket handshake. Done a few addresses per call so it never blocks:
// a few addresses per call. Printers are matched by their serial number, and
// where there is no serial, one-to-one - exactly one unreachable Creality
// against exactly one newly found Creality is an unambiguous pairing.
namespace disc {
    enum { IDLE, SWEEP, RECONCILE } st = IDLE;
    uint32_t lastRun = 0;
    int      cur = 1;
    IPAddress base;
    struct Found { uint8_t oct; String sn; };
    Found  found[10];
    int    nFound = 0;

    // Minimal WS handshake, then ask for printerInfo. Returns the deviceSn if
    // this really is a K2.
    bool probe(IPAddress ip, String& sn) {
        WiFiClient c;
        if (!c.connect(ip, 9999, 150)) { c.stop(); return false; }
        c.print(F("GET / HTTP/1.1\r\nHost: k\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                  "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n"));
        String buf; uint32_t t0 = millis();
        while (millis() - t0 < 250 && buf.indexOf("\r\n\r\n") < 0) {
            while (c.available()) buf += (char)c.read();
            delay(5);
        }
        if (buf.indexOf(" 101 ") < 0 && buf.indexOf("101 Switching") < 0) { c.stop(); return false; }
        // masked WS text frame carrying the request
        const char* req = "{\"method\":\"get\",\"params\":{\"printerInfo\":1}}";
        uint8_t rl = strlen(req), hdr[6] = { 0x81, (uint8_t)(0x80 | rl), 0x00, 0x00, 0x00, 0x00 };
        c.write(hdr, 6);
        for (uint8_t i = 0; i < rl; i++) { uint8_t x = req[i]; c.write(&x, 1); }
        // read ~500 ms of answer and look for deviceSn / hostname inside the frames
        buf = ""; t0 = millis();
        while (millis() - t0 < 500 && buf.length() < 1800) {
            while (c.available()) buf += (char)c.read();
            delay(5);
        }
        c.stop();
        bool isK2 = buf.indexOf("hostname") >= 0 || buf.indexOf("deviceSn") >= 0
                 || buf.indexOf("modelVersion") >= 0;
        int a = buf.indexOf("\"deviceSn\":\"");
        if (a >= 0) { a += 12; int b = buf.indexOf('"', a); if (b > a) sn = buf.substring(a, b); }
        return isK2;
    }

    void reconcile() {
        bool used[10] = { false };
        bool changed = false;
        nvs.begin("tigerspool", false);
        // 1) match on the serial
        for (int i = 0; i < MAX_PRINTERS; i++) {
            if (printers[i].type != PT_CREALITY || printers[i].sn.isEmpty() || isOnline(i)) continue;
            for (int j = 0; j < nFound; j++) {
                if (used[j] || found[j].sn.isEmpty() || found[j].sn != printers[i].sn) continue;
                IPAddress ip = base; ip[3] = found[j].oct;
                String s = ip.toString();
                if (s != printers[i].host) {
                    Serial.printf("[discovery] %s: IP %s -> %s (serial)\n", printers[i].name.c_str(),
                                  printers[i].host.c_str(), s.c_str());
                    printers[i].host = s; char k[6]; snprintf(k, sizeof(k), "p%dh", i);
                    nvs.putString(k, s); changed = true;
                }
                used[j] = true; pLastSeen[i] = 0;
            }
        }
        // 2) match 1:1 (exactly one unmatched offline K2 <-> exactly one found free)
        int io = -1, jo = -1, no = 0, nj = 0;
        for (int i = 0; i < MAX_PRINTERS; i++)
            if (printers[i].type == PT_CREALITY && !isOnline(i)) { no++; io = i; }
        for (int j = 0; j < nFound; j++) if (!used[j]) { nj++; jo = j; }
        if (no == 1 && nj == 1) {
            IPAddress ip = base; ip[3] = found[jo].oct;
            String s = ip.toString();
            if (s != printers[io].host) {
                Serial.printf("[discovery] %s: IP %s -> %s (1:1)\n", printers[io].name.c_str(),
                              printers[io].host.c_str(), s.c_str());
                printers[io].host = s; char k[6]; snprintf(k, sizeof(k), "p%dh", io);
                nvs.putString(k, s); changed = true; pLastSeen[io] = 0;
            }
        }
        nvs.end();
        Serial.printf("[discovery] done: %d K2 on the LAN, %s\n", nFound, changed ? "IPs corrected" : "no change");
    }

    void tick() {
        if (st == IDLE) {
            if (!WiFi.isConnected()) return;
            if (lastRun && millis() - lastRun < 180000) return;       // no max 1x / 3 min
            bool anyOff = false;
            for (int i = 0; i < MAX_PRINTERS; i++)
                if (printers[i].type == PT_CREALITY && !isOnline(i)) anyOff = true;
            if (!anyOff) return;
            base = WiFi.localIP(); nFound = 0; cur = 1; st = SWEEP;
            lastRun = millis();
            Serial.printf("[discovery] varrer %d.%d.%d.1-254 :9999...\n", base[0], base[1], base[2]);
        }
        if (st == SWEEP) {
            for (int k = 0; k < 4 && cur <= 254; k++, cur++) {
                IPAddress ip = base; ip[3] = cur;
                if (ip == WiFi.localIP()) continue;
                String sn;
                if (probe(ip, sn) && nFound < 10) {
                    found[nFound++] = { (uint8_t)cur, sn };
                    Serial.printf("[discovery] K2 @ %s sn=%s\n", ip.toString().c_str(), sn.c_str());
                }
            }
            if (cur > 254) st = RECONCILE;
        }
        if (st == RECONCILE) { reconcile(); st = IDLE; }
    }
}

// ---- printer list helpers ------------------------------------------------
// Everything that used to draw is gone: the screens live in src/ui/ and are
// built from LVGL widgets. What is left here is the data the screens need.

static int visiblePrinters(int idx[MAX_PRINTERS]) {
    int n = 0;
    for (int i = 0; i < MAX_PRINTERS; i++) if (printerVisible(i)) idx[n++] = i;
    return n;
}


// ---- NVS ---------------------------------------------------------------
// One-time migration from the prototype's namespaces.
//
// The prototype stored everything under "k2cfg" (and the account under
// "ttcfg"), names that belong to a Creality-only ancestor. Renaming them was
// right; dropping the data was not. Without this, every existing device reboots
// into the setup portal and its owner has to type a Wi-Fi password again to
// recover a device that was working - which is exactly the friction this
// product exists to remove.
//
// Runs once: the copy is persisted, so the old namespace is read at most one
// more time in the life of a device.
static void migrateLegacyConfig() {
    Preferences dst;
    dst.begin("tigerspool", true);
    bool alreadyDone = dst.getString("ssid", "").length() || dst.getBool("migrated", false);
    dst.end();
    if (alreadyDone) return;

    Preferences src;
    if (!src.begin("k2cfg", true)) return;
    String ssid = src.getString("ssid", "");
    if (ssid.isEmpty()) { src.end(); return; }

    dst.begin("tigerspool", false);
    dst.putString("ssid", ssid);
    dst.putString("pass", src.getString("pass", ""));
    // NOT the language: the prototype ordered its enum PT, EN, ES, FR and this
    // firmware orders it EN, FR, DE, ES, ... Carrying the old index over would
    // silently pick a different language. The picker asks once instead.
    dst.putInt("printerIdx", src.getInt("psel", 0));
    for (int i = 0; i < MAX_PRINTERS; i++) {
        char k[6];
        snprintf(k, sizeof(k), "p%dt", i); dst.putInt(k, src.getInt(k, 0));
        snprintf(k, sizeof(k), "p%dn", i); dst.putString(k, src.getString(k, ""));
        snprintf(k, sizeof(k), "p%dh", i); dst.putString(k, src.getString(k, ""));
        snprintf(k, sizeof(k), "p%ds", i); dst.putString(k, src.getString(k, ""));
        snprintf(k, sizeof(k), "p%dc", i); dst.putString(k, src.getString(k, ""));
    }
    dst.putBool("migrated", true);
    dst.end();
    src.end();

    // The account session lived in its own namespace and moves with it.
    Preferences oldAcc, newAcc;
    if (oldAcc.begin("ttcfg", true)) {
        String refresh = oldAcc.getString("refresh", "");
        if (refresh.length()) {
            newAcc.begin("tsaccount", false);
            newAcc.putString("refresh", refresh);
            newAcc.putString("email", oldAcc.getString("email", ""));
            newAcc.putString("uid",   oldAcc.getString("uid", ""));
            newAcc.end();
        }
        oldAcc.end();
    }
    Serial.printf("[config] migrated k2cfg -> tigerspool (network '%s')\n", ssid.c_str());
}

static void loadCfg() {
    nvs.begin("tigerspool", true);
    wifiSsid = nvs.getString("ssid", "");
    wifiPass = nvs.getString("pass", "");
    selectedPrinter     = nvs.getInt("printerIdx", 0);
    for (int i = 0; i < MAX_PRINTERS; i++) {
        char k[6];
        snprintf(k, sizeof(k), "p%dt", i); printers[i].type = (PrinterType)nvs.getInt(k, 0);
        snprintf(k, sizeof(k), "p%dn", i); printers[i].name = nvs.getString(k, "");
        snprintf(k, sizeof(k), "p%dh", i); printers[i].host = nvs.getString(k, "");
        snprintf(k, sizeof(k), "p%ds", i); printers[i].sn   = nvs.getString(k, "");
        snprintf(k, sizeof(k), "p%dc", i); printers[i].cc   = nvs.getString(k, "");
        snprintf(k, sizeof(k), "p%dv", i); printers[i].visible = nvs.getBool(k, true);
    }
    String oldK2 = nvs.getString("k2ip", "");
    nvs.end();

    // Legacy single-printer format: one "k2ip" key becomes printer 0, and the
    // conversion is persisted so this is read at most once more.
    if (printers[0].type == PT_NONE && oldK2.length()) {
        printers[0].type = PT_CREALITY; printers[0].name = "K2"; printers[0].host = oldK2;
        nvs.begin("tigerspool", false);
        nvs.putInt("p0t", PT_CREALITY);
        nvs.putString("p0n", "K2");
        nvs.putString("p0h", oldK2);
        nvs.remove("k2ip");
        nvs.end();
        Serial.println("[config] migrado k2ip -> p0 (K2)");
    }
    if (selectedPrinter < 0 || selectedPrinter >= MAX_PRINTERS || printers[selectedPrinter].type == PT_NONE) {
        selectedPrinter = 0;
        for (int i = 0; i < MAX_PRINTERS; i++) if (printers[i].type != PT_NONE) { selectedPrinter = i; break; }
    }
}
// Visibility is the user's, not the account's: a sync must never put a printer
// back on screen that someone deliberately hid.
// Screen preferences. Both are the user's and both survive a reboot: a device
// that forgets it was dimmed is a device that blinds someone every morning.
uint8_t screenBrightness = 80;
int     screenSleepSec   = 60;
int     screenRotation   = SCR_ROTATION;
bool    screenAutoRot    = false;

static void loadScreenPrefs() {
    nvs.begin("tigerspool", true);
    screenBrightness = (uint8_t)nvs.getInt("bright", 80);
    screenSleepSec   = nvs.getInt("sleep", 60);
    // A device that has never been told which way up it is asks the
    // accelerometer, once. Anything the user does afterwards - the button on
    // the language screen, or the Display setting - overwrites this and it
    // never runs again, because the key now exists.
    screenRotation = nvs.getInt("rot", -1);
    screenAutoRot  = nvs.getInt("autorot", 0) != 0;
    if (screenRotation < 0) {
        int fromImu = imu::suggestedRotation();
        screenRotation = (fromImu >= 0) ? fromImu : SCR_ROTATION;
        Serial.printf("[ui] first boot: orientation %d %s\n", screenRotation,
                      fromImu >= 0 ? "from the accelerometer" : "(flat - kept the default)");
    }
    nvs.end();
    lvgl_port::setBacklight(screenBrightness);
}
static void saveScreenPrefs() {
    nvs.begin("tigerspool", false);
    nvs.putInt("bright", screenBrightness);
    nvs.putInt("sleep", screenSleepSec);
    nvs.putInt("rot", screenRotation);
    nvs.putInt("autorot", screenAutoRot ? 1 : 0);
    nvs.end();
    lvgl_port::setBacklight(screenBrightness);
}

static void savePrinterVisible(int i, bool v) {
    char k[6];
    snprintf(k, sizeof(k), "p%dv", i);
    nvs.begin("tigerspool", false);
    nvs.putBool(k, v);
    nvs.end();
    printers[i].visible = v;
}

static void saveSel(int i) {
    nvs.begin("tigerspool", false);
    nvs.putInt("printerIdx", i);
    nvs.end();
}

// ---- Wi-Fi / startup -------------------------------------------------------
static const uint32_t WIFI_TIMEOUT_MS = 30000;   // 30 s por tentativa; se falhar -> portal AP

static bool wifiConnect() {
    if (wifiSsid.isEmpty()) return false;      // no network saved -> setup portal
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) {
        int left = (int)((WIFI_TIMEOUT_MS - (millis() - t0)) / 1000) + 1;
        screen_setup::showWifiConnecting(wifiSsid.c_str(), left);
        lvgl_port::loop();
        delay(250);
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[wifi] timeout after 30 s with no connection");
        screen_setup::showWifiFailed(wifiSsid.c_str());
        lvgl_port::loop();
        return false;
    }
    Serial.printf("[wifi] OK %s\n", WiFi.localIP().toString().c_str());
    return true;
}
static void onWifiUp() {
    // Confirm the running image so the bootloader stops treating it as a
    // candidate, and say in the log whether there is a slot to update into.
    ota::begin();
    if (!webStarted) { webcfg::begin(); webStarted = true; }
}

// Where to go once there is a network. An unlinked device has nothing to show
// on the printer list, so it asks for the account first - that is the step that
// fills the list.
static State afterWifi() {
    return ttcloud::haveSession() ? ST_PRINTER : ST_ACCOUNT;
}
// Set by startConfigAP once the QR is on the panel, so ST_AP does not encode
// it a second time. Encoding the QR is the most expensive thing on that screen.
static bool apScreenDrawn = false;

static void startConfigAP() {
    // The QR goes up FIRST, and the radio work happens behind it.
    //
    // The SSID is derived from the MAC, so it is known before the radio has
    // done anything - there is no reason to make someone watch a dead screen
    // while the access point comes up. Drawn the other way round, choosing a
    // language on a new device was followed by seconds of nothing, which reads
    // as a device that has crashed rather than one that is working.
    screen_setup::showWifi(webcfg::apName(), webcfg::apPass());
    lvgl_port::loop();
    apScreenDrawn = true;

    webcfg::beginAP();
    state = ST_AP;
    stateSince = millis();
}
static void goAfterLang() {
    // Already associated while the language screen was up? Then there is
    // nothing to wait for.
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[wifi] already up: %s\n", WiFi.localIP().toString().c_str());
        onWifiUp(); state = afterWifi(); stateSince = millis();
        return;
    }
    if (wifiConnect()) { onWifiUp(); state = afterWifi(); stateSince = millis(); }
    else startConfigAP();                 // no usable network -> open the setup portal
}
static void backToPrinters() {
    screen_home::leave();            // force a full LVGL repaint on re-entry
    screen_slots::invalidate();
    if (backend) { backend->stop(); backend = nullptr; }
    selSlot = -1;
    // Probe every printer again from scratch: whatever was known about
    // reachability is stale the moment we stop talking to one.
    for (int i = 0; i < MAX_PRINTERS; i++) pLastSeen[i] = 0;
    state = ST_PRINTER;
    stateSince = millis();
}
static void selectPrinter(int i) {
    if (i < 0 || i >= MAX_PRINTERS || printers[i].type == PT_NONE) return;
    if (backend) backend->stop();
    selectedPrinter = i; saveSel(i);
    switch (printers[i].type) {
        case PT_FF_C5:     backend = &flashForgeBackend;    break;
        case PT_BAMBU:     backend = &bambuBackend; break;
        case PT_SNAPMAKER: backend = &snapmakerBackend;  break;
        default:           backend = &crealityBackend;    break;
    }
    backend->begin(printers[i]);
    selSlot = -1; resultMsg = "";
    state = ST_GRID; stateSince = millis();
    Serial.printf("[ui] printer %d '%s' (%s)\n", i, printers[i].name.c_str(), typeTag(printers[i].type));
}

// ---- setup / loop -----------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(150);
    lcd.init();
    lcd.setRotation(SCR_ROTATION);
    lcd.setBrightness(200);

    // The boot screen, pushed straight from flash before LVGL exists. Drawn
    // here and not later because "here" is the first instant the panel can show
    // anything: everything after this line - LVGL, the language table, the NVS
    // read, the reader handshake - happens with the logo already up.
    //
    // Held for a second, and the second is not wasted: the deadline is set
    // here and only waited out at the END of setup(), so the whole boot -
    // LVGL, the language table, NVS, the reader - happens inside it. On a
    // board that takes longer than a second to come up, nothing is added at
    // all. Without this the device boots faster than the eye reads, and the
    // logo is a flicker nobody can see.
    lvgl_port::drawSplash();
    const uint32_t splashUntil = millis() + 1000;

    canvas.setPsram(true);
    canvas.setColorDepth(16);
    canvasReady = (canvas.createSprite(SCR_W, SCR_H) != nullptr);

    // (panel colour self-test lived here; re-add it if the tint returns)

    // LVGL owns the printer list. The remaining screens are still raw-drawn and
    // are being ported one at a time - see docs/MIGRATION.md.
    lvgl_port::begin();

    i18n::begin();
    migrateLegacyConfig();     // must run before anything reads the new namespace
    loadCfg();
    imu::begin();          // before loadScreenPrefs: first boot asks it
    loadScreenPrefs();
    // After lvgl_port::begin(), because turning the panel repaints it. The
    // boot logo above was drawn at the compiled default, which is the right
    // way up for the reference shell; a device mounted the other way sees the
    // logo one way and everything after it the other, for one second.
    lvgl_port::setRotation(screenRotation);
    ttcloud::begin();

    nfcReady = reader::begin();
    if (!nfcReady) Serial.printf("[reader] %s\n", reader::lastError().c_str());

    if (i18n::chosen()) {
        goAfterLang();
    } else {
        // First boot. Start associating NOW, in the background, while the user
        // reads the language list: the radio has nothing else to do and the
        // choice takes a few seconds. By the time they tap, the network is
        // usually already up, so the Wi-Fi step is skipped entirely instead of
        // making them wait for something that could have happened already.
        //
        // On a genuinely new device there are no credentials and this does
        // nothing, which is the correct outcome too.
        if (!wifiSsid.isEmpty()) {
            WiFi.mode(WIFI_STA);
            WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
            Serial.printf("[wifi] associating to '%s' behind the language screen\n",
                          wifiSsid.c_str());
        }
        langFromSettings = false;
        state = ST_LANG; stateSince = millis();
    }

    // Whatever is left of the splash's second. Usually nothing: the work above
    // has already spent it.
    while ((int32_t)(splashUntil - millis()) > 0) delay(5);

}

void loop() {
    // The config page belongs to the network, not to a screen: as soon as there
    // is an address, http://tigerspool.local answers. That also means the setup
    // screens are reachable for a remote screenshot, which is how this UI gets
    // checked without someone standing in front of the device.
    if (!webStarted && WiFi.status() == WL_CONNECTED) onWifiUp();

    // The backlight is the only thing that sleeps. Everything below this line
    // keeps running whether the screen is lit or not.
    // Follow the accelerometer, but only when the user asked for it and only
    // on a reading the sensor is sure of. Checked once a second: turning a box
    // over is not a gesture that needs millisecond response, and polling an
    // I2C sensor the touch panel shares is not free.
    if (screenAutoRot) {
        static uint32_t lastLook = 0;
        if (millis() - lastLook > 1000) {
            lastLook = millis();
            int want = imu::suggestedRotation();
            if (want >= 0 && want != screenRotation) {
                screenRotation = want;
                lvgl_port::setRotation(want);
                nvs.begin("tigerspool", false);
                nvs.putInt("rot", want);
                nvs.end();
            }
        }
    }

    // The screen never sleeps during setup. Every one of these states puts
    // something on the panel that has to be READ off it - a QR code to scan, a
    // pairing code to type, a network being joined - and a screen that goes
    // dark while someone is holding a phone up to it is a screen that has
    // failed at its one job. The sleep timeout is for the home screen, where
    // the device sits idle between spools.
    const bool inSetup = (state == ST_LANG || state == ST_WIFI || state == ST_AP
                       || state == ST_ACCOUNT || state == ST_WEB_PAIR);
    lvgl_port::sleepTick(inSetup ? 0 : screenSleepSec, screenBrightness);

    if (backend) backend->loop();
    if (webStarted || webcfg::apActive()) webcfg::loop();

    // A Google pairing started from the phone puts the same QR on this screen
    // for as long as it is waiting. Two reasons, and the second is the one that
    // made it worth doing: someone who started on their phone can finish on a
    // PC by scanning the box instead of retyping a code, and a box that is
    // waiting stops looking like a box that is idle.
    //
    // Read here rather than pushed from webcfg: the web server runs in the
    // same loop but state belongs to this file, and a screen driven from two
    // places disagrees with itself after the next redraw.
    {
        static State beforePair = ST_PRINTER;
        String purl, pcode; int pleft = 0;
        bool pairing = webcfg::webPairing(purl, pcode, pleft);
        if (pairing && state != ST_WEB_PAIR) {
            beforePair = state;
            screen_home::leave();
            state = ST_WEB_PAIR; stateSince = millis();
        } else if (!pairing && state == ST_WEB_PAIR) {
            screen_setup::hide();
            screen_home::leave();
            state = beforePair; stateSince = millis();
        }
        if (state == ST_WEB_PAIR) {
            screen_setup::showPairing(purl.c_str(), pcode.c_str(), pleft);
            lvgl_port::loop();
            if (webStarted || webcfg::apActive()) webcfg::loop();
            webcfg::pairTick();      // the device asks, not the phone
            return;
        }
    }

    // One check, once, a little after boot. It is what lets the Settings menu
    // colour the Update row before anyone opens the update page - the only
    // announcement a waiting update gets. Deliberately not repeated on a timer:
    // nothing here needs to know within the hour, and a device that phones home
    // every few minutes is a device that phones home for no reason.
    // Twenty seconds after boot, then every six hours.
    //
    // The boot check alone was not enough: this box sits on a shelf next to a
    // printer and plenty of them will never be switched off. A device that
    // checks once in its life learns about one update and then stops.
    static const uint32_t CHECK_EVERY_MS = 6UL * 60 * 60 * 1000;
    static uint32_t nextCheckAt = 20000;
    static bool     everChecked = false;
    if (WiFi.isConnected() && (int32_t)(millis() - nextCheckAt) >= 0) {
        if (ota::checkAsync()) { everChecked = true; nextCheckAt = millis() + CHECK_EVERY_MS; }
        else                     nextCheckAt = millis() + 60000;   // busy or offline: soon
    }

    // Told once per version, not once per boot and not once per check. An
    // update the device never mentions is an update nobody installs - the
    // orange row in Settings is seen only by someone who already went looking
    // - but repeating it every six hours for a version already declined is
    // nagging, and nagging is what teaches people to dismiss without reading.
    // A NEWER version speaks up again, because it is news.
    //
    // Held to the home screen so it cannot land on top of a spool being
    // assigned.
    static String notifiedVersion;
    if (everChecked && state == ST_PRINTER && ota::state() == ota::AVAILABLE
        && notifiedVersion != ota::latestVersion()) {
        notifiedVersion = ota::latestVersion();
        screen_home::leave();
        screen_settings::invalidate();
        state = ST_UPDATE_NOTICE; stateSince = millis();
    }

    if (!nfcReady && millis() - nfcLastTry > 2000) {
        nfcLastTry = millis();
        nfcReady = reader::begin();
        if (nfcReady) Serial.println("[reader] PN532 OK (retry)");
    }

    switch (state) {

    case ST_LANG: {
        screen_setup::showLanguage(false, langFromSettings);
        lvgl_port::loop();

        // The rotate button on the first-boot header. Flipping by hand is a
        // decision, so it also settles the question the accelerometer was
        // answering: automatic following goes off, and stays off unless the
        // user turns it on themselves under Display.
        if (screen_setup::takeRotate()) {
            screenRotation = (screenRotation == 0) ? 2 : 0;
            screenAutoRot  = false;
            saveScreenPrefs();
            lvgl_port::setRotation(screenRotation);
            screen_setup::showLanguage(true, langFromSettings);   // rebuild the right way up
            lvgl_port::loop();
        }

        // Opened from Settings to see which language is set, and closed again
        // without touching it. Without this the only way out was to pick one.
        if (langFromSettings && screen_setup::takeBack()) {
            screen_setup::hide();
            screen_settings::invalidate();
            state = ST_SETTINGS; stateSince = millis();
            break;
        }

        int pick = screen_setup::takeLanguage();
        if (pick >= 0) {
            i18n::set((Lang)pick);
            screen_setup::hide();
            if (langFromSettings) {
                screen_settings::invalidate();
                state = ST_SETTINGS; stateSince = millis();
            } else {
                goAfterLang();
            }
        }
        break;
    }

    case ST_WIFI:
        startConfigAP();                      // Wi-Fi failed -> AP portal
        return;

    case ST_AP: {
        // The portal joins the network on its own and takes the access point
        // down afterwards. Nothing tells this state machine, so it has to
        // notice - without this the device sits on the setup screen forever
        // while it is already online, which is exactly what it did.
        if (WiFi.status() == WL_CONNECTED) {
            loadCfg();                    // the portal wrote new credentials
            onWifiUp();
            screen_setup::hide();
            Serial.printf("[wifi] portal joined '%s' as %s\n",
                          WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
            state = afterWifi(); stateSince = millis();
            break;
        }
        // Drawn once. The screen has nothing that changes: encoding the QR is
        // the most expensive thing on it, and the payload never varies.
        // Two screens, and which one is up is decided by whether anybody has
        // joined yet.
        //
        // Before: the Wi-Fi QR. After: a QR pointing at the portal itself.
        // That second one is the fallback for a phone whose captive-portal
        // sheet never opens - the page is up and reachable the whole time, and
        // this is how someone reaches it without being read an IP address out
        // loud. It costs nothing to show, because the moment a phone is
        // associated the join QR has done its job and is only in the way.
        //
        // Drawn on the transition, never every pass: encoding a QR is the most
        // expensive thing on either screen.
        static int apShown = -1;                 // -1 nothing, 0 join, 1 portal
        int want = webcfg::apClients() > 0 ? 1 : 0;
        if (apShown != want) {
            if (want) screen_setup::showPortalReady(webcfg::url().c_str());
            else      screen_setup::showWifi(webcfg::apName(), webcfg::apPass());
            apShown = want;
            apScreenDrawn = true;
        }
        lvgl_port::loop();
        return;
    }

    // ---- linking the TigerTag account -----------------------------------
    //
    // The QR carries the code in its URL, so scanning it is the whole
    // interaction. See docs/ACCOUNT-PAIRING.md.
    //
    // pairStart and pairPoll are blocking HTTPS calls. They stall the UI for
    // about a second each, which is visible but not confusing on a screen that
    // is already showing a countdown; moving them onto the sync task is worth
    // doing once the settings screen needs the same flow.
    case ST_ACCOUNT: {
        // Ask which route first: an account made with Google has no password
        // to type, one made with an email has no Google to fall back on, and
        // the device cannot tell them apart. One tap, and it removes the dead
        // end where someone reaches a screen that cannot serve them.
        static enum { CHOICE, EMAIL, STARTING, POLLING, FAILED } step = CHOICE;
        static String code, verifyUrl, pollToken;
        static uint32_t startedAt = 0, lastPoll = 0;
        static int intervalS = 5;
        static String failReason;

        if (step == CHOICE) {
            screen_setup::showSignInChoice();
            lvgl_port::loop();
            int c = screen_setup::takeSignInChoice();
            if (c == 0) { screen_setup::hide(); step = EMAIL; }
            else if (c == 1) { screen_setup::hide(); step = STARTING; }
            break;
        }

        // Email and password happen on the phone, on the device's own page,
        // where there is a keyboard and a password manager. The QR is just the
        // address - nothing to read off this screen and type into that one.
        if (step == EMAIL) {
            // /login, not "/": the configuration page opens with the Wi-Fi
            // picker and buries the account form below it, so a phone scanning
            // this QR landed on a network selector.
            String url = String("http://") + WiFi.localIP().toString() + "/login";
            screen_setup::showEmailPairing(url.c_str());
            lvgl_port::loop();
            if (screen_setup::takeBack()) { screen_setup::hide(); step = CHOICE; break; }
            // The page signs in; this notices when it has.
            if (ttcloud::haveSession()) {
                Serial.printf("[account] linked as %s\n", ttcloud::email().c_str());
                screen_setup::hide();
                step = CHOICE;
                state = ST_PRINTER; stateSince = millis();
            }
            break;
        }

        // Fetch the code on a task and show a spinner that actually spins.
        // Rendering a placeholder QR and swapping it for the real one a second
        // later invites someone to scan the wrong thing.
        if (step == STARTING) {
            static bool launched = false;
            if (!launched) { launched = ttcloud::startPairAsync(); }

            screen_setup::showPreparing();
            lvgl_port::loop();
            if (screen_setup::takeBack()) {
                screen_setup::hide(); launched = false; step = CHOICE; break;
            }

            String err;
            int r = ttcloud::pairAsyncTake(code, verifyUrl, pollToken, intervalS, err);
            if (r == 1) {
                launched = false; screen_setup::hide();
                startedAt = millis(); lastPoll = 0; step = POLLING;
            } else if (r == -1) {
                launched = false; failReason = err; screen_setup::hide(); step = FAILED;
            }
            break;
        }

        if (step == POLLING) {
            int left = 600 - (int)((millis() - startedAt) / 1000);
            if (left <= 0) { failReason = i18n::T(S_ERR); step = FAILED; break; }
            screen_setup::showPairing(verifyUrl.c_str(), code.c_str(), left);
            lvgl_port::loop();
            if (screen_setup::takeBack()) { screen_setup::hide(); step = CHOICE; break; }

            if (millis() - lastPoll >= (uint32_t)intervalS * 1000) {
                lastPoll = millis();
                String customToken, email, err;
                int r = ttcloud::pairPoll(pollToken, customToken, email, err);
                if (r == 1) {
                    if (ttcloud::signInWithCustomToken(customToken, email, err)) {
                        Serial.printf("[account] linked as %s\n", email.c_str());
                        screen_setup::hide();
                        step = CHOICE;                // ready for a next time
                        state = ST_PRINTER; stateSince = millis();
                        break;
                    }
                    failReason = err; step = FAILED;
                } else if (r == 2 || r == 3) {
                    failReason = i18n::T(S_ERR); step = FAILED;
                }
            }
            break;
        }

        // FAILED: say so, and let a tap start over rather than stranding here.
        screen_setup::showPairFailed(failReason.c_str());
        lvgl_port::loop();
        if (screen_setup::takeStartPairing()) { screen_setup::hide(); step = CHOICE; }
        break;
    }

    case ST_PRINTER: {
        // Account re-sync, ON ITS OWN TASK. This is the screen the user returns
        // to constantly and it must never wait on the network: the list always
        // comes from NVS, and the sync only updates it if something changed.
        // The reload happens here, in the UI loop, so nothing races printers[].
        if (ttcloud::due() && !ttcloud::asyncBusy()) ttcloud::startAsyncSync();
        {
            String s;
            if (ttcloud::asyncTake(s)) {
                if (ttcloud::consumeChanged()) {
                    loadCfg(); resultMsg = s;
                    for (int i = 0; i < MAX_PRINTERS; i++) pLastSeen[i] = 0;
                }
            }
        }
        probeTick();                 // background online/offline indicator
        disc::tick();                // LAN sweep when a Creality is unreachable

        {
            bool online[MAX_PRINTERS];
            for (int i = 0; i < MAX_PRINTERS; i++) online[i] = isOnline(i);
            screen_home::show(printers, MAX_PRINTERS, selectedPrinter,
                              online, ttcloud::asyncBusy(),
                              WiFi.isConnected() ? WiFi.RSSI() : 0);
        }
        lvgl_port::loop();

        {
            int tapped = screen_home::takeTappedPrinter();
            if (tapped >= 0) { screen_home::leave(); selectPrinter(tapped); }
            else if (screen_home::takeSettingsTap()) {
                screen_home::leave();
                screen_settings::invalidate();
                state = ST_SETTINGS; stateSince = millis();
            }
        }
        break;
    }

    case ST_SETTINGS: {
        int visible = 0, total = 0;
        for (int i = 0; i < MAX_PRINTERS; i++) {
            if (printers[i].type == PT_NONE) continue;
            total++; if (printers[i].visible) visible++;
        }
        // WiFi.SSID() and ttcloud::email() both return String BY VALUE. Held
        // in named locals, or the temporary dies at the end of the statement
        // and the struct carries a pointer into freed memory - which showed up
        // as an empty Wi-Fi value on the menu, not as a crash.
        String ssid  = WiFi.SSID();
        String who   = ttcloud::displayName();

        screen_settings::MenuState menu{};
        menu.wifiUp   = WiFi.isConnected();
        menu.signedIn = ttcloud::haveSession();
        menu.network  = menu.wifiUp   ? ssid.c_str()  : i18n::T(S_OFFLINE);
        menu.account  = menu.signedIn ? who.c_str()   : i18n::T(S_ADD_WEB);
        menu.visiblePrinters = visible;
        menu.totalPrinters   = total;
        menu.updateWaiting   = (ota::state() == ota::AVAILABLE);
        menu.latest          = ota::latestVersion();
        screen_settings::showMenu(menu);
        lvgl_port::loop();

        if (screen_settings::takeBack()) {
            screen_home::leave();
            state = ST_PRINTER; stateSince = millis(); break;
        }
        switch (screen_settings::takeEntry()) {
            case screen_settings::E_PRINTERS:
                screen_settings::invalidate();
                state = ST_PICK; stateSince = millis();
                break;
            case screen_settings::E_LANGUAGE:
                screen_setup::hide();
                langFromSettings = true;
                state = ST_LANG; stateSince = millis();
                break;
            case screen_settings::E_WIFI:
                screen_settings::invalidate();
                state = ST_SET_WIFI; stateSince = millis(); break;
            case screen_settings::E_ACCOUNT:
                screen_settings::invalidate();
                state = ST_SET_ACCOUNT; stateSince = millis(); break;
            case screen_settings::E_SCREEN:
                screen_settings::invalidate();
                state = ST_SET_SCREEN; stateSince = millis(); break;
            case screen_settings::E_UPDATE:
                screen_settings::invalidate();
                // Opening the page is the question. Asking the user to then
                // press "check" was making them state an intent they had
                // already stated by arriving. checkAsync() declines by itself
                // when it is busy or offline, so the button stays for the
                // offline case and for a retry.
                ota::checkAsync();
                state = ST_SET_UPDATE; stateSince = millis(); break;
            case screen_settings::E_RESTART:
                screen_settings::invalidate();
                state = ST_SET_RESTART; stateSince = millis(); break;
            case screen_settings::E_FACTORY:
                screen_settings::invalidate();
                state = ST_SET_FACTORY; stateSince = millis(); break;
            default: break;
        }
        break;
    }

    case ST_PICK: {
        screen_settings::showPrinters(printers, MAX_PRINTERS);
        lvgl_port::loop();

        int t = screen_settings::takeToggled();
        if (t >= 0) savePrinterVisible(t, !printers[t].visible);

        if (screen_settings::takeBack()) {
            screen_settings::invalidate();
            state = ST_SETTINGS; stateSince = millis();
        }
        break;
    }

    // Every settings view returns to the menu the same way, so the way back is
    // learned once.
    #define BACK_TO_SETTINGS()                                   \
        if (screen_settings::takeBack()) {                       \
            screen_settings::invalidate();                       \
            state = ST_SETTINGS; stateSince = millis(); break;   \
        }

    case ST_SET_WIFI: {
        screen_settings::showWifi(
            WiFi.isConnected() ? WiFi.SSID().c_str() : "",
            WiFi.isConnected() ? WiFi.localIP().toString().c_str() : "-",
            WiFi.macAddress().c_str(),
            WiFi.isConnected(),
            WiFi.isConnected() ? WiFi.RSSI() : 0);
        lvgl_port::loop();
        BACK_TO_SETTINGS();
        if (screen_settings::takeAction() == screen_settings::A_CHANGE_WIFI) {
            // Straight to the portal, without wiping the saved network: if the
            // user changes their mind the old one is still there.
            screen_setup::hide();
            startConfigAP();
        }
        break;
    }

    case ST_SET_ACCOUNT: {
        int n = 0;
        for (int i = 0; i < MAX_PRINTERS; i++) if (printers[i].type != PT_NONE) n++;
        screen_settings::showAccount(ttcloud::email().c_str(), n, ttcloud::haveSession());
        lvgl_port::loop();
        BACK_TO_SETTINGS();
        if (screen_settings::takeAction() == screen_settings::A_SIGN_OUT) {
            // The imported printers go with the session. Leaving them would
            // show a list belonging to an account nobody is logged into.
            ttcloud::forget();
            nvs.begin("tigerspool", false);
            for (int i = 0; i < MAX_PRINTERS; i++) {
                char k[6];
                snprintf(k, sizeof(k), "p%dt", i); nvs.putInt(k, 0);
            }
            nvs.end();
            loadCfg();
            screen_home::leave(); screen_settings::invalidate();
            Serial.println("[account] signed out, imported printers cleared");
            state = ST_ACCOUNT; stateSince = millis();
        }
        break;
    }

    case ST_SET_SCREEN: {
        screen_settings::showScreen(screenBrightness, screenSleepSec,
                                    screenRotation, screenAutoRot);
        lvgl_port::loop();
        BACK_TO_SETTINGS();
        int b = screen_settings::takeBrightness();
        int sl = screen_settings::takeSleep();
        if (b >= 0)  { screenBrightness = (uint8_t)b; saveScreenPrefs();
                       screen_settings::invalidate(); }
        if (sl >= 0) { screenSleepSec = sl; saveScreenPrefs();
                       screen_settings::invalidate(); }
        int r = screen_settings::takeRotation();
        if (r != screen_settings::ROT_NONE) {
            screenAutoRot = (r == screen_settings::AUTO_ROT);
            if (!screenAutoRot) { screenRotation = r; lvgl_port::setRotation(r); }
            saveScreenPrefs();
            screen_settings::invalidate();
        }
        break;
    }

    case ST_SET_UPDATE: {
        screen_settings::showUpdate(TIGERSPOOL_FW_VERSION, "stable",
                                    (int)ota::state(), ota::latestVersion(),
                                    ota::percent());
        lvgl_port::loop();

        // The new image is written and verified; the bootloader will start it.
        // The pause is so the screen that says so is actually read.
        if (ota::state() == ota::DONE) {
            if (millis() - stateSince > 1500) { delay(200); ESP.restart(); }
            break;
        }

        // No leaving mid-write: Back would return to Settings while the
        // download task keeps writing the spare slot, and the next screen would
        // give no sign that anything was happening.
        if (ota::state() != ota::DOWNLOADING) BACK_TO_SETTINGS();
        break;
    }

    case ST_UPDATE_NOTICE: {
        screen_settings::showUpdateNotice(TIGERSPOOL_FW_VERSION, ota::latestVersion());
        lvgl_port::loop();
        screen_settings::Action a = screen_settings::takeAction();
        if (a == screen_settings::A_INSTALL_NOW) {
            // Start it here rather than dropping the user on the update page
            // to press Install a second time. They just pressed Install.
            ota::applyAsync();
            screen_settings::invalidate();
            state = ST_SET_UPDATE; stateSince = millis();
        } else if (a == screen_settings::A_LATER) {
            screen_settings::invalidate();
            screen_home::leave();
            state = ST_PRINTER; stateSince = millis();
        }
        break;
    }

    case ST_SET_RESTART: {
        screen_settings::showRestart();
        lvgl_port::loop();
        BACK_TO_SETTINGS();
        if (screen_settings::takeAction() == screen_settings::A_RESTART) {
            Serial.println("[ui] restarting on request");
            delay(150); ESP.restart();
        }
        break;
    }

    case ST_SET_FACTORY: {
        // Two seconds of continuous contact. A destructive action has to cost
        // more than a stray finger, and letting go at any point cancels it.
        static uint32_t holdStart = 0;
        if (screen_settings::factoryHolding()) {
            if (!holdStart) holdStart = millis();
            uint32_t held = millis() - holdStart;
            screen_settings::showFactory((int)(held * 100 / 2000));
            if (held >= 2000) {
                Serial.println("[config] factory reset from settings");
                const char* names[] = { "tigerspool", "tsaccount", "k2cfg", "ttcfg" };
                for (const char* ns : names) {
                    Preferences w;
                    if (w.begin(ns, false)) { w.clear(); w.end(); }
                }
                delay(200); ESP.restart();
            }
        } else {
            holdStart = 0;
            screen_settings::showFactory(-1);
        }
        lvgl_port::loop();
        BACK_TO_SETTINGS();
        break;
    }

    case ST_GRID: {
        screen_slots::show(printers[selectedPrinter].name.c_str(), backend,
                           selSlot, nfcReady);
        lvgl_port::loop();

        if (screen_slots::takeBack()) { backToPrinters(); break; }
        int slot = screen_slots::takeTappedSlot();
        if (slot >= 0) {
            selSlot = slot; resultMsg = "";
            screen_scan::invalidate();
            state = ST_SCAN; stateSince = millis();
        }
        break;
    }

    case ST_SCAN: {
        screen_scan::showScan(backend ? backend->slotLabel(selSlot) : "?",
                              resultMsg.length() ? resultMsg.c_str() : nullptr,
                              backend && backend->connected(), nfcReady);
        lvgl_port::loop();

        if (screen_scan::takeCancel()) {
            selSlot = -1; screen_slots::invalidate();
            state = ST_GRID; break;
        }
        if (reader::present()) {
            if (reader::read(tag) && tag.ok) { state = ST_REVIEW; stateSince = millis(); }
            else resultMsg = reader::lastError();
        }
        break;
    }

    case ST_REVIEW: {
        screen_scan::showReview(backend ? backend->slotLabel(selSlot) : "?", tag,
                                backend && backend->connected(), nfcReady);
        lvgl_port::loop();

        if (screen_scan::takeCancel()) {
            selSlot = -1; screen_slots::invalidate();
            state = ST_GRID; break;
        }
        if (screen_scan::takeSend()) {
            sendOk = backend && backend->connected() && backend->assign(selSlot, tag);
            char m[48];
            if (sendOk) snprintf(m, sizeof(m), i18n::T(S_UPDATED), backend->slotLabel(selSlot));
            else        snprintf(m, sizeof(m), "%s",
                                 (backend && backend->connected()) ? i18n::T(S_SEND_FAIL)
                                                                   : i18n::T(S_PRINTER_OFF));
            resultMsg = m;
            // Re-read the printer's own state: several of these protocols
            // acknowledge a command they ignored, so the colour that actually
            // landed is the only thing worth showing.
            if (sendOk) backend->refresh();
            state = ST_RESULT; stateSince = millis();
        }
        break;
    }

    case ST_RESULT: {
        uint32_t landed = 0xFFFFFFFFu;
        if (sendOk && backend) {
            const SlotState& st = backend->slot(selSlot);
            if (st.known) landed = ((uint32_t)st.r << 16) | ((uint32_t)st.g << 8) | st.b;
        }
        screen_scan::showResult(backend ? backend->slotLabel(selSlot) : "?",
                                sendOk, resultMsg.c_str(), tag, landed);
        lvgl_port::loop();

        if (screen_scan::takeDismiss() || millis() - stateSince > 4000) {
            selSlot = -1; screen_slots::invalidate();
            state = ST_GRID;
        }
        break;
    }
    }

    delay(15);
}
