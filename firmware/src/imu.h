// The board's accelerometer, used for one question: which way up is the panel?
//
// A QMI8658 at 0x6B, on the SAME I2C bus as the touch controller - SDA 48,
// SCL 47 - not on GPIO6/7. That was worth measuring rather than assuming: a
// scan of this board finds 0x15, 0x6B and 0x7E on the touch bus and nothing at
// all on GPIO6/7.
//
// Only the sign of one axis is needed. The panel is portrait either way, so
// there are two answers - upright or turned through 180 degrees - and gravity
// distinguishes them without any calibration.
#pragma once
#include <Arduino.h>

namespace imu {

// True if the chip answered. Everything below returns a safe default if not:
// an absent or broken IMU must never stop a device from showing its screen.
bool begin();
bool present();

// 0 or 2, the same values the display rotation uses, or -1 when the reading is
// not usable - lying flat on a bench, where gravity says nothing about which
// edge is up.
int suggestedRotation();

}  // namespace imu
