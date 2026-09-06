#include "tt_db.h"
#include "tigertag_db.h"
#include "net/tls.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace {

const char* API_BASE = "https://api.tigertag.io/api:tigertag";
const char* DIR = "/tt";

// The same seven datasets the desktop tools pull, and the same last_update
// handshake. This is a second reader of a public API, not a second source of
// truth: if the two ever disagree about what a field is called, the API wins.
struct Dataset {
    const char* key;        // the name inside last_update.json
    const char* endpoint;
    const char* file;       // under /tt
    const char* labelKey;   // "label" for most, "name" for brands and versions
};
const Dataset SETS[] = {
    { "filament_materials", "material/get/all",          "id_material.json",     "label" },
    { "brands",             "brand/get/all",             "id_brand.json",        "name"  },
    { "aspects",            "aspect/get/all",            "id_aspect.json",       "label" },
    { "types",              "type/get/all",              "id_type.json",         "label" },
    { "filament_diameters", "diameter/filament/get/all", "id_diameter.json",     "label" },
    { "measure_units",      "measure_unit/get/all",      "id_measure_unit.json", "label" },
    { "versions",           "version/get/all",           "id_version.json",      "name"  },
};
const int SET_N = sizeof(SETS) / sizeof(SETS[0]);

// One loaded table. Ids and labels live in PSRAM: there is 8 MB of it and none
// of it is contended, while internal RAM is what LVGL's draw buffers and every
// TLS session compete for.
struct Table {
    uint32_t* ids = nullptr;
    char**    labels = nullptr;
    char*     blob = nullptr;      // one allocation holding every string
    int       n = 0;

    void clear() {
        free(ids); free(labels); free(blob);
        ids = nullptr; labels = nullptr; blob = nullptr; n = 0;
    }
    const char* find(uint32_t id) const {
        int lo = 0, hi = n;
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (ids[mid] == id) return labels[mid];
            if (ids[mid] < id) lo = mid + 1; else hi = mid;
        }
        return nullptr;
    }
};

Table g_tables[SET_N];
bool  g_mounted = false;
volatile bool g_busy = false;
volatile bool g_checked = false;
String g_summary = "reference tables: compiled-in only";

void* psAlloc(size_t n) {
    void* p = ps_malloc(n);
    return p ? p : malloc(n);       // no PSRAM is not a reason to give up
}

// ---------------------------------------------------------------------------
//  Loading one file into a table
// ---------------------------------------------------------------------------

// Parses with a filter, so a 47 KB file becomes a document of a few kilobytes
// rather than one of a hundred: everything except the id and the label is
// discarded as it streams past, and the file is never held in memory whole.
bool loadFile(int idx, Table& out) {
    const Dataset& d = SETS[idx];
    String path = String(DIR) + "/" + d.file;
    File f = LittleFS.open(path, "r");
    if (!f) return false;

    JsonDocument filter;
    JsonObject row = filter.add<JsonObject>();
    row["id"] = true;
    row[d.labelKey] = true;

    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, f, DeserializationOption::Filter(filter));
    f.close();
    if (err) {
        Serial.printf("[ttdb] %s: %s - keeping the compiled table\n",
                      d.file, err.c_str());
        return false;
    }
    JsonArrayConst arr = doc.as<JsonArrayConst>();
    if (arr.isNull() || arr.size() == 0) {
        Serial.printf("[ttdb] %s: empty - keeping the compiled table\n", d.file);
        return false;
    }

    // Two passes: measure, then fill. One allocation for every string together
    // means one free, and no per-label heap fragmentation on a device that will
    // do this again in six hours.
    size_t bytes = 0; int count = 0;
    for (JsonObjectConst e : arr) {
        const char* lab = e[d.labelKey];
        if (!e["id"].is<long long>() || !lab || !*lab) continue;
        bytes += strlen(lab) + 1;
        count++;
    }
    if (!count) return false;

    Table t;
    t.ids    = (uint32_t*)psAlloc(sizeof(uint32_t) * count);
    t.labels = (char**)   psAlloc(sizeof(char*) * count);
    t.blob   = (char*)    psAlloc(bytes);
    if (!t.ids || !t.labels || !t.blob) { t.clear(); return false; }

    char* w = t.blob;
    for (JsonObjectConst e : arr) {
        const char* lab = e[d.labelKey];
        if (!e["id"].is<long long>() || !lab || !*lab) continue;
        t.ids[t.n] = (uint32_t)(long long)e["id"];
        t.labels[t.n] = w;
        size_t len = strlen(lab);
        memcpy(w, lab, len + 1);
        w += len + 1;
        t.n++;
    }

    // Sorted so a lookup is a binary search. Insertion sort: these tables are a
    // few hundred entries and already nearly ordered, and it costs no stack.
    for (int i = 1; i < t.n; i++) {
        uint32_t id = t.ids[i]; char* lab = t.labels[i];
        int j = i - 1;
        while (j >= 0 && t.ids[j] > id) {
            t.ids[j + 1] = t.ids[j]; t.labels[j + 1] = t.labels[j]; j--;
        }
        t.ids[j + 1] = id; t.labels[j + 1] = lab;
    }

    out.clear();
    out = t;
    return true;
}

void loadAll() {
    int total = 0, files = 0;
    for (int i = 0; i < SET_N; i++) {
        if (loadFile(i, g_tables[i])) { total += g_tables[i].n; files++; }
    }
    g_summary = files ? (String("reference tables: ") + total + " entries from "
                         + files + " downloaded file(s)")
                      : String("reference tables: compiled-in only");
    Serial.printf("[ttdb] %s\n", g_summary.c_str());
}

// ---------------------------------------------------------------------------
//  Downloading
// ---------------------------------------------------------------------------

int httpsGet(const String& url, String& body) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("[ttdb] not connected (WiFi.status()=%d)\n", WiFi.status());
        return -100;      // NOT -1: HTTPClient already uses that for a refused
                          // connection, and the two look identical in a log
                          // exactly when you need to tell them apart.
    }
    WiFiClientSecure c; tls::secure(c);
    HTTPClient h;
    if (!h.begin(c, url)) return -2;
    h.setTimeout(15000);
    int code = h.GET();
    body = (code > 0) ? h.getString() : String();
    h.end();
    return code;
}

// Everything a bad payload has to get past before it can replace a good file.
//
// A 200 is not proof. An API mid-migration answers `[]`, a soft failure answers
// an object, a captive portal answers HTML, and a dropped connection answers
// half an array - each of which parses or fails in a way that would otherwise
// leave the device with a reference table it cannot use, and no way back.
bool validate(const String& body, const Dataset& d, int& countOut) {
    JsonDocument filter;
    JsonObject row = filter.add<JsonObject>();
    row["id"] = true;
    row[d.labelKey] = true;

    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;
    JsonArrayConst arr = doc.as<JsonArrayConst>();
    if (arr.isNull() || arr.size() == 0) return false;
    int good = 0;
    for (JsonObjectConst e : arr) {
        const char* lab = e[d.labelKey];
        if (!e["id"].is<long long>() || !lab || !*lab) return false;
        good++;
    }
    countOut = good;
    return good > 0;
}

// Write to a temp file and rename. A rename is the only way the file at `path`
// is either entirely the old one or entirely the new one; writing in place
// truncates first, so a reset in the wrong half-second leaves a device with a
// file it will refuse to parse for ever after.
bool writeAtomic(const String& path, const String& body) {
    String tmp = path + ".tmp";
    File f = LittleFS.open(tmp, "w");
    if (!f) return false;
    const size_t n = f.print(body);
    f.close();
    if (n != body.length()) { LittleFS.remove(tmp); return false; }
    LittleFS.remove(path);
    return LittleFS.rename(tmp, path);
}

void updateTask(void*) {
    String body;
    int code = httpsGet(String(API_BASE) + "/all/last_update", body);
    if (code != 200) {
        Serial.printf("[ttdb] last_update: HTTP %d (%s)\n", code,
                      body.length() ? body.substring(0, 60).c_str() : "no body");
        g_busy = false; vTaskDelete(nullptr); return;
    }
    JsonDocument remote;
    if (deserializeJson(remote, body)) {
        Serial.println("[ttdb] last_update did not parse");
        g_busy = false; vTaskDelete(nullptr); return;
    }

    JsonDocument local;
    { File f = LittleFS.open(String(DIR) + "/last_update.json", "r");
      if (f) { deserializeJson(local, f); f.close(); } }

    int changed = 0;
    bool allOk = true;
    for (int i = 0; i < SET_N; i++) {
        const Dataset& d = SETS[i];
        String path = String(DIR) + "/" + d.file;
        const bool missing = !LittleFS.exists(path);
        if (!missing && remote[d.key] == local[d.key]) continue;

        String payload;
        code = httpsGet(String(API_BASE) + "/" + d.endpoint, payload);
        int count = 0;
        if (code != 200 || !validate(payload, d, count)) {
            Serial.printf("[ttdb] %s: refused (HTTP %d) - keeping what we have\n",
                          d.file, code);
            allOk = false;
            continue;
        }
        if (!writeAtomic(path, payload)) {
            Serial.printf("[ttdb] %s: could not be written\n", d.file);
            allOk = false;
            continue;
        }
        Serial.printf("[ttdb] %s: %d entries\n", d.file, count);
        changed++;
    }

    // The stamp is written only when every dataset is current, for the same
    // reason the desktop tool does it: it records what is on disk, so stamping
    // it after a partial run would make the next run skip a file it never got.
    if (allOk) writeAtomic(String(DIR) + "/last_update.json", body);
    if (changed) loadAll();
    else Serial.println("[ttdb] nothing changed");

    g_checked = true;
    g_busy = false;
    vTaskDelete(nullptr);
}

}  // namespace

namespace tt_db {

void begin() {
    // A first boot has no filesystem at all, so format it - there is nothing to
    // lose on a partition this firmware has never written, and the alternative
    // is a device that can never store a table.
    // The partition LABEL matters and it is not the default. Arduino's LittleFS
    // looks for a partition called "spiffs"; ours is called "littlefs" (with
    // subtype spiffs, which is what the subtype means). Called without the
    // label, begin() finds nothing, formats nothing and returns false - which
    // is exactly what it did, silently, on the first flash of this code.
    g_mounted = LittleFS.begin(true, "/littlefs", 10, "littlefs");
    if (!g_mounted) {
        Serial.println("[ttdb] no filesystem - compiled tables only");
        return;
    }
    if (!LittleFS.exists(DIR)) LittleFS.mkdir(DIR);
    loadAll();
}

bool updating()    { return g_busy; }
bool everChecked() { return g_checked; }
String summary()   { return g_summary; }

int loadedCount() {
    int n = 0;
    for (int i = 0; i < SET_N; i++) n += g_tables[i].n;
    return n;
}

bool updateAsync() {
    if (!g_mounted || g_busy || WiFi.status() != WL_CONNECTED) return false;
    g_busy = true;
    // 16 KB: mbedTLS needs room to shake hands, and the JSON is parsed on this
    // stack too. The same figure the account sync uses, for the same reasons.
    // Core 0, for the same reason the account sync is: this parses tens of
    // kilobytes of JSON and holds a TLS session, and the interface has core 1.
    if (xTaskCreatePinnedToCore(updateTask, "ttdb", 16384, nullptr, 1, nullptr, 0)
        != pdPASS) {
        g_busy = false;
        Serial.println("[ttdb] xTaskCreate failed - update skipped");
        return false;
    }
    return true;
}

// Downloaded first, compiled second. Per table, so one bad file costs one
// table's worth of freshness and nothing else.
const char* material(uint16_t id) {
    const char* v = g_tables[0].find(id); return v ? v : tt_material(id);
}
const char* brand(uint16_t id) {
    const char* v = g_tables[1].find(id); return v ? v : tt_brand(id);
}
const char* aspect(uint16_t id) {
    const char* v = g_tables[2].find(id); return v ? v : tt_aspect(id);
}
const char* type(uint16_t id) {
    const char* v = g_tables[3].find(id); return v ? v : tt_type(id);
}
const char* diameter(uint16_t id) {
    const char* v = g_tables[4].find(id); return v ? v : tt_diameter(id);
}
const char* unit(uint16_t id) {
    const char* v = g_tables[5].find(id); return v ? v : tt_unit(id);
}
const char* version(uint32_t id) {
    const char* v = g_tables[6].find(id); return v ? v : tt_version(id);
}

}  // namespace tt_db
