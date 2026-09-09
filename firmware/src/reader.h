#pragma once
#include <Arduino.h>

// One decoded TigerTag.
//
// Colour and temperatures come straight off the chip. Material and brand are
// numeric ids on the tag, resolved to names through the generated table in
// include/tigertag_db.h - so a scan needs no network.
struct TagInfo {
    bool     ok = false;
    uint32_t idProduct = 0;
    uint16_t idMaterial = 0, idBrand = 0;
    // Page 0x08 is four bytes, not three: R, G, B and an ALPHA. Every colour on
    // this chip is stored that way - the second and third carry one too, at
    // bytes 39 and 43. Nothing in this firmware blends with it, but the tester
    // exists to show what is on the chip, and a byte not shown is a byte
    // nobody can check.
    uint8_t  r = 0, g = 0, b = 0, a = 0;
    uint16_t nozMin = 0, nozMax = 0;
    uint8_t  bedMin = 0, bedMax = 0;
    // Straight off the chip, as numbers rather than resolved labels - this is
    // what the NFC tester exists to show. Offsets come from the TigerTag memory
    // layout: aspects share page 0x06 with the material id, kind and diameter
    // share 0x07 with the brand id, drying shares 0x0B with the bed window, and
    // the stamp is page 0x0C on its own.
    //
    // Checked field by field against TigerTag's own Python SDK (`_parse` in
    // tigertag/tag.py) on a real dump, not read off a diagram: every value this
    // decoder produces matches it. The SDK is the reference if they ever
    // disagree. What it parses and this does not, deliberately: the quantity
    // and its unit (bytes 20-23), the second and third colours and the HueForge
    // distance (36-45), the 28-byte custom message (48-75), the remaining
    // quantity the scale keeps up to date (76-79) and the ECDSA signature
    // (80-143). Reading them means reading to page 0x17 instead of 0x0F.
    uint8_t  aspect1 = 0, aspect2 = 0;   // primary / secondary finish
    uint8_t  kind = 0;                   // filament, resin, accessory...
    uint8_t  diameter = 0;               // 1.75 / 2.85
    uint32_t protocol = 0;               // page 0x04, the tag family
    String   aspect1Label, aspect2Label; // resolved, "-" when the id is unknown
    String   kindLabel, diameterLabel, protocolLabel;
    uint8_t  dryTemp = 0, dryHours = 0;
    uint32_t stamp = 0;                  // seconds since 2000-01-01 GMT; the
                                         // same u32 is the twin tag pairing id

    // Pages 0x09 and 0x0D-0x17. Everything the chip carries beyond the spool's
    // identity: how much filament it left the factory with, how much is left
    // now, the extra colours a bicolour or tricolour spool needs, the HueForge
    // transmission distance, and 28 bytes of whatever the maker wanted to say.
    uint32_t measure = 0;                // quantity at manufacturing, u24
    uint8_t  unitId = 0;
    String   unitLabel;
    uint32_t available = 0;              // remaining, kept up to date by the scale
    // How many of the three colours actually mean something.
    //
    // NOT derived from the colour bytes, and it cannot be: 00 00 00 is a
    // perfectly good black, and it is also exactly what an unused slot holds.
    // The chip gives no way to tell those apart. The ASPECT does - "Bicolor"
    // carries two, "Tricolor" three - and the count comes from the reference
    // database rather than from ids written into this firmware.
    uint8_t  colorCount = 1;
    // Kept for the callers that only want to know whether a non-black second
    // or third colour is present at all.
    bool     hasColor2 = false, hasColor3 = false;
    uint8_t  c2r = 0, c2g = 0, c2b = 0, c2a = 0;
    uint8_t  c3r = 0, c3g = 0, c3b = 0, c3a = 0;
    uint16_t tdRaw = 0;                  // HueForge transmission distance x10
    String   message;                    // custom text, ASCII-filtered for the panel

    // ECDSA-P256 over SHA-256(uid + id_tigertag + id_product), against the
    // public key that ships with the protocol version. Offline, no server.
    enum Sig : uint8_t { SIG_UNREAD = 0, SIG_NONE, SIG_VALID, SIG_INVALID, SIG_NO_KEY };
    uint8_t  signature = SIG_UNREAD;
    String   material;               // resolved label, e.g. "PETG"
    String   brand;                  // resolved label, e.g. "Polymaker"
    String   uid;                    // the chip's own serial, hex, no separators
    String   pages;                  // 0x04-0x0B as hex, for the test screen

    // Creality wants seven hex digits: a '0' after the '#', then RRGGBB.
    String colorHexCreality() const {
        char buf[10];
        snprintf(buf, sizeof(buf), "#0%02x%02x%02x", r, g, this->b);
        return String(buf);
    }
};

namespace reader {
    // Brings the PN532 up over HSU. Returns false if it does not answer, which
    // is normal on the first attempt - the caller retries every two seconds.
    bool begin();

    // Is a tag in the field? Fills `uid`/`len` when they are given, so a caller
    // can tell "the same spool is still there" from "a different one arrived"
    // without paying for a full read - which is 575 ms and the reason the test
    // screen used to freeze.
    //
    // "Safe to call from the UI loop" was doing a lot of work in the old
    // comment: it costs 33 ms with a tag in the field and up to 120 ms without
    // one, so it is safe to call, not free. Callers rate-limit it.
    bool present(uint8_t* uid = nullptr, uint8_t* len = nullptr);

    // Reads and decodes. Makes several attempts: one successful read is not
    // evidence of a good read on a link this marginal. See docs/WIRING.md.
    bool read(TagInfo& out);

    // Written for the user, not the developer: "move the spool closer" rather
    // than "read error".
    String lastError();
}
