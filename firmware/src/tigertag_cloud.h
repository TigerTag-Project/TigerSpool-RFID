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
    bool   asyncBusy();
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
}
