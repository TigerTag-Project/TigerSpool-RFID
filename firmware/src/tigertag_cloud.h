#pragma once
#include <Arduino.h>

// The TigerTag account: sign in, and import the user's printers.
//
// This is the premise of the product. Printers are added once in Tiger Studio,
// on a computer with a keyboard; the device reads them from the account and
// configures itself. Nothing here is ever typed on a 2.0" screen.
//
// Two ways in, both without a device keyboard:
//   - email and password, typed on the phone through the config page
//   - a QR pairing flow for accounts created with Google, which have no
//     password to type. See docs/ACCOUNT-PAIRING.md.
//
namespace ttcloud {
    void   begin();                       // load the stored session from NVS
    bool   haveSession();
    String email();

    // What to put on a settings row: the account's display name when it has
    // one, the address otherwise. A name is what the user calls the account;
    // an address is 30 characters that get truncated to "benoit@atom...".
    // Never empty while there is a session.
    String displayName();
    String photoUrl();                    // the account's picture, or empty
    String lastResult();

    // Email and password. Stores the refresh token, never the password.
    bool   signIn(const String& mail, const String& pass, String& err);
    void   forget();                      // sign out and clear the session

    // QR pairing, for accounts created with Google. The URL in the QR already
    // carries the code, so scanning it is the whole interaction - nothing is
    // read off one screen and typed into another.
    //   1) pairStart() -> short code + verify URL to display + poll token
    //   2) pairPoll()  -> repeat until approved, yields a custom token
    //   3) signInWithCustomToken() -> stores the session
    // The uid comes from the JWT claim: signInWithCustomToken does not return
    // localId in the body.
    bool   pairStart(String& code, String& verifyUrl, String& pollToken,
                     int& intervalS, String& err);
    int    pairPoll(const String& pollToken, String& customToken,
                    String& emailOut, String& err);   // <0 error, 0 pending, 1 approved, 2 denied, 3 expired
    bool   signInWithCustomToken(const String& customToken, const String& emailHint,
                                 String& err);

    bool   due();                         // is a re-sync due?
    // Has the account ever been read on this boot? Distinguishes "still
    // loading" from "this account really has no printers" - two states that
    // look identical on an empty list and mean opposite things.
    bool   everSynced();

    // Four answers, and green is an assertion about the LAST EXCHANGE rather
    // than about holding a token. A device whose token is valid but whose
    // network died reports trouble instead of staying green until the token
    // expires half an hour later.
    //   3  the last exchange with the account succeeded          - nothing to do
    //   2  linked, but the last exchange failed or has gone stale - fix something
    //   1  linked, working on it - bounded, so it cannot mean "stuck" for ever
    //   0  no account linked                                     - link one
    int    health();
    bool   syncNow(String& summary);      // blocking; prefer startAsyncSync()
    bool   consumeChanged();              // true once, if the last sync changed anything

    // Runs the sync on its own task. The home screen is where the user comes
    // back constantly and it must never wait on the network: the list is drawn
    // from NVS immediately, and this updates it underneath.
    bool   startAsyncSync();              // false if one is already running
    // Makes a sync due now; the loop starts it, on the task, once there is room.
    // What a web handler calls instead of syncNow(): those run on the loop task,
    // whose 8 KB stack a TLS handshake nested in a handler overflows.
    void   requestSync();
    // For a page waiting on a sync it asked for: finished syncs so far, and
    // whether the last one changed a list main.cpp has not reloaded yet.
    uint32_t syncCount();
    bool     changePending();
    bool   asyncBusy();
    // True while a sync is waiting for internal RAM it cannot get: the task
    // stack is 16 KB and must be contiguous. main.cpp reads this the same way
    // it reads product_api::needsRoom(), and stands the background printer
    // links down until there is a block big enough.
    bool   needsRoom();
    bool   asyncTake(String& summary);    // true once, when it finishes

    // Bambu Lab's own cloud session, as Tiger Studio stores it in the account.
    //
    // The device never signs in to Bambu: the desktop does that and writes the
    // result to users/{uid}/printers/bambulab/secrets/cloud_session, which is
    // where these come from. The token lasts about three months, so an empty
    // answer means "renew it in Studio" rather than "something broke".
    // Returns false when the account carries no session.
    bool   bambuCloud(String& mqttUser, String& token, String& region);

    // pairStart on its own task. It is a blocking HTTPS round trip of a second
    // or two, and it happens while the screen is showing a spinner - a spinner
    // that freezes for the whole wait is worse than no spinner, because it
    // reads as a crash.
    bool   startPairAsync();
    bool   pairAsyncBusy();
    // 0 while running, 1 when the code is ready, -1 on failure.
    int    pairAsyncTake(String& code, String& verifyUrl, String& pollToken,
                         int& intervalS, String& err);

    // ---- presence: what this device tells the account about itself ----------
    //
    // Studio draws a device list out of these documents, so the identity,
    // liveness and power fields carry the SAME names a TigerScale writes. An
    // online dot and a battery icon must not have to know which product they
    // are looking at. See docs/PRESENCE.md for the whole contract.

    // A printer's own document id in the account, by position in printers[].
    // Refreshed by every sync and kept in RAM only - the NVS partition is
    // frozen and has no room for it; see the note in the sync.
    String printerDocId(int i);

    // The device document id: the Wi-Fi MAC, lowercase hex, no separators.
    // It is the identity of this box in the account, so its FORMAT is frozen -
    // change it and every device already registered is orphaned under its old
    // id, with no way to notice from the device side.
    String deviceId();

    struct Presence {
        // liveness and power, in the shared vocabulary
        int    wifiDbm        = 0;      // 0 when not connected -> written null
        String ip;
        bool   batteryPresent = false;
        int    batteryPercent = -1;     // <0 -> written null
        bool   charging       = false;
        bool   chargingKnown  = false;  // false -> written null
        bool   onUsb          = true;
        bool   screenOff      = false;

        // what a TigerSpool is, that a scale is not
        int           printersActive = 0;
        const String* printerIds     = nullptr;   // account document ids
        int           printerIdCount = 0;
        // A spool was written to a printer since the last beat. It turns into
        // last_used_at, stamped by the SERVER: this device has no clock it can
        // vouch for - no RTC, no NTP - so a time it wrote itself would be a
        // guess presented as a fact.
        bool          usedNow        = false;
    };

    // One Firestore commit, blocking, about a second. `full` also writes the
    // identity block and reads display_name before writing it, so a name typed
    // in Studio is never trampled. Fields outside the mask are untouched.
    bool heartbeat(const Presence& p, bool full, String& err);
}
