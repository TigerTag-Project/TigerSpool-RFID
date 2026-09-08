#pragma once
#include "printer.h"

// The slot grid of the selected printer.
//
// It scrolls. The prototype paginated six at a time, which was already wrong for
// a Bambu X1 reporting seventeen trays and is worse for an Anycubic Kobra X
// reporting twenty - and its page arrows were a 2 mm target for moving past a
// 12 mm one.
namespace screen_slots {

// `link`: 0 idle, 1 connecting, 2 up, 3 gave up. When it is 3 the header
// carries a retry button instead of a dot - an indicator that spins for ever
// is one nobody believes, and there has to be a way to ask again.
// `tries`/`budget` and `fetching` drive the line under the spinner - what the
// device is doing right now, not merely that it is doing something. They are
// deliberately NOT part of what decides a rebuild: a counter that rebuilt the
// screen would destroy the spinner and start it again from zero on every tick,
// which is the exact bug that made the Wi-Fi screen look frozen.
// `cloud` marks a printer this device can read and must not write. Its slots
// are drawn and are NOT tappable, and the screen says why rather than letting
// somebody press a cell that will never do anything.
void show(const char* printerName, PrinterBackend* backend,
          int selected, bool readerReady, int link,
          int tries, int budget, bool fetching, bool cloud);
void invalidate();                 // force a rebuild on the next show()
int  takeTappedSlot();             // slot index, or -1
bool takeBack();
bool takeRetry();          // the user asked to reconnect

}  // namespace screen_slots
