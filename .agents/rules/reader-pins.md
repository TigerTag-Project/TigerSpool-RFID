# Reader wiring

**Rule.** The PN532 is on UART1 at 115200, GPIO44 RX and GPIO43 TX. Never GPIO6/7.

**Why.** GPIO6/7 is an I2C bus on this board with pull-ups, shared with the IMU and the camera header. A reader wired there enumerates and answers, then returns random UIDs with failing reads - it looks like a bad tag or antenna.

**How.**
- The pins live in `firmware/src/config.h`; `check-wiring.py` fails when a wiring table in the docs disagrees with them.
- Warning against GPIO6/7 in prose is fine; prescribing them in a table is not.
