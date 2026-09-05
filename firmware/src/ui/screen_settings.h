#pragma once
#include "printer.h"

// Settings, and the printer picker that lives inside it.
namespace screen_settings {

// The menu. Entries are fixed; what each one shows on the right is its current
// value, so the list answers "what is it set to" without opening anything.
enum Entry {
    E_NONE = -1,
    E_PRINTERS = 0,
    E_WIFI,
    E_ACCOUNT,
    E_SCREEN,
    E_LANGUAGE,
    E_UPDATE,
    E_RESTART,
    E_FACTORY,
    E_COUNT
};

// What the menu draws. A struct rather than eight positional arguments: two
// adjacent bools in a call are two bools nobody can read at the call site, and
// swapping them compiles.
//
// `updateWaiting` colours the Update row and puts the new version on it. It is
// the only place a waiting update is announced outside its own page: a spool
// reader is not a phone, and a badge on the home screen would be nagging.
struct MenuState {
    const char* network;          // the SSID, or the offline label
    const char* account;          // the e-mail, or the "add it on the web" label
    int         visiblePrinters;
    int         totalPrinters;
    bool        wifiUp;           // tints the Wi-Fi icon
    bool        signedIn;         // tints the Account icon
    bool        updateWaiting;
    const char* latest;
};
void showMenu(const MenuState& st);
Entry takeEntry();
bool  takeBack();
void  invalidate();

// The printer picker: every printer the account knows about, each with a switch.
//
// Hiding is not deleting. A hidden printer stays in the account and stays
// synced; it simply does not crowd a 2.0" screen belonging to someone who owns
// three machines and cares about one of them today.
void showPrinters(const PrinterCfg* printers, int count);
int  takeToggled();          // index whose switch was flipped, or -1

// ---- the rest of the settings views ---------------------------------------
//
// Each one answers a question and offers at most one action. A settings screen
// that lists five things you could do is a screen nobody reads.
enum Action { A_NONE = 0, A_CHANGE_WIFI, A_SIGN_OUT, A_RESTART, A_FACTORY, A_CHECK_UPDATE,
              A_INSTALL_NOW, A_LATER };
Action takeAction();

void showWifi(const char* ssid, const char* ip, const char* mac, bool connected,
              int rssi);   // dBm, 0 when not connected
void showAccount(const char* email, int printers, bool linked);
void showScreen(uint8_t brightness, int sleepSeconds, int rotation, bool autoRot);
int  takeBrightness();       // new percentage, or -1
int  takeSleep();            // new timeout in seconds, or -1
// 0 or 2 for a fixed orientation, AUTO_ROT to follow the accelerometer, or
// ROT_NONE when the user has not touched it.
constexpr int AUTO_ROT = -1;
constexpr int ROT_NONE = -2;
int  takeRotation();
// `channel` is still taken and still hashed into the redraw signature, but it
// is not drawn: there is one channel, and a row that always reads "stable" is a
// row nobody reads twice. It stays in the signature so the day a second channel
// exists, switching it redraws.
void showUpdate(const char* version, const char* channel,
                int otaState, const char* latest, int percent);
// Shown once, after the check that runs on boot finds a newer version. It is
// the only interruption this device makes: an update it never mentions is an
// update nobody installs, and the Settings row alone is only seen by someone
// who already went looking.
void showUpdateNotice(const char* current, const char* latest);

void showRestart();
void showFactory(int holdPercent);   // -1 = not holding
bool factoryHolding();               // true while the finger is down

}  // namespace screen_settings
