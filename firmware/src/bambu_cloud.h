#pragma once
#include <Arduino.h>
#include <functional>

// One TLS session for every Bambu Lab printer an account has in cloud mode.
//
// WHY. A TLS session costs about 45 KB of internal RAM on this device -
// measured, see linkCost() in main.cpp - and internal RAM is what limits how
// many printers can be open at once. Every cloud printer on an account talks to
// the same regional broker with the same credentials; only the topic differs,
// device/<serial>/report. Nothing in MQTT asks for a connection per topic, and
// it was verified on the bench that one session receives the reports of every
// printer it subscribes to. So three cloud printers cost one session, not three.
//
// WHAT IS SHARED AND WHAT IS NOT. The pipe is shared. Each printer still has
// its own backend object, its own slots, its own status and its own dot: a
// report arriving on device/<serial>/report is handed to the backend that
// attached with that serial and to nobody else. LAN printers are not involved
// at all - each of those is its own broker and keeps its own session.
//
// The session exists only while at least one cloud printer is attached, so an
// account with no cloud printers pays nothing for this.
namespace bambu_cloud {

// Called with the raw bytes of one report for the serial that attached it.
using Sink = std::function<void(uint8_t* payload, unsigned len)>;

// A cloud-mode backend joins with its serial. The first one opens the session;
// the last one to leave closes it. Returns false if the table is full.
bool attach(const String& serial, Sink sink);
void detach(const String& serial);

// Connect, reconnect and pump. Safe to call from every attached backend on
// every pass: the work is idempotent and reconnects are rate-limited here.
void loop();

// True when the session is up AND this serial's subscription is in place.
bool connected(const String& serial);

// Publish to device/<serial>/request over the shared session.
bool publish(const String& serial, const String& body);

// Whether a session is open at all - what makes a second cloud printer cheap.
bool active();

}  // namespace bambu_cloud
