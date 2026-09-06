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
void show(const char* printerName, PrinterBackend* backend,
          int selected, bool readerReady, int link);
void invalidate();                 // force a rebuild on the next show()
int  takeTappedSlot();             // slot index, or -1
bool takeBack();
bool takeRetry();          // the user asked to reconnect

}  // namespace screen_slots
