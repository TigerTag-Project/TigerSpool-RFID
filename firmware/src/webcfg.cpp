#include "webcfg.h"
#include <WiFi.h>
#include <WebServer.h>
#include "net/captive_dns.h"
#include <ESPmDNS.h>
#include <Preferences.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "ui/lvgl_port.h"
#include "config.h"
#include "ui/screen_setup.h"
#include "ui/screen_home.h"
#include "ui/screen_slots.h"
#include "ui/screen_scan.h"
#include "ui/screen_settings.h"
#include "ui/screen_read.h"
#include "ui/frame.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/fonts.h"
#include "net/portal_page.h"
#include "product_api.h"
#include "battery.h"
#include "tt_db.h"
#include <ArduinoJson.h>
#include "version.h"
#include "web_assets.h"
#include "net/ota.h"
#include <ArduinoJson.h>
#include "printer.h"
#include "printer_budget.h"
#include "tigertag_cloud.h"
#include "i18n.h"

// The offscreen canvas lives in main.cpp; /screen.bmp serialises it as-is.
// Declared at GLOBAL scope: inside the anonymous namespace below they
// would name different symbols and the link step would fail.
// The real list, so the chooser can be previewed as it will actually be
// seen: an empty one says nothing about how rows and the button share the
// screen, which is the whole question on this one.
extern PrinterCfg printers[];
// Owned by main.cpp; this only asks. See memtestTick() for what it does.
extern volatile bool g_memtestRequested;
extern volatile bool g_memtestAll;
extern LGFX_Sprite canvas;
extern bool        canvasReady;

namespace {
    // ---- legacy web page translations (UTF-8, order: PT, EN, ES, FR) ----
    //
    // Words for the small pages this file still serves itself: the account
    // sign-in, the Google pairing wait, and the short replies. Four languages,
    // where the portal and the device screens speak eight - these are seen
    // once, and by someone who has already chosen a language on the panel.
    //
    // The prototype's configuration form used to live here too. It was deleted
    // once the portal and the account replaced every part of it, and it is why
    // some rows below are no longer referenced; they cost nothing and the
    // table's order is checked against the enum, so they are left in place.
    enum Wid {
        W_CFGMODE, W_RESCAN, W_STAR_PW, W_NETS_FOUND, W_PICK,
        W_NET, W_PASS, W_KEEP_EMPTY, W_UNCHANGED, W_LANG, W_PRINTERS,
        W_LAN_HINT, W_PRINTER, W_TYPE, W_NAME, W_SERIAL, W_CHECKCODE,
        W_SAVE_RESTART, W_TT_ACCOUNT, W_CONNECTED, W_SYNC_NOW, W_TT_FORGET,
        W_TT_HINT, W_TT_LOGIN, W_RETRY_NET, W_WIPE, W_NONE,
        W_SAVED, W_RESTART_JOIN, W_WIPED, W_RESTARTING, W_RETRY_SAVED,
        W_LOGIN_FAIL, W_ACCT_LINKED, W_FAILED, W_ACCT_OFF,
        W_GOOGLE, W_EMAIL, W_OR, W_NO_ACCOUNT, W_SHOW_PW,
        W_PAIR_SCAN, W_PAIR_CODE, W_PAIR_WAIT, W_PAIR_DENIED, W_PAIR_EXPIRED,
        W_SYNC_STARTED, W_FORGET_ASK, W_SYNC_TIMEOUT, W_NO_ROOM,
        W_N
    };
    const char* const WT[W_N][4] = {
        /* W_CFGMODE      */ { "Modo de configuracao - escolhe a rede Wi-Fi e grava.",
                              "Setup mode - pick the Wi-Fi network and save.",
                              "Modo de configuracion - elige la red Wi-Fi y guarda.",
                              "Mode configuration - choisis le reseau Wi-Fi et enregistre." },
        /* W_RESCAN       */ { "procurar de novo", "scan again", "buscar de nuevo", "rechercher a nouveau" },
        /* W_STAR_PW      */ { ". * = com password.", ". * = password-protected.",
                              ". * = con contrasena.", ". * = protege par mot de passe." },
        /* W_NETS_FOUND   */ { "Redes encontradas", "Networks found", "Redes encontradas", "Reseaux trouves" },
        /* W_PICK         */ { "-- escolhe --", "-- pick one --", "-- elige --", "-- choisir --" },
        /* W_NET          */ { "Rede", "Network", "Red", "Reseau" },
        /* W_PASS         */ { "Password", "Password", "Contrasena", "Mot de passe" },
        /* W_KEEP_EMPTY   */ { " (vazio p/ manter)", " (blank to keep)", " (vacio para mantener)", " (vide pour garder)" },
        /* W_UNCHANGED    */ { "(sem alteracao)", "(unchanged)", "(sin cambios)", "(inchange)" },
        /* W_LANG         */ { "Idioma", "Language", "Idioma", "Langue" },
        /* W_PRINTERS     */ { "Impressoras", "Printers", "Impresoras", "Imprimantes" },
        /* W_LAN_HINT     */ { "Ativa o Modo LAN em cada uma. K2: WebSocket :9999. FlashForge C5: HTTP :8898. Bambu: MQTT :8883. Serial + code do ecra da impressora.",
                              "Enable LAN mode on each. K2: WebSocket :9999. FlashForge C5: HTTP :8898. Bambu: MQTT :8883. Serial + code from the printer screen.",
                              "Activa el Modo LAN en cada una. K2: WebSocket :9999. FlashForge C5: HTTP :8898. Bambu: MQTT :8883. Serial + code de la pantalla de la impresora.",
                              "Active le mode LAN sur chacune. K2: WebSocket :9999. FlashForge C5: HTTP :8898. Bambu: MQTT :8883. Serie + code depuis l ecran de l imprimante." },
        /* W_PRINTER      */ { "Impressora", "Printer", "Impresora", "Imprimante" },
        /* W_TYPE         */ { "Tipo", "Type", "Tipo", "Type" },
        /* W_NAME         */ { "Nome", "Name", "Nombre", "Nom" },
        /* W_SERIAL       */ { "Serial (FF / Bambu)", "Serial (FF / Bambu)", "Numero de serie (FF / Bambu)", "Numero de serie (FF / Bambu)" },
        /* W_CHECKCODE    */ { "Check / Access code (FF / Bambu)", "Check / Access code (FF / Bambu)",
                              "Check / Access code (FF / Bambu)", "Check / Access code (FF / Bambu)" },
        /* W_SAVE_RESTART */ { "Guardar e reiniciar", "Save and restart", "Guardar y reiniciar", "Enregistrer et redemarrer" },
        /* W_TT_ACCOUNT   */ { "Conta TigerTag", "TigerTag account", "Cuenta TigerTag", "Compte TigerTag" },
        /* W_CONNECTED    */ { "Ligado: ", "Connected: ", "Conectado: ", "Connecte : " },
        /* W_SYNC_NOW     */ { "Sincronizar maquinas agora", "Sync machines now", "Sincronizar maquinas ahora", "Synchroniser les machines" },
        /* W_TT_FORGET    */ { "Desligar a conta TigerTag", "Disconnect the TigerTag account",
                              "Desconectar la cuenta TigerTag", "Deconnecter le compte TigerTag" },
        /* W_TT_HINT      */ { "Importa as impressoras registadas na tua conta (Firebase). O login e so email/password.",
                              "Imports the printers registered in your account (Firebase). Login is just email/password.",
                              "Importa las impresoras registradas en tu cuenta (Firebase). El acceso es solo email/password.",
                              "Importe les imprimantes enregistrees dans ton compte (Firebase). La connexion est juste email/mot de passe." },
        // A button says what happens when it is pressed. Importing the
        // printers is the consequence, not the action.
        /* W_TT_LOGIN     */ { "Entrar", "Sign in", "Iniciar sesión", "Se connecter" },
        /* W_RETRY_NET    */ { "Tentar rede atual de novo", "Retry current network", "Reintentar la red actual", "Reessayer le reseau actuel" },
        /* W_WIPE         */ { "Apagar tudo", "Wipe everything", "Borrar todo", "Tout effacer" },
        /* W_NONE         */ { "Nenhuma", "None", "Ninguna", "Aucune" },
        /* W_SAVED        */ { "Guardado.", "Saved.", "Guardado.", "Enregistre." },
        /* W_RESTART_JOIN */ { "A reiniciar e a ligar a rede...", "Restarting and joining the network...",
                              "Reiniciando y conectando a la red...", "Redemarrage et connexion au reseau..." },
        /* W_WIPED        */ { "Apagado.", "Wiped.", "Borrado.", "Efface." },
        /* W_RESTARTING   */ { "A reiniciar...", "Restarting...", "Reiniciando...", "Redemarrage..." },
        /* W_RETRY_SAVED  */ { "Nova tentativa na rede guardada.", "Retrying the saved network.",
                              "Reintentando la red guardada.", "Nouvel essai sur le reseau enregistre." },
        /* W_LOGIN_FAIL   */ { "Login falhou", "Login failed", "Fallo de acceso", "Echec de connexion" },
        /* W_ACCT_LINKED  */ { "Conta ligada", "Account linked", "Cuenta conectada", "Compte lie" },
        /* W_FAILED       */ { "Falhou", "Failed", "Fallo", "Echoue" },
        /* W_ACCT_OFF     */ { "Conta desligada", "Account disconnected", "Cuenta desconectada", "Compte deconnecte" },
        /* W_GOOGLE       */ { "Continuar com Google", "Continue with Google",
                              "Continuar con Google", "Continuer avec Google" },
        /* W_EMAIL        */ { "Endereço de e-mail", "Email address",
                              "Dirección de correo", "Adresse e-mail" },
        /* W_OR           */ { "ou", "or", "o", "ou" },
        // Accents intact: this page is drawn by the phone's browser, so the
        // ASCII-only limit of the panel's compiled font never applied here.
        /* W_NO_ACCOUNT   */ { "Ainda sem conta? Cria-a no Tiger Studio Manager e adiciona lá as tuas impressoras.",
                              "No account yet? Create one in Tiger Studio Manager, then add your printers there.",
                              "¿Todavía sin cuenta? Créala en Tiger Studio Manager y añade allí tus impresoras.",
                              "Pas encore de compte ? Créez-le dans Tiger Studio Manager, puis ajoutez-y vos imprimantes." },
        /* W_SHOW_PW      */ { "Mostrar a password", "Show password",
                              "Mostrar la contraseña", "Afficher le mot de passe" },
        /* W_PAIR_SCAN    */ { "Lê o QR code no ecrã do TigerSpool",
                              "Scan the QR code on the TigerSpool screen",
                              "Escanea el código QR en la pantalla del TigerSpool",
                              "Scannez le QR code sur l'écran du TigerSpool" },
        /* W_PAIR_CODE    */ { "codigo", "code", "codigo", "code" },
        // No device named, for the same reason W_PAIR_OPEN was dropped: this
        // page is read from a phone that scanned the QR and from a PC where
        // someone typed the address off the device's screen, and it cannot
        // tell which. Accents restored - the browser draws this, not the panel.
        /* W_PAIR_WAIT    */ { "A aguardar aprovação...",
                              "Waiting for approval...",
                              "Esperando aprobación...",
                              "En attente d'approbation..." },
        /* W_PAIR_DENIED  */ { "Pedido recusado", "Request denied", "Solicitud rechazada", "Demande refusee" },
        /* W_PAIR_EXPIRED */ { "Codigo expirado - tenta de novo", "Code expired - try again",
                              "Codigo expirado - intenta de nuevo", "Code expire - reessaie" },
        /* W_SYNC_STARTED */ { "A sincronizar as impressoras em segundo plano.",
                              "Syncing your printers in the background.",
                              "Sincronizando las impresoras en segundo plano.",
                              "Synchronisation des imprimantes en arrière-plan." },
        /* W_FORGET_ASK   */ { "Desligar a conta? O dispositivo vai reiniciar.",
                              "Disconnect the account? The device will restart.",
                              "¿Desconectar la cuenta? El dispositivo se reiniciará.",
                              "Déconnecter le compte ? Le TigerSpool va redémarrer." },
        /* W_SYNC_TIMEOUT */ { "Sem resposta da sincronização.", "The sync did not answer.",
                              "La sincronización no respondió.", "La synchronisation n’a pas répondu." },
        /* W_NO_ROOM      */ { "Sem memória para ligar mais uma impressora.",
                              "Not enough memory to switch on one more printer.",
                              "Sin memoria para activar una impresora más.",
                              "Pas assez de mémoire pour activer une imprimante de plus." },
    };
    // The web form still carries its own four-column table (PT, EN, ES, FR),
    // inherited from the prototype. The device now has nine languages, so an
    // unmapped index would read past the end of every row.
    //
    // This maps what it can and falls back to English. The real fix is phase 7:
    // serve a static page from LittleFS with proper locale files, the way
    // TigerScale does - see docs/MIGRATION.md.
    const char* wl(Wid id) {
        int col;
        switch (i18n::current()) {
            case LANG_PT:
            case LANG_PT_PT: col = 0; break;
            case LANG_ES:    col = 2; break;
            case LANG_FR:    col = 3; break;
            default:         col = 1; break;   // English
        }
        return WT[id][col];
    }
    void reply(const String& title, const String& msg);   // fwd

    WebServer   server(80);
    Preferences p;
    uint32_t    restartAt = 0;
    uint32_t    apTeardownAt = 0;
    bool        apMode    = false;
    // The last scan that found something. An ESP32 cannot scan usefully while
    // a station is associated to its own access point - the radio is committed
    // to serving that client - so the list is taken BEFORE anyone joins and
    // kept. That is the only moment it can be taken.
    String      netsJson;
    // ------------------------------------------------------------------
    //  Names carry the last four hex digits of the station MAC.
    //
    //  A bare "tigerspool.local" works until there are two of them: mDNS
    //  refuses a duplicate, so the second device silently never claims the
    //  name and becomes unreachable by name.
    //
    //  The setup access point has the same problem and it is worse there.
    //  Two devices in setup mode both broadcasting "TigerSpool-Setup" means
    //  the phone joins one of them at random, and the user configures the
    //  wrong box without ever knowing.
    //
    //  The suffix costs nothing: the QR on the screen carries the SSID, so
    //  nobody types it, and the resolved name is shown on the device and on
    //  the portal's success page for anyone who needs it later.
    //
    //  The STATION MAC, not the AP's - the two differ by one on an ESP32, and
    //  the station's is what a DHCP reservation has to be made against.
    // ------------------------------------------------------------------
    char HOSTNAME_BUF[24];
    char AP_SSID_BUF[28];
    char AP_PASS_BUF[20];
    const char* HOSTNAME = HOSTNAME_BUF;
    const char* AP_SSID  = AP_SSID_BUF;
    const char* AP_PASS  = AP_PASS_BUF;

    void buildNames() {
        if (HOSTNAME_BUF[0]) return;                 // built once
        uint8_t mac[6] = {0};
        WiFi.macAddress(mac);                        // station interface
        snprintf(HOSTNAME_BUF, sizeof(HOSTNAME_BUF), "tigerspool-%02x%02x", mac[4], mac[5]);
        snprintf(AP_SSID_BUF,  sizeof(AP_SSID_BUF),  "TigerSpool-Setup-%02X%02X", mac[4], mac[5]);
        // The setup access point is WPA2, not open, and this is its key.
        //
        // Not for secrecy: the key is printed on the device's own screen and
        // carried in the QR, so nobody types it. It is here because Android
        // treats an open network that has no internet as a mistake to be
        // corrected - Samsung's adaptive Wi-Fi in particular drops back to
        // mobile data and stops probing for a portal - and an encrypted
        // network is handled as a deliberate choice instead. Reported from the
        // field on a Galaxy S24 that never showed the sign-in sheet.
        //
        // It earns its place a second way: during setup this portal accepts
        // the user's home Wi-Fi password and their TigerTag password, over
        // plain HTTP. On an open access point those cross the air in clear to
        // anyone within range.
        //
        // Derived from the MAC, so the QR, the screen and the radio always
        // agree, and a device reset comes back with the same key rather than
        // stranding whoever wrote it down.
        snprintf(AP_PASS_BUF, sizeof(AP_PASS_BUF), "tiger%02x%02x%02x",
                 mac[3], mac[4], mac[5]);
    }
    const IPAddress AP_IP(192, 168, 4, 1);
    const char* PTYPES[] = { "None", "Creality K2", "FlashForge Creator 5 Pro",
                             "Bambu Lab (A1/A2/P1/X1)", "Snapmaker (Moonraker)" };
    const int   NPTYPES  = 5;
    // Mirrors enum Lang exactly: the form writes this index straight into NVS,
    // so a shorter list here would silently store the wrong language.
    const char* LANGS[]  = { "English", "Français", "Deutsch", "Español",
                             "Italiano", "Polski", "Português (BR)", "Português (PT)" };

    String esc(const String& s) {
        String o; o.reserve(s.length() + 8);
        for (size_t i = 0; i < s.length(); i++) {
            char c = s[i];
            if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;";
            else if (c == '>') o += "&gt;"; else if (c == '"') o += "&quot;"; else o += c;
        }
        return o;
    }

    // ------------------------------------------------------------------
    //  Screen capture: /screen.bmp and /screen (a page that refreshes it)
    //
    //  Every draw goes through the offscreen 'canvas' sprite before
    //  pushSprite(), so its buffer IS the framebuffer. We serve it as 24-bit
    //  BMP: no compression to carry, and every browser reads it. 240*3 = 720
    //  bytes per row, a multiple of 4, so there is no padding to handle.
    //
    //  The handler runs in the same loop as the drawing (WebServer::
    //  handleClient is called from loop()), so there is no race on the buffer.
    // ------------------------------------------------------------------
    void le32(uint8_t* p, uint32_t v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }


    // A colour question the screenshot cannot answer.
    //
    // /screen.bmp serialises what LVGL DREW. Near black, an IPS panel's gamma
    // stops being linear, so two fills that differ by a few counts in the
    // buffer can be one colour on the glass - or one of them can come out with
    // a cast that is not in the data at all. This has bitten this project once
    // already, on the screen ground.
    //
    // So the card is not chosen from a capture. Six candidate fill/border pairs
    // are drawn on the panel at once, labelled, and whoever is standing in
    // front of it says which row separates. That is a measurement; reading a
    // BMP of it is not.
    void previewGreys() {
        lvgl_port::Lock lvglGuard;   // builds LVGL objects from the loop
        struct Pair { uint32_t fill, line; const char* label; };
        static const Pair PAIRS[] = {
            { 0x000000, 0x3A4046, "1  000000 / 3A4046" },
            { 0x111417, 0x3A4046, "2  111417 / 3A4046" },
            { 0x1A1E22, 0x454C54, "3  1A1E22 / 454C54" },
            { 0x1B212A, 0x2E3646, "4  1B212A / 2E3646" },
            { 0x22262B, 0x4E555D, "5  22262B / 4E555D" },
            { 0x252A2F, 0x3A4046, "6  252A2F / 3A4046" },
        };
        lv_obj_t* body = frame::build("Panel test", nullptr);
        lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        for (const Pair& p : PAIRS) {
            lv_obj_t* card = lv_obj_create(body);
            lv_obj_remove_style_all(card);
            lv_obj_set_size(card, LV_PCT(100), 38);
            lv_obj_set_style_bg_color(card, lv_color_hex(p.fill), 0);
            lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(card, lv_color_hex(p.line), 0);
            lv_obj_set_style_border_width(card, 1, 0);
            lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(card, 10, 0);
            lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_t* l = lv_label_create(card);
            lv_label_set_text(l, p.label);
            lv_obj_set_style_text_font(l, &font_ui_14, 0);
            lv_obj_set_style_text_color(l, lv_color_hex(theme::TEXT), 0);
            lv_obj_center(l);
        }

        // Primaries and a grey ramp, full brightness, as a control.
        //
        // The six cards above answer "which fill separates". This row answers
        // the question behind it: does the panel put out the colour it was
        // given at all. If R, G and B come out as themselves and the greys
        // climb evenly, the cards are a gamma question and the numbers just
        // need moving. If a dark grey lands blue here too, the fault is in the
        // pixel path - colour order, depth or inversion - and no palette
        // change would ever have fixed it.
        static const uint32_t RAMP[] = { 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF,
                                         0x808080, 0x404040, 0x202020, 0x101010 };
        lv_obj_t* strip = lv_obj_create(body);
        lv_obj_remove_style_all(strip);
        lv_obj_set_size(strip, LV_PCT(100), 34);
        lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
        lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
        for (uint32_t c : RAMP) {
            lv_obj_t* sw = lv_obj_create(strip);
            lv_obj_remove_style_all(sw);
            lv_obj_set_flex_grow(sw, 1);
            lv_obj_set_height(sw, 34);
            lv_obj_set_style_bg_color(sw, lv_color_hex(c), 0);
            lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
        }
    }


    // Every icon at once, each beside its name.
    //
    // Icons are drawn from primitives at three different sizes and two of them
    // are font glyphs, so "are they the same weight and the same diameter" is a
    // question no single screen answers - the menu shows five of them, and the
    // rest are below the fold on a panel that cannot be scrolled from here. Two
    // faults were found this way: a sun set at 16 px beside a globe drawn to 20,
    // and a reader row quietly borrowing the sun.
    // A spool that does not exist, for the reader screen: a full set of fields
    // so the layout is judged against its worst case rather than its best.
    TagInfo previewTag() {
        TagInfo t;
        t.ok = true;
        t.idProduct = 0x1234;
        t.r = 0x1E; t.g = 0x88; t.b = 0xE5;
        t.material = "PETG HF";
        t.brand = "Polymaker";
        t.aspect1Label = "Matte";
        t.diameterLabel = "1.75 mm";
        t.nozMin = 230; t.nozMax = 260;
        t.bedMin = 70;  t.bedMax = 85;
        t.dryTemp = 65; t.dryHours = 8;
        t.available = 742;
        t.unitLabel = "g";
        t.signature = TagInfo::SIG_VALID;
        return t;
    }

    void previewIcons() {
        lvgl_port::Lock lvglGuard;   // builds LVGL objects from the loop
        struct Row { icons::Id id; const char* name; };
        static const Row ROWS[] = {
            { icons::PRINTER, "PRINTER" }, { icons::WIFI,   "WIFI"   },
            { icons::USER,    "USER"    }, { icons::SCREEN, "SCREEN" },
            { icons::GLOBE,   "GLOBE"   }, { icons::NFC,    "NFC"    },
            { icons::UPDATE,  "UPDATE"  }, { icons::RESTART,"RESTART"},
            { icons::ERASE,   "ERASE"   },
        };
        lv_obj_t* body = frame::build("Icons", nullptr);
        lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        for (const Row& r : ROWS)
            frame::row(body, r.name, "", false, nullptr, nullptr, r.id, theme::TEXT);
    }

    void handleShot() {
        if (!canvasReady) { server.send(503, "text/plain", "no canvas"); return; }
        // The whole handler holds the LVGL lock, in pieces: it builds preview
        // screens, forces a repaint, and then reads the sprite the drawing task
        // paints into. Taken around each of those rather than for the duration,
        // because the send itself takes seconds and the panel must keep moving.

        // The shadow copy is not maintained frame by frame - that would put a
        // PSRAM write in the render path for a feature used a few times a day.
        // Ask for it, force a full repaint so every pixel passes through
        // flush_cb, and pump LVGL until the frame has been drawn.
        // Diagnostic: /screen.bmp?preview=lang|wifi|pair|pairfail renders one of
        // the setup screens for the capture and nothing else. The state machine
        // redraws on its next pass, so the device is not left showing it.
        //
        // This exists because the first-boot screens are, by definition, only
        // reachable on a device that has not been set up - which is exactly the
        // state a developer cannot get a networked screenshot out of.
        String preview = server.hasArg("preview") ? server.arg("preview") : String();
        if      (preview == "lang") screen_setup::showLanguage(true);
        else if (preview == "wifi") { buildNames(); screen_setup::showWifi(AP_SSID, AP_PASS); }
        else if (preview == "portal") screen_setup::showPortalReady("http://192.168.4.1");
        else if (preview == "pair") screen_setup::showPairing(
                     "https://tigersystem.io/pair/K7QF-3M2P", "K7QF-3M2P", 587);
        else if (preview == "pairfail") screen_setup::showPairFailed("Code expired");
        else if (preview == "account") screen_setup::showAccountIntro();
        else if (preview == "signin")  screen_setup::showSignInChoice();
        else if (preview == "waiting") screen_setup::showPreparing();
        else if (preview == "email")   screen_setup::showEmailPairing("http://192.168.20.170");
        else if (preview == "settings") screen_settings::showMenu({"Atelier", "benoit@atome3d.com", 3, 6, true, true, true, "1.6.0", 64, false});
        else if (preview == "apwifi")  { buildNames(); screen_setup::hide(); screen_setup::showWifi(AP_SSID, AP_PASS, true); }
        else if (preview == "apportal") { screen_setup::hide(); screen_setup::showPortalReady("http://192.168.4.1", true); }
        else if (preview == "setwifi")  screen_settings::showWifi("Atelier", "192.168.20.170",
                                                                  WiFi.macAddress().c_str(), true, 11, -55);
        else if (preview == "setwifi-fair") screen_settings::showWifi("Stargate", "192.168.20.170",
                                                                  WiFi.macAddress().c_str(), true, 6, -76);
        else if (preview == "setwifi-none") screen_settings::showWifi("", "-",
                                                                  WiFi.macAddress().c_str(), false, 0, 0);
        else if (preview == "setacct")  screen_settings::showAccount("benoit@atome3d.com", 6, true);
        else if (preview == "setbatt")  screen_settings::showBattery(3.86f, 74, false, 270, true);
        else if (preview == "setcharge") screen_settings::showBattery(3.76f, 42, true, 95, true);
        else if (preview == "setbattlow") screen_settings::showBattery(3.45f, 8, false, 25, true);
        else if (preview == "battnone") screen_settings::showBattery(4.05f, -1, false, -1, false);
        else if (preview == "setscreen") screen_settings::showScreen(80, 60, 2, false);
        // The state that cannot be reached on demand - the device is only ever
        // behind by accident - and the one whose layout is tightest.
        else if (preview == "setupdate") screen_settings::showUpdate(TIGERSPOOL_FW_VERSION, "stable",
                                             (int)ota::AVAILABLE, "9.9.9", 0);
        else if (preview == "updone")    screen_settings::showUpdate(TIGERSPOOL_FW_VERSION, "stable",
                                             (int)ota::UP_TO_DATE, "", 0);
        // Older on the left, newer on the right: the screen is about
        // moving forward, and a preview that shows it backwards is a
        // preview of a screen that cannot happen.
        else if (preview == "notice") screen_settings::showUpdateNotice("1.41.0", TIGERSPOOL_FW_VERSION);
        else if (preview == "setrestart") screen_settings::showRestart();
        else if (preview == "setfactory") screen_settings::showFactory();
        else if (preview == "pick")      screen_settings::showPrinters(printers, MAX_PRINTERS, false,
                                             budget::used(printers, MAX_PRINTERS, -1), false);
        else if (preview == "cloudslot") screen_slots::showCloudNotice("B2");
        else if (preview == "choose")    screen_settings::showChoosePrinters(printers, MAX_PRINTERS, false,
                                             budget::used(printers, MAX_PRINTERS, -1), false);
        else if (preview == "choosing")  screen_settings::showChoosePrinters(nullptr, 0, true, 0, false);
        else if (preview == "main")      screen_home::showMain(2, 3, true, -58, 3);
        else if (preview == "read")      screen_read::showWaiting();
        else if (preview == "readtag")   { screen_read::invalidate(); screen_read::showTag(previewTag()); }
        // The everyday loop: present the spool, confirm it, and the receipt
        // that tells you where to put it. `resultlong` is the layout's worst
        // case - a printer whose slots are called AMS2-4.
        else if (preview == "scan")      screen_scan::showScan("B2");
        else if (preview == "result")    screen_scan::showResult("B2", true, "", previewTag(), 3200);
        else if (preview == "resultlong") screen_scan::showResult("AMS2-4", true, "", previewTag(), 5000);
        else if (preview == "resultfail") screen_scan::showResult("B2", false,
                                             i18n::T(S_SEND_FAIL), previewTag(), 0);
        else if (preview == "greys")     previewGreys();
        else if (preview == "icons")     previewIcons();

        // The boot screen cannot be captured the way it is actually shown: it
        // is drawn before the web server exists. This redraws it on demand so
        // its geometry can be checked from a desk.
        //
        // It skips the LVGL pump below on purpose. That pump exists to make
        // LVGL paint the requested screen into the sprite - and painting is
        // exactly what would overwrite a bitmap that LVGL knows nothing about.
        if (preview == "splash") {
            lvgl_port::Lock lvglGuard;
            lvgl_port::drawSplash(true);
        } else {
            lvgl_port::requestCapture(true);
            { lvgl_port::Lock lvglGuard; lv_obj_invalidate(lv_scr_act()); }
            // The drawing task paints it; this only waits for it to happen.
            for (uint32_t t0 = millis(); millis() - t0 < 400; ) delay(5);
            lvgl_port::requestCapture(false);
        }

        // Hand the display back to the state machine.
        //
        // Every screen carries a "already showing, do not rebuild" guard so it
        // does not tear itself down under the finger pressing it. Rendering a
        // preview leaves those guards believing their screen is up, and the
        // state machine then never redraws - the device looks frozen while its
        // main loop is running perfectly, which is exactly what happened.
        //
        // A debug tool that changes what it is measuring is worse than no tool.
        if (preview.length()) {
            screen_setup::hide();
            screen_home::leave();
            screen_slots::invalidate();
            screen_scan::invalidate();
            screen_settings::invalidate();
            screen_read::invalidate();
        }

        const int W = 240, H = 320;
        const uint32_t rowBytes  = (uint32_t)W * 3;
        const uint32_t dataSize  = rowBytes * H;

        uint8_t hdr[54] = {0};
        hdr[0] = 'B'; hdr[1] = 'M';
        le32(hdr + 2,  54 + dataSize);
        le32(hdr + 10, 54);
        le32(hdr + 14, 40);
        le32(hdr + 18, (uint32_t)W);
        le32(hdr + 22, (uint32_t)H);      // positif = stocke de bas en haut
        hdr[26] = 1;
        hdr[28] = 24;
        le32(hdr + 34, dataSize);

        server.setContentLength(54 + dataSize);
        server.sendHeader("Cache-Control", "no-store");
        server.send(200, "image/bmp", "");
        server.sendContent((const char*)hdr, 54);

        // Convert the whole frame FIRST, into its own buffer, then send that.
        //
        // Two problems, one answer. Measured on the bench: this transfer runs
        // inside handleClient(), so the main loop was blocked for as long as
        // 230 KB took to leave - twelve seconds with a browser watching. The
        // obvious fix, pumping LVGL between rows, produced a torn screenshot:
        // LVGL goes on painting into the canvas while it is being read, so the
        // top of the image came from one frame and the bottom from another.
        //
        // Reading it out in one pass costs about 230 KB of PSRAM for the length
        // of one request, which this board has eight megabytes of. After that
        // the pixels are ours, LVGL can repaint as much as it likes, and the
        // send can stop to let it.
        uint8_t* shot = (uint8_t*)ps_malloc(dataSize);
        if (!shot) { server.sendContent("", 0); return; }
        uint8_t* o = shot;
        lvgl_port::lock();       // the sprite is written by the drawing task
        for (int y = H - 1; y >= 0; y--) {          // BMP stores the last row first
            for (int x = 0; x < W; x++) {
                uint32_t c = canvas.readPixel(x, y);    // RGB565 -> RGB888
                uint8_t r = (c >> 8) & 0xF8, g = (c >> 3) & 0xFC, b = (c << 3) & 0xF8;
                *o++ = b | (b >> 5);                // BMP is BGR
                *o++ = g | (g >> 6);
                *o++ = r | (r >> 5);
            }
        }
        lvgl_port::unlock();     // the pixels are ours now; LVGL may repaint
        for (uint32_t sent = 0; sent < dataSize; ) {
            const uint32_t chunk = min<uint32_t>(rowBytes * 8, dataSize - sent);
            server.sendContent((const char*)(shot + sent), chunk);
            sent += chunk;
            delay(1);                // the drawing task keeps the panel moving
        }
        free(shot);
        server.sendContent("", 0);
    }

    // The live screen page. It refreshes /screen.bmp on a loop, and - the point
    // of this handler existing at all rather than people opening the .bmp
    // directly - a click on the image is forwarded to /api/tap.
    //
    // That closes the loop for a person the same way it was already closed for
    // an agent: open the page on a laptop, drive the device by clicking its
    // picture, watch it react. Nobody has to be at the bench, and nobody has to
    // hand-write query strings to move one screen forward.
    //
    // Coordinates come from the image's bounding rect rather than from the
    // event offset, so the mapping survives the CSS scaling the image down on a
    // phone. A press that travels more than a few pixels is sent as a drag,
    // which is how a list is scrolled - the same distinction the touch panel
    // itself makes.
    void handleShotPage() {
        String p = F(
          "<!doctype html><meta charset=utf-8><meta name=viewport "
          "content='width=device-width,initial-scale=1'>"
          "<title>TigerSpool screen</title>"
          "<style>body{margin:0;background:#111;color:#888;font:13px system-ui;"
          "display:flex;flex-direction:column;align-items:center;gap:10px;padding:16px}"
          "img{width:240px;height:auto;max-width:92vw;aspect-ratio:240/320;"
          "image-rendering:pixelated;border-radius:8px;border:1px solid #333;"
          "cursor:crosshair;touch-action:none;-webkit-user-select:none;user-select:none}"
          "b{color:#ddd}#h{color:#666}</style>"
          "<img id=s draggable=false>"
          "<div>live &middot; <b id=n>0</b> frames &middot; <span id=e></span></div>"
          "<div id=h>click the screen to tap it &middot; drag to swipe</div>"
          "<script>"
          "const i=document.getElementById('s'),H=document.getElementById('h');"
          "let n=0,busy=0,seen=-1,pending=0;"
          // The bitmap is 150 KB; the counter is four bytes. Poll the cheap one
          // often and fetch the expensive one only when the panel actually
          // repainted - which is what makes moving between screens feel
          // immediate instead of waiting out an interval sized for the image.
          "function shot(){if(pending)return;pending=1;"
          "i.src='/screen.bmp?'+Date.now()}"
          "i.onload=()=>{pending=0;document.getElementById('n').textContent=++n};"
          "i.onerror=()=>{pending=0;document.getElementById('e').textContent='erreur'};"
          "async function poll(){"
          "try{const v=+await (await fetch('/screen.ver',{cache:'no-store'})).text();"
          "if(v!==seen){seen=v;shot()}"
          "document.getElementById('e').textContent=''}"
          "catch(_){document.getElementById('e').textContent='erreur'}"
          "setTimeout(poll,120)}"
          "function pt(ev){const r=i.getBoundingClientRect();"
          "const x=Math.round((ev.clientX-r.left)/r.width*240);"
          "const y=Math.round((ev.clientY-r.top)/r.height*320);"
          "return [Math.max(0,Math.min(239,x)),Math.max(0,Math.min(319,y))]}"
          "let dn=null;"
          "i.addEventListener('pointerdown',e=>{e.preventDefault();dn=pt(e)});"
          "i.addEventListener('pointerup',async e=>{"
          "if(!dn||busy)return;const up=pt(e),d=dn;dn=null;"
          "const far=Math.abs(up[0]-d[0])>12||Math.abs(up[1]-d[1])>12;"
          "const q=far?`x=${d[0]}&y=${d[1]}&x2=${up[0]}&y2=${up[1]}`"
          ":`x=${d[0]}&y=${d[1]}`;"
          "busy=1;H.textContent=(far?'swipe ':'tap ')+q;"
          "try{await fetch('/api/tap?'+q)}catch(_){H.textContent='tap failed'}"
          "busy=0;seen=-1});"
          "i.addEventListener('pointercancel',()=>{dn=null});"
          "poll();</script>");
        server.send(200, "text/html", p);
    }

    // ------------------------------------------------------------------
    //  The setup portal: one page, and three small endpoints behind it.
    //
    //  The page is served from PROGMEM with three placeholders filled in. It
    //  opens in the language chosen on the device, so someone who picked
    //  Portugues on the screen does not meet an English page on their phone.
    // ------------------------------------------------------------------
    const char* LANG_CODES[] = { "en", "fr", "de", "es", "it", "pl", "pt", "ptpt", "zh" };

    void startBackgroundScan();          // defined below, called from here

    void handlePortal() {
        buildNames();
        String page = FPSTR(PORTAL_HTML);
        page.replace("%SSID%", AP_SSID);
        page.replace("%FW%",   TIGERSPOOL_FW_VERSION);
        int l = (int)i18n::current();
        page.replace("%LANG%", LANG_CODES[(l >= 0 && l < (int)LANG_N) ? l : 0]);
        server.send(200, "text/html", page);
    }

    // Networks, strongest first and deduplicated. The signal is reported in dBm
    // and the page turns it into arcs - the mapping belongs with the drawing,
    // not here.
    // Kicked off the moment the access point comes up, so the results are
    // usually already waiting by the time a phone has joined, opened the page
    // and asked. Scanning on request made every user watch a spinner for two
    // seconds that the device could have spent before they arrived.
    // Started from handlePortal, NOT from beginAP. A scan needs the station
    // interface, and the radio then hops channels - which is fine while nobody
    // is associated and fatal in the two seconds after somebody is.
    //
    // The incident: scan the Wi-Fi QR code on a recent Android phone and the
    // captive-portal sheet never appears. The phone probes for a portal within
    // about a second of associating; this scan used to be launched from the
    // last line of beginAP(), so that probe landed while the access point was
    // off hopping channels. The probe times out, Android files the network
    // under "connected, no internet", and it does not ask again. The portal is
    // there and reachable the whole time - the phone simply stopped looking.
    //
    // Starting it when the portal page is served is late enough to be safe and
    // early enough to be useful: the page being served is itself proof that
    // the probe already succeeded, and the picker is two taps further on.
    // Mode is returned to AP-only by handleApiScan, which the page always
    // calls to fill that picker.
    void startBackgroundScan() {
        if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return;
        // The station interface has to actually be up before a scan can start.
        // Issued in the same breath as the mode change it fails outright, and
        // the failure is silent - which is how the network picker came up
        // empty on first open and only filled in after "rescan".
        WiFi.mode(WIFI_AP_STA);
        delay(80);
        WiFi.scanDelete();
        if (WiFi.scanNetworks(true, true) == WIFI_SCAN_FAILED)
            Serial.println("[webcfg] background scan refused to start");
    }

    // Reads whatever the last scan left behind and turns it into the portal's
    // list. Kept apart from the scanning so one can fail without the other.
    String scanToJson(int n) {
        int idx[32], m = n > 32 ? 32 : (n < 0 ? 0 : n);
        for (int i = 0; i < m; i++) idx[i] = i;
        for (int i = 0; i < m; i++)
            for (int j = i + 1; j < m; j++)
                if (WiFi.RSSI(idx[j]) > WiFi.RSSI(idx[i])) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }

        JsonDocument doc;
        JsonArray arr = doc["nets"].to<JsonArray>();
        String seen = "\n";
        for (int k = 0; k < m; k++) {
            String ssid = WiFi.SSID(idx[k]);
            if (!ssid.length() || seen.indexOf("\n" + ssid + "\n") >= 0) continue;
            seen += ssid + "\n";
            JsonObject o = arr.add<JsonObject>();
            o["s"] = ssid;
            o["r"] = WiFi.RSSI(idx[k]);
            o["k"] = WiFi.encryptionType(idx[k]) != WIFI_AUTH_OPEN;
        }
        String out; serializeJson(doc, out);
        return out;
    }

    // Keep a finished scan. Only one that found something replaces what is
    // held: an empty result while a phone is associated is the radio saying
    // "not now", not the flat saying "there are no networks".
    void harvestScan() {
        int n = WiFi.scanComplete();
        if (n <= 0) return;
        netsJson = scanToJson(n);
        Serial.printf("[webcfg] scan cached: %d network(s)\n", n);
        WiFi.scanDelete();
    }

    // The battery, as numbers, for anyone watching it from a desk.
    //
    // The serial console is not an option for this one: the cable that carries
    // it is the cable whose plugging and unplugging is the thing being
    // watched. Over the network the measurement survives the event.
    void handleApiBatt() {
        JsonDocument d;
        d["present"]  = battery::present();
        d["mv"]       = battery::millivolts();      // at the pin
        d["volts"]    = battery::volts();           // at the cell
        d["percent"]  = battery::percent();
        d["charging"] = battery::charging();
        d["minutes"]  = battery::minutesLeft();
        d["uptime_s"] = (uint32_t)(millis() / 1000);
        String out; serializeJson(d, out);
        server.send(200, "application/json", out);
    }

    void handleApiScan() {
        // ?all=1: every access point, one row each - BSSID, channel, security -
        // from a fresh scan. The list below is one row per network NAME and is
        // cached, which is right for a picker and useless for the question
        // "which of this network's radios can the device hear, and how well".
        if (server.hasArg("all")) {
            WiFi.scanDelete();
            const int n = WiFi.scanNetworks(false, true);
            JsonDocument d;
            d["connected"]["bssid"] = WiFi.BSSIDstr();
            d["connected"]["rssi"]  = WiFi.RSSI();
            d["connected"]["ch"]    = WiFi.channel();
            JsonArray a = d["aps"].to<JsonArray>();
            for (int i = 0; i < n; i++) {
                JsonObject o = a.add<JsonObject>();
                o["s"]    = WiFi.SSID(i);
                o["b"]    = WiFi.BSSIDstr(i);
                o["r"]    = WiFi.RSSI(i);
                o["ch"]   = WiFi.channel(i);
                o["auth"] = (int)WiFi.encryptionType(i);
            }
            WiFi.scanDelete();
            String out; serializeJson(d, out);
            server.send(200, "application/json", out);
            return;
        }
        harvestScan();

        // A scan already in flight is worth a short wait: the page has just
        // opened and this list is the only thing on it.
        if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
            for (uint32_t t0 = millis(); millis() - t0 < 4000; ) {
                delay(60);
                if (WiFi.scanComplete() != WIFI_SCAN_RUNNING) break;
            }
            harvestScan();
        }

        // Nothing held and nobody has joined yet - still a good moment to look,
        // so look. Once a phone is associated it is not, and what was found
        // before it arrived is the honest answer.
        if (netsJson.isEmpty() && WiFi.softAPgetStationNum() == 0) {
            WiFi.scanDelete();
            int n = WiFi.scanNetworks(false, true);
            if (n > 0) { netsJson = scanToJson(n); WiFi.scanDelete(); }
            else Serial.printf("[webcfg] on-demand scan gave %d\n", n);
        }

        if (netsJson.isEmpty()) {
            Serial.println("[webcfg] no networks held to serve");
            server.send(200, "application/json", "{\"nets\":[],\"error\":true}");
            return;
        }
        server.send(200, "application/json", netsJson);
    }

    // Join, verify, and only then report - no reboot.
    //
    // The prototype saved and restarted, which drops the phone and reopens the
    // portal with no explanation when the password was wrong. Here the access
    // point stays up through the attempt, so a failure is reported while the
    // user is still looking at the field they typed it into.
    //
    // Honest caveat: an ESP32 shares one radio between AP and station, and the
    // access point follows the station's channel. If the home network is on a
    // different channel the phone can drop mid-attempt and never see this
    // response. That is why the device's own screen shows the same result - the
    // page is the nice path, the screen is the one that cannot fail.
    void handleApiJoin() {
        JsonDocument in;
        if (deserializeJson(in, server.arg("plain"))) {
            server.send(400, "application/json", "{\"ok\":false}");
            return;
        }
        String ssid = in["ssid"] | "";
        String pass = in["pass"] | "";
        if (ssid.isEmpty()) { server.send(400, "application/json", "{\"ok\":false}"); return; }

        WiFi.mode(WIFI_AP_STA);
        // Nearest access point, not the first one heard - see staBegin() in
        // main.cpp. Without erasing anything: the phone is on this device's own
        // access point right now, and dropping the radio would drop it.
        WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
        WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
        WiFi.begin(ssid.c_str(), pass.c_str());
        WiFi.setSleep(false);   // see staNoSleep() in main.cpp
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 18000) delay(120);

        JsonDocument out;
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.disconnect(true);
            // Stay AP+STA. Dropping the station interface here left the next
            // scan with nothing to start on, and that is what emptied the
            // network picker.
            out["ok"] = false;
            String j; serializeJson(out, j);
            server.send(200, "application/json", j);
            Serial.printf("[webcfg] join '%s' failed\n", ssid.c_str());
            return;
        }

        Preferences w;
        w.begin("tigerspool", false);
        w.putString("ssid", ssid);
        w.putString("pass", pass);
        w.end();

        out["ok"]   = true;
        out["host"] = String(HOSTNAME) + ".local";
        out["ip"]   = WiFi.localIP().toString();
        // The STATION MAC. It differs from the access point's by one, and a DHCP
        // reservation made against the wrong one silently never fires.
        out["mac"]  = WiFi.macAddress();
        String j; serializeJson(out, j);
        server.send(200, "application/json", j);

        Serial.printf("[webcfg] joined '%s' as %s (%s)\n",
                      ssid.c_str(), out["ip"].as<String>().c_str(), out["mac"].as<String>().c_str());

        // Give the phone a few seconds to render the result before the access
        // point disappears from under it.
        apTeardownAt = millis() + 6000;
    }

    // GET /api/tap?x=&y= - a synthetic touch at a panel coordinate.
    // Add &x2=&y2= and it becomes a drag, which is how a list is scrolled.
    //
    // /screen.bmp made the interface readable from a desk; this makes it
    // navigable. Together they close the loop: open a screen, tap, look at the
    // result, without anyone standing at the bench. It is the difference
    // between "does this look right?" being a question you ask someone and one
    // you answer.
    //
    // LVGL is pumped here until the tap has been read and the screen has
    // settled, so a single request leaves the panel in its new state and the
    // caller can fetch /screen.bmp straight away. Same reasoning as the
    // screenshot handler: this runs in the main loop, so pumping LVGL from it
    // races nothing.
    void handleApiTap() {
        if (!server.hasArg("x") || !server.hasArg("y")) {
            server.send(400, "text/plain", "need x and y");
            return;
        }
        int x = server.arg("x").toInt();
        int y = server.arg("y").toInt();
        if (x < 0 || x >= SCR_W || y < 0 || y >= SCR_H) {
            server.send(400, "text/plain", "outside the panel");
            return;
        }
        if (server.hasArg("x2") && server.hasArg("y2")) {
            lvgl_port::injectSwipe(x, y, server.arg("x2").toInt(),
                                         server.arg("y2").toInt());
        } else {
            lvgl_port::injectTap(x, y);
        }
        for (uint32_t t0 = millis(); millis() - t0 < 400; ) delay(5);
        server.send(200, "text/plain", "ok");
    }

    void handleApiLang() {
        String l = server.hasArg("l") ? server.arg("l") : String();
        for (int i = 0; i < (int)LANG_N; i++)
            if (l == LANG_CODES[i]) { i18n::set((Lang)i); break; }
        server.send(200, "application/json", "{\"ok\":true}");
    }

    struct Row { int type; String name, host, sn, cc; };
    // GET /login - the page the sign-in QR points at.
    //
    // It used to point at "/", which serves the configuration page. That page
    // opens with the Wi-Fi picker and puts the account form some forty lines
    // below it, so a phone scanning the QR landed on a network selector and had
    // to scroll to find what it came for. Worse, the first thing under the
    // finger was a "save and restart" that could take the device off the
    // network it had just joined.
    //
    // So the QR gets a page with one job on it. The POST target is the same
    // handler as before; only the wrapper is new.
    // The sign-in page: the one screen of this product a stranger reaches by
    // scanning a QR code, on a 192.168.x.x URL their browser decorates with a
    // warning triangle, and it is where they are asked for a password. So it
    // carries the mark, and nothing that is not needed to sign in.
    //
    // NOTHING here may be fetched from the internet. The phone reading this is
    // joined to the device's own access point when this matters most, and a
    // missing web font or a remote logo fails silently - an empty box on the
    // one screen where trust is being decided.
    // The shell both account pages wear. Extracted rather than copied: they
    // are two steps of one flow, and a phone that changes typeface and ground
    // halfway through looks like it has handed you to somewhere else - on the
    // step where the question is precisely who you are talking to.
    //
    // NOTHING here may be fetched from the internet. The phone reading this is
    // joined to the device's own access point when this matters most, and a
    // missing web font or a remote logo fails silently.
    const char GOOGLE_G[] PROGMEM =
        "<svg viewBox=\"0 0 48 48\" aria-hidden=true>"
        "<path fill=#EA4335 d=\"M24 9.5c3.54 0 6.71 1.22 9.21 3.6l6.85-6.85C35.9 2.38 30.47 0 24 0 14.62 0 6.51 5.38 2.56 13.22l7.98 6.19C12.43 13.72 17.74 9.5 24 9.5z\"/>"
        "<path fill=#4285F4 d=\"M46.98 24.55c0-1.57-.15-3.09-.38-4.55H24v9.02h12.94c-.58 2.96-2.26 5.48-4.78 7.18l7.73 6c4.51-4.18 7.09-10.36 7.09-17.65z\"/>"
        "<path fill=#FBBC05 d=\"M10.53 28.59c-.48-1.45-.76-2.99-.76-4.59s.27-3.14.76-4.59l-7.98-6.19C.92 16.46 0 20.12 0 24c0 3.88.92 7.54 2.56 10.78l7.97-6.19z\"/>"
        "<path fill=#34A853 d=\"M24 48c6.48 0 11.93-2.13 15.89-5.81l-7.73-6c-2.15 1.45-4.92 2.3-8.16 2.3-6.26 0-11.57-4.22-13.47-9.91l-7.98 6.19C6.51 42.62 14.62 48 24 48z\"/>"
        "</svg>";

    const char STUDIO_LINK[] PROGMEM =
        "<a class=studio href=https://tigersystem.io>"
        "<img src=/tiger-icon.svg alt=\"\">Tiger Studio Manager"
        "<svg viewBox=\"0 0 24 24\" aria-hidden=true>"
        "<path d=\"M9 5h10v10\"/><path d=\"M19 5 8 16\"/><path d=\"M15 19H5V9\"/></svg></a>";

    const char SOCIAL_ROW[] PROGMEM =
        "<div class=soc>"
        "<a href=https://github.com/TigerTag-Project aria-label=GitHub>"
        "<svg viewBox=\"0 0 24 24\" aria-hidden=true><path d=\"M12 .5C5.37.5 0 5.87 0 12.5c0 5.3 3.44 9.8 8.21 11.39.6.11.82-.26.82-.58 0-.29-.01-1.24-.02-2.25-3.34.73-4.04-1.42-4.04-1.42-.55-1.38-1.33-1.75-1.33-1.75-1.09-.75.08-.73.08-.73 1.2.08 1.84 1.24 1.84 1.24 1.07 1.83 2.81 1.3 3.5.99.11-.78.42-1.3.76-1.6-2.67-.3-5.47-1.34-5.47-5.95 0-1.31.47-2.38 1.24-3.22-.12-.31-.54-1.53.12-3.18 0 0 1.01-.32 3.3 1.23a11.5 11.5 0 0 1 6.01 0c2.29-1.55 3.3-1.23 3.3-1.23.66 1.65.24 2.87.12 3.18.77.84 1.24 1.91 1.24 3.22 0 4.62-2.81 5.64-5.49 5.94.43.37.81 1.1.81 2.22 0 1.6-.01 2.9-.01 3.29 0 .32.22.7.83.58C20.56 22.29 24 17.79 24 12.5 24 5.87 18.63.5 12 .5Z\"/></svg></a>"
        "<a href=https://discord.gg/3Qv5TSqnJH aria-label=Discord>"
        "<svg viewBox=\"0 0 24 24\" aria-hidden=true><path d=\"M20.317 4.369a19.79 19.79 0 0 0-4.885-1.515.074.074 0 0 0-.079.037c-.21.375-.444.864-.608 1.249a18.27 18.27 0 0 0-5.487 0 12.6 12.6 0 0 0-.617-1.25.077.077 0 0 0-.079-.036A19.736 19.736 0 0 0 3.677 4.37a.07.07 0 0 0-.032.027C.533 9.046-.32 13.58.099 18.058a.082.082 0 0 0 .031.057 19.9 19.9 0 0 0 5.993 3.03.078.078 0 0 0 .084-.028 14.09 14.09 0 0 0 1.226-1.994.076.076 0 0 0-.041-.106 13.107 13.107 0 0 1-1.872-.892.077.077 0 0 1-.008-.128c.126-.094.252-.192.372-.291a.074.074 0 0 1 .077-.01c3.928 1.793 8.18 1.793 12.062 0a.074.074 0 0 1 .078.009c.12.099.246.198.373.292a.077.077 0 0 1-.006.127 12.3 12.3 0 0 1-1.873.891.077.077 0 0 0-.041.107c.36.698.772 1.362 1.225 1.993a.076.076 0 0 0 .084.028 19.839 19.839 0 0 0 6.002-3.03.077.077 0 0 0 .032-.056c.5-5.177-.838-9.674-3.549-13.66a.061.061 0 0 0-.031-.03ZM8.02 15.331c-1.183 0-2.157-1.085-2.157-2.419 0-1.333.955-2.418 2.157-2.418 1.21 0 2.176 1.094 2.157 2.418 0 1.334-.956 2.419-2.157 2.419Zm7.975 0c-1.183 0-2.157-1.085-2.157-2.419 0-1.333.955-2.418 2.157-2.418 1.21 0 2.176 1.094 2.157 2.418 0 1.334-.947 2.419-2.157 2.419Z\"/></svg></a>"
        "<a href=https://tigersystem.io aria-label=TigerSystem>"
        "<img src=/tiger-icon.svg alt=\"\"></a></div>";

    void pageOpen(String& h, const char* extraHead = nullptr) {
        h += F("<!doctype html><html><head><meta charset=utf-8>"
               "<meta name=viewport content=\"width=device-width,initial-scale=1,viewport-fit=cover\">"
               "<title>TigerSpool</title>");
        if (extraHead) h += extraHead;     // e.g. the poll page's meta refresh
        h += F("<style>"
               ":root{--bg:#08090d;--raised:#171a22;--line:#262a36;--soft:#1c2029;"
               "--text:#f4f5f8;--muted:#9aa0b0;--faint:#5f6674;--brand:#ff7a18;--ember:#e6352b}"
               "*{box-sizing:border-box}"
               "body{font-family:system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;"
               "background:var(--bg);color:var(--text);margin:0;"
               "padding:26px 18px calc(26px + env(safe-area-inset-bottom));"
               "display:flex;justify-content:center}"
               ".card{width:100%;max-width:380px}"
               ".brand{display:flex;align-items:center;gap:11px;margin:6px 0 30px}"
               ".brand img{width:34px;height:34px;border-radius:9px;display:block}"
               ".brand b{font-size:16.5px;font-weight:700;letter-spacing:-.01em}"
               ".brand b i{color:var(--brand);font-style:normal}"
               "label{display:block;margin:0 0 6px;font-size:12px;color:var(--muted)}"
               ".fg{margin-bottom:14px}"
               ".pw{position:relative}"
               // 16px on the inputs is not a style choice: below that, iOS
               // Safari zooms the page on focus and the layout jumps under the
               // thumb mid-word.
               "input{width:100%;height:46px;padding:0 13px;border-radius:12px;"
               "border:1px solid var(--line);background:var(--raised);color:var(--text);"
               "font-size:16px;font-family:inherit}"
               "input:focus{outline:0;border-color:var(--brand);"
               "box-shadow:0 0 0 3px rgba(255,122,24,.16)}"
               ".pw input{padding-right:46px}"
               ".eye{position:absolute;right:4px;top:0;height:46px;width:42px;"
               "border:0;background:0;color:var(--faint);display:grid;place-items:center;"
               "padding:0;cursor:pointer}"
               ".eye svg{width:19px;height:19px;fill:none;stroke:currentColor;"
               "stroke-width:1.7;stroke-linecap:round;stroke-linejoin:round}"
               "button.go,.g,a.go{width:100%;height:50px;border:0;border-radius:14px;"
               "font-size:15px;font-family:inherit;display:flex;align-items:center;"
               "justify-content:center;gap:11px;text-decoration:none}"
               "button.go,a.go{margin-top:20px;font-weight:700;color:#fff;"
               "background:linear-gradient(96deg,var(--brand),var(--ember))}"
               ".g{background:#fff;color:#1f1f1f;font-weight:600}"
               // Both buttons carry the Google mark, so both have to cap it.
               // Sized only on .g, the same glyph filled the orange button
               // edge to edge on the pairing page - an SVG with a viewBox and
               // no width takes whatever the box will give it.
               ".g svg,a.go svg{width:19px;height:19px;flex:none}"
               ".sep{display:flex;align-items:center;gap:12px;margin:18px 0;"
               "color:var(--faint);font-size:12px}"
               ".sep:before,.sep:after{content:'';flex:1;height:1px;background:var(--soft)}"
               ".lead{margin:0 0 4px;font-size:15px;line-height:1.45}"
               ".pl{list-style:none;margin:0;padding:0;border:1px solid var(--line);"
               "border-radius:12px;background:var(--raised)}"
               ".pl li{padding:11px 13px;border-top:1px solid var(--soft);display:flex;"
               "align-items:center;gap:12px}"
               ".pl li>div{flex:1;min-width:0}"
               ".pl b{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
               ".sw{position:relative;flex:none;width:44px;height:26px}"
               ".sw input{position:absolute;opacity:0;width:100%;height:100%;margin:0;cursor:pointer}"
               ".sw i{position:absolute;inset:0;border-radius:13px;background:var(--line);"
               "pointer-events:none;transition:background .15s}"
               ".sw i:after{content:'';position:absolute;left:3px;top:3px;width:20px;height:20px;"
               "border-radius:50%;background:#fff;transition:transform .15s}"
               ".sw input:checked+i{background:var(--brand)}"
               ".sw input:checked+i:after{transform:translateX(18px)}"
               ".sw input:disabled+i{opacity:.5}"
               "button.studio{width:100%;font-family:inherit;cursor:pointer;text-align:left}"
               ".who{display:flex;align-items:center;gap:14px;margin:4px 0 6px}"
               ".who>div{min-width:0}"
               ".who b{display:block;font-size:17px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
               ".who>div>span{display:block;font-size:13.5px;color:var(--muted);overflow:hidden;"
               "text-overflow:ellipsis;white-space:nowrap}"
               ".av{position:relative;flex:none;width:52px;height:52px;border-radius:50%;"
               "display:grid;place-items:center;overflow:hidden;font-size:21px;font-weight:700;"
               "color:#fff;background:linear-gradient(135deg,var(--brand),var(--ember))}"
               ".av img{position:absolute;inset:0;width:100%;height:100%;object-fit:cover}"
               ".off-btn{display:flex;align-items:center;justify-content:center;height:46px;"
               "margin-top:22px;border:1px solid #5a2328;border-radius:14px;color:#ff8a80;"
               "font-size:14.5px;font-weight:600;text-decoration:none}"
               ".studio .n{margin-left:auto;color:var(--muted);font-variant-numeric:tabular-nums}"
               "#op svg{margin-left:6px}"
               ".bd{position:fixed;inset:0;background:rgba(0,0,0,.55);opacity:0;"
               "pointer-events:none;transition:opacity .2s}"
               ".bd.open{opacity:1;pointer-events:auto}"
               ".drawer{position:fixed;top:0;right:0;bottom:0;width:min(400px,100%);overflow-y:auto;"
               "padding:18px 16px calc(18px + env(safe-area-inset-bottom));background:var(--bg);"
               "border-left:1px solid var(--line);transform:translateX(100%);"
               "transition:transform .25s ease,visibility .25s;visibility:hidden}"
               ".drawer.open{transform:none;visibility:visible}"
               ".dh{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px}"
               ".dh b{font-size:17px}"
               ".drawer button.go{margin-top:0}"
               ".drawer .st{margin:8px 0 10px}"
               ".x{width:40px;height:40px;border:0;border-radius:10px;background:var(--raised);"
               "color:var(--text);font-size:24px;line-height:1;cursor:pointer}"
               ".pl li:first-child{border-top:0}"
               ".pl b{display:block;font-size:15px;font-weight:600}"
               ".pl span{font-size:12.5px;color:var(--muted)}"
               ".pl .off>div{opacity:.45}"
               ".st{min-height:18px;margin:10px 0 0;font-size:13px;color:var(--muted)}"
               "button.go:disabled{opacity:.55}"
               ".codelabel{margin:16px 0 0;text-align:center;font-size:12px;"
               "color:var(--muted)}"
               ".code{margin:8px 0 0;padding:14px;border:1px solid var(--line);"
               "border-radius:12px;background:var(--raised);text-align:center;"
               "font-size:23px;font-weight:700;letter-spacing:.16em;"
               "font-variant-numeric:tabular-nums}"
               ".wait{display:flex;align-items:center;justify-content:center;gap:10px;"
               "margin-top:20px;color:var(--muted);font-size:13.5px}"
               ".sp{width:15px;height:15px;border-radius:50%;flex:none;"
               "border:2px solid rgba(255,255,255,.18);border-top-color:var(--brand);"
               "animation:t .9s linear infinite}"
               "@keyframes t{to{transform:rotate(360deg)}}"
               "@media(prefers-reduced-motion:reduce){.sp{animation:none}}"
               ".foot{margin-top:24px;padding-top:16px;border-top:1px solid var(--soft);"
               "color:var(--faint);font-size:12.5px;line-height:1.5}"
               ".studio{display:flex;align-items:center;gap:10px;margin-top:12px;"
               "padding:11px 13px;border:1px solid var(--line);border-radius:12px;"
               "background:var(--raised);color:var(--text);text-decoration:none;"
               "font-size:13.5px;font-weight:500}"
               ".studio img{width:22px;height:22px;border-radius:6px;flex:none}"
               ".studio svg{width:15px;height:15px;margin-left:auto;flex:none;"
               "fill:none;stroke:var(--faint);stroke-width:1.8;stroke-linecap:round;"
               "stroke-linejoin:round}"
               ".soc{display:flex;justify-content:center;gap:26px;margin-top:22px}"
               ".soc a{display:block;color:#4d5462}"
               ".soc svg{width:19px;height:19px;display:block;fill:currentColor}"
               ".soc img{width:19px;height:19px;display:block;border-radius:5px;opacity:.55}"
               ".legal{margin:14px 0 0;text-align:center;font-size:11px;color:#3f4653}"
               "</style></head><body><div class=card>"
               "<div class=brand><img src=/tiger-icon.svg alt=\"\">"
               "<b>Tiger<i>Spool</i></b></div>");
    }

    // The version is read from the macro, never typed. It is also the half of
    // this line that gets used: the first thing asked for when a problem is
    // reported, and legible from a phone while the device's own screen is in
    // another room.
    void pageClose(String& h) {
        h += F("<p class=legal>TigerSpool RFID ");
        h += TIGERSPOOL_FW_VERSION;
        h += F(" &middot; MIT &middot; &copy; TigerTag</p></div></body></html>");
    }

    // Signed in, the page the QR opens is where the account is managed: it used
    // to be a bare status line that refreshed onto itself, and with the legacy
    // configuration page gone nothing anywhere led to a sign-out or a sync.
    // Links, not forms, for the reason given at the Google button below; the
    // sign-out asks first, because it restarts the device.
    //
    // The printers are a drawer over the page, opened from the account card and
    // put away again - the same on/off switches the device's own list has.
    // Nothing on this page navigates: a sync and a switch both answer in place.
    void handleAccount() {
        String h; h.reserve(6600);
        pageOpen(h);
        // Who is signed in, as the account shows them: picture, name, address.
        // No picture (most email accounts) is an initial; a picture that will
        // not load - the phone is offline, the URL has expired - falls back to
        // the same initial rather than a broken-image icon.
        const String name = ttcloud::displayName(), mail = ttcloud::email();
        const String photo = ttcloud::photoUrl();
        String ini = name.length() ? name.substring(0, 1) : String("?");
        ini.toUpperCase();
        h += F("<div class=who><span class=av>");
        if (photo.startsWith("https://")) {
            h += F("<img src=\""); h += esc(photo);
            h += F("\" alt=\"\" referrerpolicy=no-referrer onerror=\"this.remove()\">");
        }
        h += esc(ini); h += F("</span><div><b>"); h += esc(name); h += F("</b>");
        if (name != mail) { h += F("<span>"); h += esc(mail); h += F("</span>"); }
        h += F("</div></div>");
        h += F("<button type=button class=studio id=op>"); h += wl(W_PRINTERS);
        h += F("<span class=n id=cnt></span><svg viewBox=\"0 0 24 24\" aria-hidden=true>"
               "<path d=\"M9 5l7 7-7 7\"/></svg></button>");
        h += STUDIO_LINK;
        h += F("<a class=off-btn href=/tt-forget onclick=\"return confirm('");
        h += wl(W_FORGET_ASK); h += F("')\">"); h += wl(W_TT_FORGET); h += F("</a>");
        h += F("<div class=bd id=bd></div><aside class=drawer id=dr aria-hidden=true>"
               "<div class=dh><b>"); h += wl(W_PRINTERS);
        h += F("</b><button type=button class=x id=cl aria-label=Close>&times;</button></div>"
               "<button class=go id=sy type=button>");
        h += wl(W_SYNC_NOW); h += F("</button><p class=st id=st></p>"
               "<ul class=pl id=pl></ul></aside>");
        pageClose(h);
        // The list is drawn from /api/account, on load and again when a sync
        // this page asked for has finished AND been applied - the count moves
        // when the task ends, the list only once main.cpp has reloaded it.
        //
        // A switch moves at once and asks afterwards. The web server is served
        // from the main loop, which can be inside a printer handshake for
        // seconds; a switch that waited for it felt dead. main.cpp decides in
        // the same loop pass that takes the request, so the read-back right
        // after it is the verdict: a refusal (the load budget) puts it back.
        h += F("<script>var B=['','Creality','FlashForge','Bambu Lab','Snapmaker','Elegoo','Anycubic'],"
               "$=function(i){return document.getElementById(i)},"
               "L=$('pl'),S=$('st'),Y=$('sy'),D=$('dr'),K=$('bd'),C=$('cnt'),NONE='"); h += wl(W_NONE);
        h += F("',BUSY='"); h += wl(W_SYNC_STARTED);
        h += F("',LATE='"); h += wl(W_SYNC_TIMEOUT);
        h += F("',ROOM='"); h += wl(W_NO_ROOM);
        h += F("';function e(s){var d=document.createElement('i');d.textContent=s;return d.innerHTML}"
               "function get(){return fetch('/api/account',{cache:'no-store'}).then(function(r){return r.json()})}"
               "function count(){var b=L.querySelectorAll('input');C.textContent=b.length?"
               "L.querySelectorAll('input:checked').length+'/'+b.length:''}"
               "function draw(a){var o='';a.printers.forEach(function(q){o+='<li'+(q.v?'':' class=off')+'><div><b>'"
               "+e(q.n)+'</b><span>'+(B[q.t]||'?')+' · '+(q.c?'cloud':e(q.h||'-'))+'</span></div>'"
               "+'<label class=sw><input type=checkbox data-i='+q.i+(q.v?' checked':'')+'><i></i></label></li>'});"
               "L.innerHTML=o||'<li><span>'+NONE+'</span></li>';count()}"
               "function show(c,on){c.checked=on;c.closest('li').className=on?'':'off';count()}"
               "L.onchange=function(ev){var c=ev.target,i=c.dataset.i,on=c.checked;S.textContent='';show(c,on);"
               "fetch('/api/printer?i='+i+'&on='+(on?1:0)).then(get).then(function(a){"
               "var q=a.printers.filter(function(p){return p.i==i})[0];"
               "if(q&&q.v!=on&&c.checked==on){show(c,q.v);S.textContent=ROOM}})"
               ".catch(function(){show(c,!on)})};"
               "function open_(o){D.classList.toggle('open',o);K.classList.toggle('open',o);"
               "D.setAttribute('aria-hidden',!o)}"
               "$('op').onclick=function(){open_(true)};$('cl').onclick=K.onclick=function(){open_(false)};"
               "document.onkeydown=function(ev){if(ev.key=='Escape')open_(false)};"
               "function done(m){S.textContent=m;Y.disabled=false}"
               "function poll(n,t0){get().then(function(a){"
               "if(a.n!=n&&!a.pend&&!a.busy){draw(a);done(a.result)}"
               "else if(Date.now()-t0>90000){draw(a);done(LATE)}"
               "else setTimeout(function(){poll(n,t0)},1000)})"
               ".catch(function(){setTimeout(function(){poll(n,t0)},1500)})}"
               "Y.onclick=function(){Y.disabled=true;S.textContent=BUSY;"
               "get().then(function(a){return fetch('/tt-sync').then(function(){poll(a.n,Date.now())})})"
               ".catch(function(){done(LATE)})};"
               "get().then(draw);</script>");
        server.send(200, "text/html", h);
    }

    // What the account page draws. Names, brands and addresses only: serials
    // and access codes stay on the device, and this answers anyone on the LAN.
    void handleApiAccount() {
        JsonDocument d;
        d["busy"]   = ttcloud::asyncBusy();
        d["pend"]   = ttcloud::changePending();
        d["n"]      = ttcloud::syncCount();
        d["result"] = ttcloud::lastResult();
        JsonArray a = d["printers"].to<JsonArray>();
        for (int i = 0; i < MAX_PRINTERS; i++) {
            const PrinterCfg& p = printers[i];
            if (p.type == PT_NONE) continue;
            JsonObject o = a.add<JsonObject>();
            o["i"] = i;
            o["n"] = p.name; o["t"] = (int)p.type; o["h"] = p.host;
            o["c"] = p.cloud; o["v"] = p.visible;
        }
        String out; serializeJson(d, out);
        server.sendHeader("Cache-Control", "no-store");
        server.send(200, "application/json", out);
    }

    // Switches on the account page. Only recorded here: main.cpp owns printers[]
    // and applies them through the same budget check as the device's own switch.
    // A mask, not one slot: two switches flipped quickly land in one loop pass.
    static_assert(MAX_PRINTERS <= 32, "the switch masks are 32 bits");
    uint32_t g_switchWant = 0, g_switchOn = 0;
    void handleApiPrinter() {
        const int i = server.arg("i").toInt();
        if (!server.hasArg("i") || i < 0 || i >= MAX_PRINTERS || printers[i].type == PT_NONE) {
            server.send(400, "text/plain", "bad index"); return;
        }
        g_switchWant |= 1u << i;
        if (server.arg("on") == "1") g_switchOn |= 1u << i; else g_switchOn &= ~(1u << i);
        server.send(202, "text/plain", "ok");
    }

    void handleLogin() {
        if (ttcloud::haveSession()) { handleAccount(); return; }

        String h; h.reserve(7400);
        pageOpen(h);

        h += F("<form method=POST action=/tt-login>"
               "<div class=fg><label for=m>"); h += wl(W_EMAIL); h += F("</label>"
               "<input id=m name=ttmail type=email autocomplete=username "
               "inputmode=email autocapitalize=off autocorrect=off></div>"
               "<div class=fg><label for=p>"); h += wl(W_PASS); h += F("</label>"
               "<div class=pw><input id=p name=ttpass type=password "
               "autocomplete=current-password>"
               "<button type=button class=eye id=e aria-label=\""); h += wl(W_SHOW_PW);
        h += F("\"><svg viewBox=\"0 0 24 24\" aria-hidden=true>"
               "<path d=\"M1.8 12S5.5 5.2 12 5.2 22.2 12 22.2 12 18.5 18.8 12 18.8 1.8 12 1.8 12Z\"/>"
               "<circle cx=12 cy=12 r=3.1 /></svg></button></div></div>"
               "<button class=go type=submit>"); h += wl(W_TT_LOGIN); h += F("</button></form>");

        h += F("<div class=sep>"); h += wl(W_OR); h += F("</div>");

        // A link, not a form. The Google path submits nothing - it only asks
        // the device to start a pairing - and Safari warns on ANY form posted
        // over plain HTTP, including one carrying no data. That warning on
        // this button was frightening people away from the one route that
        // never asks them to type a password over the clear.
        h += F("<a class=g href=/tt-gstart>"); h += GOOGLE_G; h += wl(W_GOOGLE);
        h += F("</a>");

        h += F("<p class=foot>"); h += wl(W_NO_ACCOUNT); h += F("</p>");
        h += STUDIO_LINK;
        h += SOCIAL_ROW;
        pageClose(h);

        h += F("<script>var e=document.getElementById('e'),p=document.getElementById('p');"
               "e.onclick=function(){p.type=p.type=='password'?'text':'password'};"
               "</script>");
        server.send(200, "text/html", h);
    }

    void handleIcon() {
        server.sendHeader("Cache-Control", "max-age=86400");
        server.send_P(200, "image/svg+xml", TIGER_ICON_SVG);
    }

    void reply(const String& title, const String& msg) {
        String h = F("<!doctype html><meta charset=utf-8><meta http-equiv=refresh content=\"5;url=/\">"
                     "<body style='font-family:system-ui;background:#111;color:#eee;padding:24px'><h2>");
        h += esc(title); h += F("</h2><p>"); h += esc(msg); h += F("</p></body>");
        server.send(200, "text/html", h);
    }

    // Signing in does NOT restart the device.
    //
    // It used to, and it was the difference a user reported between the two
    // routes: sign in with Google and the device carries on, sign in with an
    // email and it reboots. Nothing about an email sign-in needs a reboot -
    // the session is in NVS, the printer list has just been fetched, and the
    // state machine notices both. The restart was there because the loop only
    // reloaded the list when IT had asked for the sync; a sync done from the
    // web page landed in NVS with nobody reading it back. That is fixed where
    // it belongs, in main.cpp, rather than by restarting a working device in
    // front of somebody who has just typed their password.
    void handleTtLogin() {
        String mail = server.arg("ttmail"); mail.trim();
        String pass = server.arg("ttpass");
        String err;
        if (!ttcloud::signIn(mail, pass, err)) { reply(wl(W_LOGIN_FAIL), err); return; }
        ttcloud::requestSync();
        reply(wl(W_ACCT_LINKED), wl(W_SYNC_STARTED));
    }
    // Neither handler syncs itself. Both run on the loop task: a sync there
    // nested a TLS handshake inside the web handler and overflowed the loop's
    // 8 KB stack the moment an email sign-in succeeded, and even when it fit,
    // it held every screen still for the ten seconds the sync takes.
    void handleTtSync() {
        ttcloud::requestSync();
        reply(wl(W_TT_ACCOUNT), wl(W_SYNC_STARTED));
    }
    void handleTtForget() {
        ttcloud::forget();
        reply(wl(W_ACCT_OFF), wl(W_RESTARTING));
        restartAt = millis() + 1200;
    }

    // --- Google sign-in (link-based pairing flow) ---
    String   g_pairTok, g_pairUrl, g_pairCode;
    int      g_pairIv = 5;
    uint32_t g_pairSince = 0;
    uint32_t g_pairPolledAt = 0;
    // The pairing code's own lifetime, so the device's screen counts down to
    // the same moment the server stops accepting it rather than to a number
    // this file invented.
    const uint32_t PAIR_WINDOW_S = 300;

    // waiting page: shows the link and code, and reloads via /tt-gpoll
    // Waiting for Google approval.
    //
    // The button is the whole instruction. The old page said "open this link
    // on a phone or PC", which guesses at something it cannot know: this page
    // is reached from a phone that scanned the QR, and equally from a PC where
    // someone typed the address off the device's screen. Either way the button
    // opens where it is pressed, so naming a device only risked being wrong.
    //
    // Below it, the code alone - no sentence around it. The device shows the
    // same pairing as a QR on its own screen for as long as this page waits,
    // so the two surfaces agree instead of offering two different pairings.
    void pairWaitPage(const String& extra) {
        String h; h.reserve(5200);
        // The refresh goes INSIDE the one head pageOpen writes. Emitted before
        // it, this page shipped two <!doctype html> and two <head> - which
        // browsers forgive and nothing else does.
        String refresh = String(F("<meta http-equiv=refresh content=\"")) +
                         g_pairIv + F(";url=/tt-gpoll\">");
        pageOpen(h, refresh.c_str());

        // The QR on the device is the primary path, and it is the one that
        // works from anywhere: a phone reading this page scans the box in
        // front of it, and so does someone at a desktop who typed the address
        // off that same screen. The button below is the shortcut for a browser
        // that is already signed in to Google - an alternative, not the
        // instruction.
        h += F("<p class=lead>"); h += wl(W_PAIR_SCAN); h += F("</p>");

        if (g_pairCode.length()) {
            h += F("<p class=codelabel>"); h += wl(W_PAIR_CODE);
            h += F("</p><p class=code>"); h += esc(g_pairCode); h += F("</p>");
        }

        h += F("<div class=sep>"); h += wl(W_OR); h += F("</div>");

        h += F("<a class=g target=_blank rel=noopener href=\""); h += esc(g_pairUrl);
        h += F("\">"); h += GOOGLE_G; h += wl(W_GOOGLE); h += F("</a>");

        h += F("<div class=wait><span class=sp></span>"); h += wl(W_PAIR_WAIT);
        h += F("</div>");
        if (extra.length()) { h += F("<p class=foot>"); h += esc(extra); h += F("</p>"); }

        pageClose(h);
        server.send(200, "text/html", h);
    }

    void handleTtGStart() {
        String err;
        if (!ttcloud::pairStart(g_pairCode, g_pairUrl, g_pairTok, g_pairIv, err)) {
            reply(wl(W_FAILED), err); return;
        }
        g_pairSince = millis();
        if (g_pairIv < 3) g_pairIv = 3;
        pairWaitPage("");
    }

    // True while a pairing started from the web page is still waiting, so the
    // device can put the same QR on its own screen. main owns the state; this
    // only reports.
    bool webPairing_(String& url, String& code, int& secondsLeft) {
        if (g_pairTok.isEmpty()) return false;
        url = g_pairUrl; code = g_pairCode;
        uint32_t up = (millis() - g_pairSince) / 1000;
        secondsLeft = (up >= PAIR_WINDOW_S) ? 0 : (int)(PAIR_WINDOW_S - up);
        return true;
    }

    // The device asks Google whether the pairing was approved, on its own,
    // instead of waiting for the phone's browser to refresh.
    //
    // The page carries a meta refresh, and that was the ONLY thing driving the
    // poll. iOS suspends timers in a background tab - and the Google approval
    // opens in another tab by construction - so the approval sat there
    // unnoticed until the user thought to switch back. Measured at two minutes
    // on a real phone for an approval that had already happened.
    //
    // Nothing about the pairing needs a browser. The device has the token.
    void pairTick_() {
        if (g_pairTok.isEmpty()) return;
        uint32_t every = (uint32_t)(g_pairIv < 3 ? 3 : g_pairIv) * 1000;
        if (g_pairPolledAt && millis() - g_pairPolledAt < every) return;
        g_pairPolledAt = millis();

        String ct, em, err;
        int st = ttcloud::pairPoll(g_pairTok, ct, em, err);
        if (st == 1) {
            g_pairTok = "";
            if (ttcloud::signInWithCustomToken(ct, em, err)) {
                ttcloud::requestSync();
                Serial.printf("[account] paired: %s\n", em.c_str());
            } else {
                Serial.printf("[account] pairing sign-in failed: %s\n", err.c_str());
            }
        } else if (st == 2 || st == 3) {
            g_pairTok = "";
            Serial.printf("[account] pairing %s\n", st == 2 ? "denied" : "expired");
        }
    }

    void handleTtGPoll() {
        if (g_pairTok.isEmpty()) { server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); return; }
        String ct, em, err;
        int st = ttcloud::pairPoll(g_pairTok, ct, em, err);
        if (st == 1) {
            g_pairTok = "";
            if (!ttcloud::signInWithCustomToken(ct, em, err)) { reply(wl(W_LOGIN_FAIL), err); return; }
            ttcloud::requestSync();
            reply(wl(W_ACCT_LINKED), wl(W_SYNC_STARTED));
        } else if (st == 2) {
            g_pairTok = ""; reply(wl(W_PAIR_DENIED), wl(W_RESTARTING)); restartAt = millis() + 1500;
        } else if (st == 3) {
            g_pairTok = ""; reply(wl(W_PAIR_EXPIRED), wl(W_RESTARTING)); restartAt = millis() + 1500;
        } else {
            pairWaitPage(st < 0 ? err : String());   // 0 = pending, <0 = transient failure
        }
    }

    void handleCaptive() {
        // Absolute in AP mode, because a captive-portal probe is asking for
        // somewhere to go and a relative redirect answers a different host.
        // Relative on the local network, where the device's address is not
        // 192.168.4.1 and sending anyone there would be a dead end.
        server.sendHeader("Location", apMode ? "http://192.168.4.1/" : "/", true);
        server.send(302, "text/plain", "");
    }

    // Registered ONCE, and every mode-dependent route decides at request time.
    //
    // The incident: a device that had joined Wi-Fi and later dropped to the
    // setup access point served the prototype's old configuration form to the
    // captive portal instead of the portal page. routes() ran twice - once
    // from begin(), once from beginAP() - and the ESP32 web server keeps its
    // handlers in a list where the FIRST match wins. The second registration
    // of "/" was therefore dead, and "/" still pointed at whatever the device
    // was doing when it first came up.
    //
    // Deciding inside the handler cannot go stale, and registering once means
    // the order the two starts happen in stops mattering at all.
    bool routesDone = false;

    void routes() {
        if (routesDone) return;
        routesDone = true;

        // In AP mode the root IS the setup portal. The legacy form stays on
        // the local network, where printers and the account are configured.
        // In AP mode the root IS the setup portal. On the local network the
        // account page is the only thing left worth landing on: the
        // prototype's configuration form is gone, and the printers it used to
        // edit come from the account now.
        server.on("/", []() {
            if (apMode) { handlePortal(); return; }
            server.sendHeader("Location", "/login", true);
            server.send(302, "text/plain", "");
        });
        server.on("/api/scan", handleApiScan);
        server.on("/api/batt", handleApiBatt);
        server.on("/api/join", HTTP_POST, handleApiJoin);
        server.on("/api/lang", handleApiLang);
        server.on("/api/tap",  handleApiTap);
        server.on("/api/account", handleApiAccount);
        server.on("/api/printer", handleApiPrinter);
        // Diagnostic: measure what each printer connection costs. The
        // report goes to the serial console; this only starts it.
        server.on("/api/memtest", []() {
            g_memtestAll = server.hasArg("all");
            g_memtestRequested = true;
            server.send(200, "text/plain", "memtest started - see the serial console");
        });
        // Diagnostic: what a spool would be sent as, without a spool and
        // without sending anything. Builds a chip from the query - protocol,
        // product, uid (hex), material (id), nozmin, nozmax - asks the product
        // endpoint the way a scan does, and answers with the resolution as it
        // stands at that moment. Ask again after a few seconds to see the
        // endpoint's answer used.
        server.on("/api/resolve", []() {
            TagInfo t;
            t.protocol   = server.hasArg("protocol")
                         ? (uint32_t)strtoul(server.arg("protocol").c_str(), nullptr, 10)
                         : filament::PROTOCOL_TIGERTAG_PLUS;
            t.idProduct  = (uint32_t)strtoul(server.arg("product").c_str(), nullptr, 10);
            t.uid        = server.arg("uid");
            t.idMaterial = (uint16_t)server.arg("material").toInt();
            t.nozMin     = (uint16_t)server.arg("nozmin").toInt();
            t.nozMax     = (uint16_t)server.arg("nozmax").toInt();
            const char* label = tt_db::material(t.idMaterial);
            t.material   = label ? String(label) : (String("MAT#") + t.idMaterial);
            const bool started = product_api::request(t);
            const filament::ResolvedFilament f = product_api::resolveFor(t);
            JsonDocument d;
            d["requested"] = started;
            d["waiting"]   = product_api::waiting(t.idProduct);
            d["type"]      = f.materialType; d["typeSrc"]    = filament::sourceName(f.typeSrc);
            d["minTemp"]   = f.nozMin;      d["tempSrc"]     = filament::sourceName(f.tempSrc);
            d["rfid"]      = f.crealityId;  d["rfidSrc"]     = filament::sourceName(f.idSrc);
            d["bambuId"]   = f.bambuId;     d["bambuSrc"]    = f.bambuId[0] ? filament::sourceName(f.bambuSrc) : "none";
            d["pressure"]  = f.pressure;    d["pressureSrc"] = filament::sourceName(f.pressureSrc);
            d["name"]      = f.crealityName; d["nameSrc"]    = filament::sourceName(f.nameSrc);
            d["maxTemp"]   = f.nozMax;
            String out; serializeJson(d, out);
            server.send(200, "application/json", out);
        });
        server.on("/login",    handleLogin);
        server.on("/tiger-icon.svg", handleIcon);
        server.on("/screen.bmp", handleShot);      // raw panel capture
        server.on("/screen", handleShotPage);      // page that refreshes it
        server.on("/screen.ver", []() {            // four bytes, see handleShotPage
            server.sendHeader("Cache-Control", "no-store");
            server.send(200, "text/plain", String(lvgl_port::frameCounter()));
        });
        server.on("/tt-login", HTTP_POST, handleTtLogin);
        server.on("/tt-gstart", handleTtGStart);   // GET: submits nothing, see handleLogin
        server.on("/tt-gpoll", handleTtGPoll);
        server.on("/tt-sync", handleTtSync);      // GET: a link, see handleAccount
        server.on("/tt-forget", handleTtForget);

        // The probe paths every phone asks for. Harmless on the local network,
        // where handleCaptive sends them to "/" instead of to 192.168.4.1.
        server.on("/generate_204", handleCaptive);
        server.on("/gen_204", handleCaptive);
        server.on("/ncsi.txt", handleCaptive);
        server.on("/connecttest.txt", handleCaptive);
        server.on("/hotspot-detect.html", handleCaptive);
        server.on("/canonical.html", handleCaptive);
        server.onNotFound(handleCaptive);
    }
}

void webcfg::begin() {
    apMode = false;
    buildNames();
    if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
    routes();
    server.begin();
    Serial.printf("[webcfg] http://%s  http://%s.local\n", WiFi.localIP().toString().c_str(), HOSTNAME);
}

// The radio half of bringing the access point up, on a task of its own.
//
// It takes about 900 ms - the station stop, the mode change and softAP() itself
// wait on the Wi-Fi driver - and it runs just after the QR has been drawn.
// Done on the loop, those 900 ms were a screen that looked ready and ignored
// every touch: the back arrow to the language screen was pressed, the press was
// never read, and going back took two or three tries. Nothing here touches
// LVGL or the web server, so nothing here needs the loop.
static volatile bool apRadioBusy = false;
static volatile bool apRadioUp = false;   // radio done, server not yet started
static bool apServed = false;             // DNS and web server running

static void apRadioSteps() {
    // Stop the station interface retrying an association: it makes the radio
    // hop channels, and the access point appears to drop every few seconds.
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true, true);          // stop, and erase the station credentials
    delay(100);

    // AP+STA. The station interface is what a scan needs, and bringing it up
    // cold at scan time is part of how the list came back empty: esp_wifi
    // refuses to start a scan on an interface that has not finished starting.
    // Idle, it costs nothing.
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);                 // AP without modem-sleep = stable connections
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);   // WPA2, channel 1, max 4 clients
    delay(300);

    // The scan goes here, asynchronously. It is the only moment it can usefully
    // happen - once a phone is associated the radio is committed to it and a
    // scan comes back empty - and async means the QR is already on the panel
    // and nothing waits on it. Removing it altogether was a mistake once: the
    // networks used to be ready before anyone had finished scanning the QR.
    startBackgroundScan();
    apRadioUp = true;
}
static void apRadioBringUp(void*) {
    apRadioSteps();
    apRadioBusy = false;
    vTaskDelete(nullptr);
}

// Blocks until the radio task is finished, if one is running. Every other
// radio call waits behind it: two tasks changing the Wi-Fi mode at once is
// not something the driver is written for.
static void waitApRadio() {
    while (apRadioBusy) delay(10);
}

void webcfg::beginAP() {
    waitApRadio();
    apMode = true;
    apServed = false;
    apRadioUp = false;
    buildNames();
    // No scan here. It used to run synchronously at this point, and it is the
    // reason picking a language on a new device was followed by four seconds of
    // a frozen screen before the QR code appeared.
    // Busy is set before the task exists, so a wait can never miss it.
    apRadioBusy = true;
    if (xTaskCreatePinnedToCore(apRadioBringUp, "ap_radio", 6144, nullptr, 1,
                                nullptr, 0) != pdPASS) {
        apRadioBusy = false;
        apRadioSteps();                   // no room for a task: the old, blocking way
    }
}

// The half that needs the loop: the DNS responder and the web server are
// served from it, so they start from it once the radio is up.
static void apServeWhenReady() {
    if (apServed || !apRadioUp) return;
    apServed = true;
    captive_dns::begin(AP_IP);
    routes();
    server.begin();
    Serial.printf("[webcfg] AP '%s' (channel 1)  http://192.168.4.1/\n", AP_SSID);
}

void webcfg::loop() {
    if (apMode) {
        apServeWhenReady();
        if (!apServed) return;
        captive_dns::loop();
        // Collect the scan the moment it lands, rather than waiting for a
        // browser to ask. By the time anyone asks, a phone is associated and
        // the radio can no longer look - so the answer has to already be held.
        harvestScan();
    }
    server.handleClient();
    if (restartAt && millis() >= restartAt) { delay(50); ESP.restart(); }

    // The access point comes down only after the phone has had time to see the
    // result. Nothing reboots: the device is already on the network.
    if (apTeardownAt && millis() >= apTeardownAt) {
        apTeardownAt = 0;
        apMode = false;
        apServed = false;
        apRadioUp = false;
        captive_dns::end();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        Serial.println("[webcfg] setup access point down, station up");
    }
}

// Leaving setup without a new network: the way back from a portal opened
// from Settings. The station comes back up; joining the saved network is the
// caller's job (main.cpp, staBegin), as is putting auto-reconnect back.
void webcfg::endAP() {
    if (!apMode) return;
    waitApRadio();
    apTeardownAt = 0;
    apMode = false;
    if (apServed) captive_dns::end();
    apServed = false;
    apRadioUp = false;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    Serial.println("[webcfg] setup access point closed without a new network");
}

bool webcfg::apActive()   { return apMode; }
const char* webcfg::apName() { buildNames(); return AP_SSID; }
const char* webcfg::apPass() { buildNames(); return AP_PASS; }
int webcfg::apClients()    { return apServed ? WiFi.softAPgetStationNum() : 0; }
void webcfg::pairTick() { pairTick_(); }
bool webcfg::takePrinterSwitch(int& index, bool& on) {
    if (!g_switchWant) return false;
    index = __builtin_ctz(g_switchWant);
    on = g_switchOn & (1u << index);
    g_switchWant &= ~(1u << index);
    return true;
}
bool webcfg::webPairing(String& url, String& code, int& secondsLeft) {
    return webPairing_(url, code, secondsLeft);
}
String webcfg::url() { buildNames(); return apMode ? String("http://192.168.4.1")
                                                : String("http://") + HOSTNAME + ".local"; }
