#include "bambu_cloud.h"
#include "tigertag_cloud.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

namespace {

const char* CLOUD_HOST_SUFFIX = ".mqtt.bambulab.com";

// Enough for every cloud printer an account is likely to have, and the table
// is a few dozen bytes per row. A ninth is refused rather than silently lost.
const int MAX_SUBS = 8;

struct Sub {
    String serial;
    String topic;           // device/<serial>/report, built once at attach
    bambu_cloud::Sink sink;
    bool   subscribed = false;
};

Sub  s_subs[MAX_SUBS];
int  s_n = 0;

// Allocated with the first attach and released with the last detach, so that
// an account with no cloud printer carries neither the object nor, more to the
// point, a TLS session.
WiFiClientSecure* s_net  = nullptr;
PubSubClient*     s_mqtt = nullptr;

uint32_t s_lastTry = 0;
bool     s_wasUp = false;
String   s_host, s_user, s_token;

void onMessage(char* topic, uint8_t* payload, unsigned int len) {
    // Strictly by topic. This is the one place a report could be handed to
    // the wrong printer, and a wrong printer would show another machine's
    // spools as its own - so there is no fallback and no guessing.
    for (int i = 0; i < s_n; i++)
        if (s_subs[i].topic == topic) { s_subs[i].sink(payload, len); return; }
}

void subscribeAll() {
    for (int i = 0; i < s_n; i++)
        s_subs[i].subscribed = s_mqtt->subscribe(s_subs[i].topic.c_str());
}

void open() {
    if (s_mqtt) return;
    s_net  = new WiFiClientSecure();
    s_mqtt = new PubSubClient(*s_net);
    // Bambu's broker presents a certificate this device has no store to check
    // against, the same situation as a printer on the LAN; trust comes from
    // the account session Tiger Studio obtained. See net/tls.h.
    s_net->setInsecure();
    s_net->setHandshakeTimeout(5);
    // A full report from a four-unit X1 is about 50 KB and has to fit whole.
    // It lands in PSRAM - anything over 4 KB does on this build - so the size
    // costs nothing that is short.
    s_mqtt->setBufferSize(51200);
    s_mqtt->setKeepAlive(30);
    // Longer than the per-printer sessions had: this one carries every cloud
    // printer's reports, a pushall on connect is several of them in a row, and
    // PubSubClient applies this limit to every READ as well as to the connect.
    // Four seconds hung up on printers that were answering, once per poll.
    s_mqtt->setSocketTimeout(6);
    s_mqtt->setCallback(onMessage);
    s_lastTry = 0;
    s_wasUp = false;
    Serial.println("[bambu-cloud] session object created");
}

void close() {
    if (!s_mqtt) return;
    s_mqtt->disconnect();
    delete s_mqtt; s_mqtt = nullptr;
    delete s_net;  s_net  = nullptr;
    s_wasUp = false;
    Serial.println("[bambu-cloud] last printer left - session closed");
}

}  // namespace

bool bambu_cloud::attach(const String& serial, Sink sink) {
    for (int i = 0; i < s_n; i++)
        if (s_subs[i].serial == serial) { s_subs[i].sink = sink; return true; }
    if (s_n >= MAX_SUBS) {
        Serial.printf("[bambu-cloud] table full - %s not attached\n", serial.c_str());
        return false;
    }
    Sub& s = s_subs[s_n++];
    s.serial = serial;
    s.topic  = String("device/") + serial + "/report";
    s.sink   = sink;
    s.subscribed = false;
    open();
    if (s_mqtt->connected()) s.subscribed = s_mqtt->subscribe(s.topic.c_str());
    Serial.printf("[bambu-cloud] %s attached (%d on the session)\n", serial.c_str(), s_n);
    return true;
}

void bambu_cloud::detach(const String& serial) {
    for (int i = 0; i < s_n; i++) {
        if (s_subs[i].serial != serial) continue;
        if (s_mqtt && s_mqtt->connected()) s_mqtt->unsubscribe(s_subs[i].topic.c_str());
        for (int j = i; j < s_n - 1; j++) s_subs[j] = s_subs[j + 1];
        s_subs[--s_n] = Sub();
        Serial.printf("[bambu-cloud] %s detached (%d left)\n", serial.c_str(), s_n);
        break;
    }
    if (s_n == 0) close();
}

void bambu_cloud::loop() {
    if (!s_mqtt) return;
    if (s_mqtt->connected()) {
        s_mqtt->loop();
        return;
    }
    if (s_wasUp) {
        Serial.printf("[bambu-cloud] session lost, state %d\n", s_mqtt->state());
        s_wasUp = false;
        for (int i = 0; i < s_n; i++) s_subs[i].subscribed = false;
    }
    if (millis() - s_lastTry < 4000) return;
    s_lastTry = millis();

    String region;
    if (!ttcloud::bambuCloud(s_user, s_token, region)) {
        Serial.println("[bambu-cloud] no cloud session in the account - "
                       "sign in to Bambu in Tiger Studio");
        return;
    }
    s_host = region + CLOUD_HOST_SUFFIX;
    s_mqtt->setServer(s_host.c_str(), 8883);

    // One id per DEVICE now, not per printer: there is one session. The
    // account may still be open in Bambu Studio and on a phone, and those use
    // their own ids, so this cannot collide with them.
    const String cid = "tigerspool-" + String((uint32_t)ESP.getEfuseMac(), HEX) + "-cloud";
    Serial.printf("[bambu-cloud] connecting to %s for %d printer(s)...\n",
                  s_host.c_str(), s_n);
    if (s_mqtt->connect(cid.c_str(), s_user.c_str(), s_token.c_str())) {
        subscribeAll();
        s_wasUp = true;
        Serial.printf("[bambu-cloud] up - one TLS session, %d subscription(s)\n", s_n);
    } else {
        Serial.printf("[bambu-cloud] connect failed, state %d\n", s_mqtt->state());
    }
}

bool bambu_cloud::connected(const String& serial) {
    if (!s_mqtt || !s_mqtt->connected()) return false;
    for (int i = 0; i < s_n; i++)
        if (s_subs[i].serial == serial) return s_subs[i].subscribed;
    return false;
}

bool bambu_cloud::publish(const String& serial, const String& body) {
    if (!s_mqtt || !s_mqtt->connected()) return false;
    const String t = String("device/") + serial + "/request";
    return s_mqtt->publish(t.c_str(), body.c_str());
}

bool bambu_cloud::active() { return s_mqtt != nullptr; }
