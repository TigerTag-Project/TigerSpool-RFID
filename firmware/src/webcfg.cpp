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
#include "net/portal_page.h"
#include "version.h"
#include "web_assets.h"
#include "net/ota.h"
#include <ArduinoJson.h>
#include "printer.h"
#include "tigertag_cloud.h"
#include "i18n.h"

// The offscreen canvas lives in main.cpp; /screen.bmp serialises it as-is.
// Declared at GLOBAL scope: inside the anonymous namespace below they
// would name different symbols and the link step would fail.
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
        W_LOGIN_FAIL, W_ACCT_LINKED, W_SYNCED, W_FAILED, W_ACCT_OFF,
        W_RESTART_SUFFIX,
        W_GOOGLE, W_EMAIL, W_OR, W_NO_ACCOUNT, W_SHOW_PW,
        W_PAIR_SCAN, W_PAIR_CODE, W_PAIR_WAIT, W_PAIR_DENIED, W_PAIR_EXPIRED,
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
        /* W_SYNCED       */ { "Sincronizado", "Synced", "Sincronizado", "Synchronise" },
        /* W_FAILED       */ { "Falhou", "Failed", "Fallo", "Echoue" },
        /* W_ACCT_OFF     */ { "Conta desligada", "Account disconnected", "Cuenta desconectada", "Compte deconnecte" },
        /* W_RESTART_SUFFIX*/{ " - a reiniciar...", " - restarting...", " - reiniciando...", " - redemarrage..." },
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
    };
    // The web form still carries its own four-column table (PT, EN, ES, FR),
    // inherited from the prototype. The device now has eight languages, so an
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
    const char* LANGS[]  = { "English", "Francais", "Deutsch", "Espanol",
                             "Italiano", "Polski", "Portugues (BR)", "Portugues (PT)" };

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

    void handleShot() {
        if (!canvasReady) { server.send(503, "text/plain", "no canvas"); return; }

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
                     "https://tigersystem.io/pair?c=K7QF3M2P", "K7QF-3M2P", 587);
        else if (preview == "pairfail") screen_setup::showPairFailed("Code expired");
        else if (preview == "account") screen_setup::showAccountIntro();
        else if (preview == "signin")  screen_setup::showSignInChoice();
        else if (preview == "waiting") screen_setup::showPreparing();
        else if (preview == "email")   screen_setup::showEmailPairing("http://192.168.20.170");
        else if (preview == "settings") screen_settings::showMenu({"Atelier", "benoit@atome3d.com", 3, 6, true, true, true, "1.6.0"});
        else if (preview == "setwifi")  screen_settings::showWifi("Atelier", "192.168.20.170",
                                                                  WiFi.macAddress().c_str(), true, -55);
        else if (preview == "setacct")  screen_settings::showAccount("benoit@atome3d.com", 6, true);
        else if (preview == "setscreen") screen_settings::showScreen(80, 60, 2, false);
        // The state that cannot be reached on demand - the device is only ever
        // behind by accident - and the one whose layout is tightest.
        else if (preview == "setupdate") screen_settings::showUpdate(TIGERSPOOL_FW_VERSION, "stable",
                                             (int)ota::AVAILABLE, "9.9.9", 0);
        else if (preview == "updone")    screen_settings::showUpdate(TIGERSPOOL_FW_VERSION, "stable",
                                             (int)ota::UP_TO_DATE, "", 0);
        else if (preview == "notice") screen_settings::showUpdateNotice(TIGERSPOOL_FW_VERSION, "1.12.0");
        else if (preview == "setrestart") screen_settings::showRestart();
        else if (preview == "setfactory") screen_settings::showFactory(-1);
        else if (preview == "pick")      screen_settings::showPrinters(nullptr, 0);

        // The boot screen cannot be captured the way it is actually shown: it
        // is drawn before the web server exists. This redraws it on demand so
        // its geometry can be checked from a desk.
        //
        // It skips the LVGL pump below on purpose. That pump exists to make
        // LVGL paint the requested screen into the sprite - and painting is
        // exactly what would overwrite a bitmap that LVGL knows nothing about.
        if (preview == "splash") {
            lvgl_port::drawSplash(true);
        } else {
            lvgl_port::requestCapture(true);
            lv_obj_invalidate(lv_scr_act());
            for (uint32_t t0 = millis(); millis() - t0 < 400; ) { lv_timer_handler(); delay(5); }
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

        static uint8_t row[720];
        for (int y = H - 1; y >= 0; y--) {          // BMP stores the last row first
            uint8_t* o = row;
            for (int x = 0; x < W; x++) {
                uint32_t c = canvas.readPixel(x, y);    // RGB565 -> RGB888
                uint8_t r = (c >> 8) & 0xF8, g = (c >> 3) & 0xFC, b = (c << 3) & 0xF8;
                *o++ = b | (b >> 5);                // BMP is BGR
                *o++ = g | (g >> 6);
                *o++ = r | (r >> 5);
            }
            server.sendContent((const char*)row, rowBytes);
        }
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
    const char* LANG_CODES[] = { "en", "fr", "de", "es", "it", "pl", "pt", "ptpt" };

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

    void handleApiScan() {
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
        WiFi.begin(ssid.c_str(), pass.c_str());
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
        for (uint32_t t0 = millis(); millis() - t0 < 400; ) { lv_timer_handler(); delay(5); }
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

    void handleLogin() {
        if (ttcloud::haveSession()) { reply(wl(W_CONNECTED) + esc(ttcloud::email()), ""); return; }

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

    void handleTtLogin() {
        String mail = server.arg("ttmail"); mail.trim();
        String pass = server.arg("ttpass");
        String err;
        if (!ttcloud::signIn(mail, pass, err)) { reply(wl(W_LOGIN_FAIL), err); return; }
        String s; ttcloud::syncNow(s);
        reply(wl(W_ACCT_LINKED), s + wl(W_RESTART_SUFFIX));
        restartAt = millis() + 1600;
    }
    void handleTtSync() {
        String s;
        bool ok = ttcloud::syncNow(s);
        reply(ok ? wl(W_SYNCED) : wl(W_FAILED), s + wl(W_RESTART_SUFFIX));
        restartAt = millis() + 1600;
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
                String s; ttcloud::syncNow(s);
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
            String s; ttcloud::syncNow(s);
            reply(wl(W_ACCT_LINKED), s + wl(W_RESTART_SUFFIX));
            restartAt = millis() + 1600;
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
        server.on("/api/join", HTTP_POST, handleApiJoin);
        server.on("/api/lang", handleApiLang);
        server.on("/api/tap",  handleApiTap);
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
        server.on("/tt-sync", HTTP_POST, handleTtSync);
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

void webcfg::beginAP() {
    apMode = true;

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
    buildNames();
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);   // WPA2, channel 1, max 4 clients
    delay(300);

    // No scan here. It used to run synchronously at this point, and it is the
    // reason picking a language on a new device was followed by four seconds of
    // a frozen screen before the QR code appeared: a scan takes seconds, and
    // the whole of setup was queued behind it for a list nobody had asked for
    // yet. The portal fetches the list itself when it is opened, and starts a
    // background scan when its page is served.
    captive_dns::begin(AP_IP);
    routes();
    server.begin();
    Serial.printf("[webcfg] AP '%s' (channel 1)  http://192.168.4.1/\n", AP_SSID);
    // The scan goes here, asynchronously. It is the only moment it can usefully
    // happen - once a phone is associated the radio is committed to it and a
    // scan comes back empty - and async means the QR is already on the panel
    // and nothing waits on it, which was the point of taking the BLOCKING scan
    // off this path. Removing it altogether was the mistake: the networks used
    // to be ready before anyone had finished scanning the QR.
    startBackgroundScan();
}

void webcfg::loop() {
    if (apMode) {
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
        captive_dns::end();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        Serial.println("[webcfg] setup access point down, station up");
    }
}

bool webcfg::apActive()   { return apMode; }
const char* webcfg::apName() { buildNames(); return AP_SSID; }
const char* webcfg::apPass() { buildNames(); return AP_PASS; }
int webcfg::apClients()    { return apMode ? WiFi.softAPgetStationNum() : 0; }
void webcfg::pairTick() { pairTick_(); }
bool webcfg::webPairing(String& url, String& code, int& secondsLeft) {
    return webPairing_(url, code, secondsLeft);
}
String webcfg::url() { buildNames(); return apMode ? String("http://192.168.4.1")
                                                : String("http://") + HOSTNAME + ".local"; }
