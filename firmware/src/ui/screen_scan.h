#pragma once
#include "reader.h"

// The three screens of the everyday loop: present the spool, confirm what was
// read, see what the printer actually stored.
namespace screen_scan {

// No status dots: reaching this screen already proves the printer answered and
// the tap came off its slot grid, so a pair of green dots would only repeat
// what the user just did.
void showScan(const char* slotLabel, const char* errorOrNull);

void showReview(const char* slotLabel, const TagInfo& tag);

// `sentColour` is what the printer ended up with. When it differs from the
// tag's own colour the screen shows both and says so - several printers accept
// only their own palette, and a bare "OK" in front of a spool that is visibly a
// different colour reads as a bug. Pass 0xFFFFFFFF when unknown.
void showResult(const char* slotLabel, bool ok, const char* message,
                const TagInfo& tag, uint32_t sentColour);

void invalidate();
bool takeCancel();
bool takeSend();
bool takeDismiss();

}  // namespace screen_scan
