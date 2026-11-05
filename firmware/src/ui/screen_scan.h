#pragma once
#include "reader.h"

// The three screens of the everyday loop: present the spool, confirm what was
// read, see what the printer actually stored.
namespace screen_scan {

// No status dots: reaching this screen already proves the printer answered and
// the tap came off its slot grid, so a pair of green dots would only repeat
// what the user just did.
// It also stays up, unchanged, while the spool is being written to the
// printer: the write gets no screen and not even a word - it is over before
// one could be read.
// A read that fails is NOT reported here. The screen already says what to do -
// hold the spool against the box - and a red line saying the same thing under
// somebody who is doing exactly that reads as a fault of their own. The
// reader's error goes to the serial log.
void showScan(const char* slotLabel);

// The success screen confirms what was SENT and names the next step: put the
// spool in that slot. It deliberately says nothing about what the slot held
// before, and nothing about the colour the printer settled on - the question
// somebody has at this moment is whether their spool went through, and where
// to go now.
//
// `msLeft` is what is left of the five seconds before it closes itself, drawn
// as a draining bar. A tap anywhere closes it at once. `message` is only read
// on failure.
//
// `warning` is the exception to "says nothing about what the printer did": a
// part of the send that did NOT happen although the send succeeded, such as a
// weight that was not updated. Drawn under the spool, in the warning colour;
// nullptr for none.
void showResult(const char* slotLabel, bool ok, const char* message,
                const TagInfo& tag, uint32_t msLeft, const char* warning = nullptr);

void invalidate();
bool takeCancel();
bool takeDismiss();

}  // namespace screen_scan
