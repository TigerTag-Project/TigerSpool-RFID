#include "imu.h"
#include <Wire.h>

namespace imu {
namespace {

constexpr uint8_t ADDR      = 0x6B;
constexpr uint8_t REG_WHOAMI = 0x00;
constexpr uint8_t REG_CTRL1  = 0x02;   // serial interface
constexpr uint8_t REG_CTRL2  = 0x03;   // accelerometer range / rate
constexpr uint8_t REG_CTRL7  = 0x08;   // which sensors are enabled
constexpr uint8_t REG_AX_L   = 0x35;   // six bytes: X, Y, Z, little-endian
constexpr uint8_t WHOAMI_QMI = 0x05;

bool s_present = false;

bool wr(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ADDR);
    Wire.write(reg); Wire.write(val);
    return Wire.endTransmission() == 0;
}

bool rd(uint8_t reg, uint8_t* buf, size_t n) {
    Wire.beginTransmission(ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    return Wire.requestFrom((int)ADDR, (int)n) == (int)n
        && Wire.readBytes(buf, n) == n;
}

}  // namespace

bool begin() {
    // The touch driver already owns this bus; begin() on an already-started
    // Wire is a no-op for the pins and simply hands us the same port.
    Wire.begin(48, 47, 400000);

    uint8_t who = 0;
    if (!rd(REG_WHOAMI, &who, 1) || who != WHOAMI_QMI) {
        Serial.printf("[imu] not found (who=0x%02X)\n", who);
        s_present = false;
        return false;
    }
    wr(REG_CTRL1, 0x60);   // auto-increment on reads, little-endian
    wr(REG_CTRL2, 0x04);   // +/-2 g, 250 Hz - far more than this needs
    wr(REG_CTRL7, 0x01);   // accelerometer on, gyroscope off: it is not used,
                           // and leaving it off is the difference between a few
                           // hundred microamps and a few milliamps.
    delay(30);             // one output period, so the first read is real
    s_present = true;
    Serial.println("[imu] QMI8658 at 0x6B on the touch bus");
    return true;
}

bool present() { return s_present; }

int suggestedRotation() {
    if (!s_present) return -1;

    uint8_t b[6];
    if (!rd(REG_AX_L, b, sizeof(b))) return -1;
    // X is the axis that runs along the panel's long edge on this board -
    // measured, not assumed. Standing in its printed holder the device reads
    // x = -9360, y = 50, z = -14790: gravity split between X and Z by the
    // holder's tilt, and nothing at all on Y. Y was the obvious guess and it
    // is the wrong one.
    const int16_t x = (int16_t)(b[0] | (b[1] << 8));

    // At +/-2 g a count is about 1/16384 g, so 4000 is roughly a quarter of a
    // gravity. Steep enough that a box lying flat on a bench answers "I cannot
    // tell" instead of guessing, shallow enough that a holder tilted well back
    // still answers - the one measured here is at 0.57 g on this axis.
    constexpr int16_t FIRM = 4000;
    if (x < -FIRM) return 2;       // the way up it is in now
    if (x >  FIRM) return 0;
    return -1;                     // flat, or too close to call
}

}  // namespace imu
