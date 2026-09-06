#pragma once
#include <Arduino.h>

// The TigerTag reference tables, in two layers.
//
// WHY TWO. The tables compiled into the firmware are what makes a brand-new box
// work out of the box, on a bench with no network: a spool scans and reads
// "PLA High Speed / R3D" the first time it is presented. But they are frozen at
// the moment the release was built, and TigerTag adds materials, brands and
// finishes continuously - so a device six weeks old already shows "brand#48804"
// on a spool whose brand has existed for a month. Nothing fails and nothing
// logs; the firmware simply does not know the word.
//
// So the compiled tables are the FLOOR, not the source. The device downloads
// the current ones into its filesystem and prefers those; the compiled ones are
// what it falls back to when a file is missing, unparsable or empty. That
// fallback is the point of keeping them: a bad download must cost nothing,
// never a device that has forgotten every material it knew.
//
// Every lookup here answers from the downloaded table if it has one and from
// the compiled table otherwise, per table rather than all-or-nothing - a
// corrupt brand file does not throw away a good material file.
namespace tt_db {

// Mounts the filesystem and loads whatever is already there. Safe to call when
// nothing has ever been downloaded: every table simply stays on its compiled
// fallback. Never blocks on the network.
void begin();

// Fetch anything the API has newer than what is on disk, on its own task.
// Returns false if one is already running or there is no network. The tables in
// use are only swapped for a downloaded file that parsed - so a failed update
// leaves the device exactly as capable as it was a moment before.
bool updateAsync();
bool updating();

// True once a refresh has finished since boot, whether or not it changed
// anything - the difference between "we are current" and "we have not asked".
bool everChecked();

// How many entries each layer holds, for the NFC tester and the log.
int  loadedCount();          // downloaded entries in use, across all tables
String summary();            // one line, English, for the serial log

const char* material(uint16_t id);
const char* brand(uint16_t id);
const char* aspect(uint16_t id);
const char* type(uint16_t id);
const char* diameter(uint16_t id);
const char* unit(uint16_t id);
const char* version(uint32_t id);

}  // namespace tt_db
