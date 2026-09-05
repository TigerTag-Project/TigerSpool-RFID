#pragma once
#include <lvgl.h>

// LVGL bound to this board's panel and touch controller.
//
// Rendering path: LVGL draws into small DMA-capable buffers in INTERNAL RAM and
// those go straight to the panel by DMA, with two buffers so rendering and
// transfer overlap. The full-screen sprite is a shadow copy for /screen.bmp
// only, written on the frames where a screenshot was actually requested - a
// PSRAM write per frame would be the slowest thing in the loop, in service of a
// feature used a few times a day. See lvgl_port.cpp for the reasoning.
namespace lvgl_port {

void begin();
// Pumps LVGL. Returns the milliseconds LVGL wants before the next call, so the
// main loop can idle instead of spinning.
uint32_t loop();

// Screenshot support: mirror the next full repaint into the sprite.
void requestCapture(bool on);

// Increments every time LVGL finishes painting. /screen polls this instead of
// re-fetching the framebuffer: the bitmap is 150 KB and the counter is four
// bytes, so a page that only fetches when the panel actually changed reacts in
// a tenth of a second instead of waiting out a poll interval sized for the
// cost of the image.
uint32_t frameCounter();
bool capturing();

// Queue a synthetic touch at a panel coordinate, as if a finger had landed
// there. Used by /api/tap so the interface can be navigated and screenshotted
// without anyone standing at the bench. It obeys the sleep rule like a real
// touch: on a dark screen it wakes and is consumed rather than acted on.
// Draw the boot screen straight to the panel. Lives here because the bitmap is
// 150 KB and a header included by two translation units is 150 KB twice.
void drawSplash(bool alsoCanvas = false);

void injectTap(int x, int y);

// Drag from one point to another, reported as a moving press. Scrolls a list
// the way a finger does - without it, anything below the fold cannot be reached
// from a desk.
void injectSwipe(int x1, int y1, int x2, int y2);

void setBacklight(uint8_t percent);   // 0 = off, used by screen sleep

// 0 or 2 - the panel the right way up, or turned through 180 degrees. Which
// one is right depends on how the board was mounted in its shell, and both
// mountings exist, so this is the user's choice rather than a build constant.
// Applied immediately: LovyanGFX turns the touch coordinates with the display,
// and both rotations are portrait, so LVGL's resolution does not change and
// there is nothing to rebuild - only to repaint.
void setRotation(int rotation);
uint8_t backlight();

// Screen sleep. Call every loop with the user's settings; it dims, then goes
// dark, and wakes on the next touch. Scanning and printer polling never stop.
void sleepTick(int timeoutSec, uint8_t awakeBrightness);
bool asleep();

}  // namespace lvgl_port
