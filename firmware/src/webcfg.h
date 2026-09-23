#pragma once
#include <Arduino.h>

// The configuration web server, in two modes and one implementation.
//
//   begin()   - on the network: http://<ip>/ and http://tigerspool.local/
//   beginAP() - access point + captive portal, entered automatically when
//               there is no usable network. AP "TigerSpool-Setup" at
//               http://192.168.4.1/, with a scan of nearby networks.
//
// The portal is a STATE of this firmware, never a separate binary to flash. A
// product whose recovery path is "flash a different firmware" has already
// failed the person holding it.
//
// It also serves the screen capture used to check the UI remotely:
//   /screen.bmp             the panel contents, right now
//   /screen.bmp?preview=... one of the setup screens, without changing state
//   /screen                 a page that refreshes the capture
namespace webcfg {
    void begin();              // station mode, on the local network
    void beginAP();            // access point + captive portal
    void endAP();              // and back down, without joining anything
    void loop();               // call from the main loop
    bool apActive();
    String url();              // the address to show the user
    const char* apName();
    const char* apPass();      // WPA2 key for the setup AP - see buildNames
    int  apClients();          // devices currently joined to the setup AP

    // True while a Google pairing started from the web page is still waiting.
    // The device puts the same QR on its own screen for the duration, so
    // whoever walked away from the phone can still finish on a PC - and so the
    // box does not sit there looking idle while it is in fact waiting.
    bool webPairing(String& url, String& code, int& secondsLeft);

    // Ask Google whether the pairing was approved. Called by main while the
    // pairing screen is up, so completing it never depends on a phone browser
    // being awake. Blocking HTTPS, about a second, rate-limited internally.
    void pairTick();
    // A printer switch flipped on the account page, once. main.cpp applies it:
    // printers[] is its state, and the load budget is its decision.
    bool takePrinterSwitch(int& index, bool& on);
}
