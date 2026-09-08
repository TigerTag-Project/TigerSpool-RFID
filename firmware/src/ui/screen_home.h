#pragma once
#include "printer.h"

// The home screen: the printers imported from the TigerTag account.
//
// This is the screen a user comes back to constantly, so it does two things the
// prototype's version could not. It SCROLLS instead of paginating - a Bambu with
// four AMS units and an 11-printer account both overflow four rows, and the
// pagination arrows were a smaller target than the rows they scrolled. And it
// never waits on the network: the list is drawn from NVS immediately, and the
// account sync updates it from a background task.
namespace screen_home {

// Builds the screen on first call, refreshes it afterwards. Cheap to re-call.
// `wifiRssi` is dBm, or 0 when there is no connection. It is bucketed into
// four levels before it reaches the screen's redraw signature: raw dBm moves by
// a few points every second on a still desk, and a screen that rebuilds itself
// on that loses the scroll position while someone is reading it.
// `account` is ttcloud::health(): 2 reachable, 1 signed in but unreachable,
// 0 not signed in. It replaced a dot that reported whether a sync happened to
// be running - true for a second every five minutes, and grey the rest of the
// time, which is not something anyone can act on.
void show(const PrinterCfg* printers, int count,
          int selected, const bool* online, bool syncing, int wifiRssi,
          int account);

// True while this screen owns the display, so the legacy raw-drawn screens know
// to leave the canvas alone.
bool active();
void leave();

// Set by the screen when the user taps something. -1 means nothing pending.
int  takeTappedPrinter();   // index into printers[], or -1
bool takeSettingsTap();
// The refresh button in the header: ask the account for the printer list now,
// rather than waiting out the five-minute cycle. The Settings printer list
// carries the same one.
bool takeReloadTap();

}  // namespace screen_home
