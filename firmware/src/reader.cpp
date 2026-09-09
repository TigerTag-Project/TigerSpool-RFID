#include "reader.h"
#include "config.h"
#include "i18n.h"
#include <mbedtls/pk.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/sha256.h>
#include "tt_db.h"
#include "tigertag_db.h"
#include <PN532_HSU.h>
#include <PN532.h>

namespace {
    HardwareSerial serNFC(PN532_UART_NUM);
    PN532_HSU      pn532hsu(serNFC);
    PN532         nfc(pn532hsu);
    String        g_err;

    uint16_t be16(const uint8_t* p) { return (uint16_t(p[0]) << 8) | p[1]; }
    uint32_t be24(const uint8_t* p) {
        return (uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | p[2];
    }
    uint32_t be32(const uint8_t* p) {
        return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
    }
}

static bool g_diag = true;   // first boot: verbose logging

bool reader::begin() {
    serNFC.begin(PN532_UART_BAUD, SERIAL_8N1, PN532_UART_RX, PN532_UART_TX);
    delay(20);

    if (g_diag) {
        // Are any bytes arriving on RX? (nothing = dead line or no power;
        //  garbage = TX/RX swapped, DIPs not in HSU mode, 5 V level, or wrong UART)
        delay(30);
        int n = serNFC.available(); uint8_t pk[8]; int m = 0;
        while (serNFC.available() && m < 8) pk[m++] = serNFC.read();
        Serial.printf("[reader] cfg UART%d RX=GPIO%d TX=GPIO%d @%d  RX idle bytes=%d",
                      PN532_UART_NUM, PN532_UART_RX, PN532_UART_TX, PN532_UART_BAUD, n);
        for (int i = 0; i < m; i++) Serial.printf(" %02X", pk[i]);
        Serial.println();
    }

    // The vendored library wakes the module before EVERY command. These modules
    // power down between commands: without that preamble only the first command
    // after boot answers, and the reader reports itself ready while reading
    // nothing. See docs/WIRING.md.
    pn532hsu.wakeup();
    delay(50);
    for (int t = 0; t < 5; t++) {
        uint32_t v1 = nfc.getFirmwareVersion();
        nfc.SAMConfig();
        uint32_t v2 = nfc.getFirmwareVersion();   // tem de responder DEPOIS do 1o
        if (g_diag)
            Serial.printf("[reader] init try %d: v1=0x%08X v2=0x%08X\n", t, v1, v2);
        if (v1 && v2) {
            Serial.printf("[reader] PN532 fw %u.%u OK\n",
                          (uint8_t)((v1 >> 16) & 0xFF), (uint8_t)((v1 >> 8) & 0xFF));
            nfc.setPassiveActivationRetries(0x02);
            g_diag = false;
            return true;
        }
        pn532hsu.wakeup();                        // wake it again before retrying
        delay(200);
    }
    g_err = "PN532 not answering (HSU)";
    return false;
}

bool reader::present(uint8_t* uidOut, uint8_t* lenOut) {
    uint8_t uid[7] = {0}; uint8_t ul = 0;
    if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &ul, 120)) return false;
    if (uidOut && lenOut) { memcpy(uidOut, uid, ul > 7 ? 7 : ul); *lenOut = ul; }
    return true;
}

// ECDSA-P256 over SHA-256(uid || id_tigertag || id_product), against the public
// key that ships with the protocol version. Entirely offline: the key is
// compiled in from id_version.json, so a spool can be proved genuine on a bench
// with no network at all - which is the whole point of signing it.
//
// mbedtls_ecdsa_verify takes r and s as MPIs, so the DER wrapper the desktop
// SDKs build is not needed here; the two halves go straight in.
static uint8_t verifySignature(const uint8_t* uid, uint8_t uidLen,
                               const uint8_t* payload) {
    bool anySig = false;
    for (int i = 80; i < 144 && !anySig; i++) anySig = payload[i] != 0;
    if (!anySig) return TagInfo::SIG_NONE;

    const char* pem = tt_public_key(be32(payload + 0));
    if (!pem || !*pem) return TagInfo::SIG_NO_KEY;

    uint8_t msg[15];
    memcpy(msg, uid, uidLen > 7 ? 7 : uidLen);
    memcpy(msg + 7, payload + 0, 4);      // id_tigertag, big-endian on the chip
    memcpy(msg + 11, payload + 4, 4);     // id_product
    uint8_t hash[32];
    mbedtls_sha256(msg, 15, hash, 0);

    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    uint8_t status = TagInfo::SIG_INVALID;
    // parse_public_key wants the terminating NUL counted in the length.
    if (mbedtls_pk_parse_public_key(&pk, (const unsigned char*)pem,
                                    strlen(pem) + 1) == 0) {
        mbedtls_ecp_keypair* kp = mbedtls_pk_ec(pk);
        mbedtls_mpi r, sv;
        mbedtls_mpi_init(&r); mbedtls_mpi_init(&sv);
        if (kp && mbedtls_mpi_read_binary(&r, payload + 80, 32) == 0
               && mbedtls_mpi_read_binary(&sv, payload + 112, 32) == 0
               && mbedtls_ecdsa_verify(&kp->grp, hash, 32, &kp->Q, &r, &sv) == 0) {
            status = TagInfo::SIG_VALID;
        }
        mbedtls_mpi_free(&r); mbedtls_mpi_free(&sv);
    } else {
        status = TagInfo::SIG_NO_KEY;
    }
    mbedtls_pk_free(&pk);
    return status;
}

bool reader::read(TagInfo& out) {
    out = TagInfo{};
    // Pages 0x04..0x27, the whole of an NTAG213's user memory.
    //
    // 0x04-0x17 is the tag proper and every byte of it is now decoded. The last
    // sixteen pages are the ECDSA signature, and they are read SEPARATELY and
    // allowed to fail: a Maker tag has none, a partial read of them says
    // nothing about the spool, and a tag whose identity was read perfectly must
    // not be rejected because sixty-four bytes of signature did not arrive.
    uint8_t payload[144];
    bool win = false;

    uint8_t uid[7] = {0}; uint8_t ul = 0;
    int okReads = 0, zeroReads = 0;

    // Twenty attempts, but no more than 1.2 seconds of them.
    //
    // The attempt count alone was the whole budget, and a read that never wins
    // - a spool being lifted away mid-read, a tag at the edge of the field -
    // spent 20 x 150 ms of polling inside the UI loop. Measured at the bench:
    // a single back press took 6.8 seconds to answer because it landed behind
    // one of these. The caller retries every 300 ms, so a shorter attempt is
    // not a worse chance of reading; it is the same chance, spent in slices
    // small enough that the screen keeps moving between them.
    const uint32_t deadline = millis() + 1200;
    for (int attempt = 0; attempt < 20 && !win; attempt++) {
        if ((int32_t)(millis() - deadline) >= 0) break;
        if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &ul, 150)) { delay(8); continue; }
        delay(5);                    // deixa a tag assentar no campo

        bool got;
        if (attempt < 10) {
            // Two 16-byte transactions: the READ command returns four pages at
            // once, which is far more reliable than eight small reads.
            got = true;
            for (uint8_t p = 0; p < 5 && got; p++)
                got = nfc.mifareultralight_ReadPage16(0x04 + p * 4, payload + p * 16);
        } else {
            // Fallback: eight 4-byte reads. Smaller frames survive a poor link
            // better on a marginal HSU link)
            got = true;
            for (uint8_t i = 0; i < 20 && got; i++)
                got = nfc.mifareultralight_ReadPage(0x04 + i, payload + i * 4);
        }
        if (!got) { delay(8); continue; }
        okReads++;

        // Count zero bytes over the FIRST EIGHT PAGES only. The four that were
        // added after them are legitimately empty on most spools - a
        // single-colour filament has no second or third colour - so counting
        // them would reject a perfectly good read.
        int zc = 0;
        for (int i = 0; i < 32; i++) if (payload[i] == 0) zc++;
        if (zc > 20 || be32(payload + 0) == 0 || be32(payload + 4) == 0) {
            zeroReads++;
            delay(10);
            continue;
        }
        win = true;
    }

    // Diagnostic dump
    { String u; char h[4];
      for (uint8_t i = 0; i < ul; i++) { snprintf(h, sizeof(h), "%02X", uid[i]); u += h; }
      String px; for (int i = 0; i < 80; i++) { snprintf(h, sizeof(h), "%02X", payload[i]); px += h; if (i % 4 == 3) px += ' '; }
      Serial.printf("[reader] UID=%s (%ub)  reads ok=%d zeros=%d  win=%d\n      p04-0F: %s\n",
                    u.c_str(), ul, okReads, zeroReads, win, px.c_str()); }

    if (!win) { g_err = i18n::T(S_READ_UNSTABLE); return false; }

    // Carried on the tag rather than only in the log: the reader test screen is
    // where someone answers "did it read the RIGHT thing", and a UID that only
    // exists on a serial console cannot be compared with the label on a spool.
    { char h[4];
      for (uint8_t i = 0; i < ul; i++) { snprintf(h, sizeof(h), "%02X", uid[i]); out.uid += h; }
      for (int i = 0; i < 80; i++) {
          snprintf(h, sizeof(h), "%02X", payload[i]); out.pages += h;
          if (i % 4 == 3 && i != 79) out.pages += ' ';
      } }

    out.idProduct  = be32(payload + 4);
    out.idMaterial = be16(payload + 8);
    out.idBrand    = be16(payload + 14);
    out.r = payload[16]; out.g = payload[17]; out.b = payload[18];
    out.a = payload[19];
    out.nozMin = be16(payload + 24);
    out.nozMax = be16(payload + 26);
    out.bedMin = payload[30];
    out.bedMax = payload[31];
    // Page 0x06 is | ID Material u16 | Aspect 1 | Aspect 2 |, so the aspects
    // are one byte each rather than the 16-bit pair their position suggests.
    // Page 0x07 is | Type | Diameter | ID Brand u16 |. Page 0x09 is a u24
    // quantity and a unit, NOT the timestamp - that is page 0x0C, and it also
    // carries the twin tag id, which is why it is reported raw.
    out.aspect1  = payload[10];
    out.aspect2  = payload[11];
    out.kind     = payload[12];
    out.diameter = payload[13];
    out.dryTemp  = payload[28];
    out.dryHours = payload[29];
    out.stamp    = be32(payload + 32);
    out.protocol = be32(payload + 0);
    out.measure   = be24(payload + 20);
    out.unitId    = payload[23];
    out.c2r = payload[36]; out.c2g = payload[37]; out.c2b = payload[38];
    out.c2a = payload[39];
    out.c3r = payload[40]; out.c3g = payload[41]; out.c3b = payload[42];
    out.c3a = payload[43];
    // All three zero is "no second colour", not "black". A tricolour spool that
    // really wanted black would still differ in one of the other two, and a
    // mono spool leaves the whole block at zero - which is the shape the tag
    // uses to say the field is unset.
    out.hasColor2 = out.c2r || out.c2g || out.c2b;
    out.hasColor3 = out.c3r || out.c3g || out.c3b;
    out.tdRaw     = be16(payload + 44);
    out.available = be24(payload + 76);

    // 28 bytes of free text, UTF-8, and the spec allows emoji. The panel's font
    // is ASCII, so anything outside it would arrive as a blank box: shown as a
    // dot instead, which reads as "there is a character here I cannot draw"
    // rather than as a gap in the text.
    for (int i = 48; i < 76 && payload[i]; i++) {
        const uint8_t c = payload[i];
        out.message += (char)((c >= 0x20 && c < 0x7F) ? c : '.');
    }
    out.message.trim();

    const char* m = tt_db::material(out.idMaterial);
    const char* br = tt_db::brand(out.idBrand);
    out.material = m ? String(m) : (String("MAT#") + out.idMaterial);
    out.brand    = br ? String(br) : (String("brand#") + out.idBrand);

    // An id on screen is a number somebody has to look up. Every one of these
    // has a reference table compiled in, so the tester shows the word and
    // falls back to the number only when the table does not know the id -
    // which is itself the useful answer, because it means the tables are
    // behind the tags.
    const auto label = [](const char* got, uint8_t id) {
        return got ? String(got) : (String("#") + id);
    };
    // Either aspect can be the one that says how many colours there are, so the
    // larger of the two wins: a spool marked Silk and Bicolor has two.
    {
        const uint8_t c1 = tt_aspect_colors(out.aspect1);
        const uint8_t c2 = tt_aspect_colors(out.aspect2);
        out.colorCount = c1 > c2 ? c1 : c2;
    }
    out.aspect1Label  = label(tt_db::aspect(out.aspect1), out.aspect1);
    out.aspect2Label  = label(tt_db::aspect(out.aspect2), out.aspect2);
    out.kindLabel     = label(tt_db::type(out.kind), out.kind);
    out.diameterLabel = label(tt_db::diameter(out.diameter), out.diameter);
    const char* pv = tt_db::version(out.protocol);
    out.protocolLabel = pv ? String(pv) : (String("#") + (unsigned long)out.protocol);
    const char* unit = tt_db::unit(out.unitId);
    out.unitLabel = unit ? String(unit) : (String("#") + out.unitId);

    // The signature, read on its own and allowed to fail. Four transactions
    // more, for sixty-four bytes that say nothing about the filament and
    // everything about whether this tag is genuine.
    bool sigGot = true;
    for (uint8_t p = 0; p < 4 && sigGot; p++)
        sigGot = nfc.mifareultralight_ReadPage16(0x18 + p * 4, payload + 80 + p * 16);
    out.signature = sigGot ? verifySignature(uid, ul, payload) : TagInfo::SIG_UNREAD;
    out.ok = true;
    Serial.printf("[reader] %s / %s  #%02X%02X%02X  nozzle %u-%u  bed %u-%u\n"
                  "          %s %s mm  aspect %s/%s  stamp %lu  dry %u C / %u h  [%s]\n",
                  out.material.c_str(), out.brand.c_str(), out.r, out.g, out.b,
                  out.nozMin, out.nozMax, out.bedMin, out.bedMax,
                  out.kindLabel.c_str(), out.diameterLabel.c_str(),
                  out.aspect1Label.c_str(), out.aspect2Label.c_str(),
                  (unsigned long)out.stamp, out.dryTemp, out.dryHours,
                  out.protocolLabel.c_str());
    Serial.printf("          %lu/%lu %s  TD %u.%u  sig=%u  msg='%s'\n",
                  (unsigned long)out.available, (unsigned long)out.measure,
                  out.unitLabel.c_str(), out.tdRaw / 10, out.tdRaw % 10,
                  out.signature, out.message.c_str());
    return true;
}

String reader::lastError() { return g_err; }
