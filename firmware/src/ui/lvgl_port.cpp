#include "lvgl_port.h"
#include "theme.h"
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "LGFX_ESP32_S3_Touch_LCD_2.h"
#include "config.h"
#include "splash.h"

extern LGFX        lcd;
static volatile uint32_t s_frame = 0;
extern LGFX_Sprite canvas;      // shadow framebuffer, only for /screen.bmp
extern bool        canvasReady;

namespace {

// ---------------------------------------------------------------------------
//  Draw buffers: INTERNAL, DMA-capable RAM. Not PSRAM.
//
//  This is the single most important choice for fluidity on this chip. The CPU
//  writes every pixel of a draw buffer, then DMA reads all of it back out.
//  Internal SRAM runs at several hundred MB/s; PSRAM over the octal bus runs at
//  a fraction of that, and it is the same bus the framebuffer sprite and the
//  network stacks are already using. LVGL's *heap* belongs in PSRAM (widget
//  metadata, touched rarely - see lv_conf.h); its draw buffers do not.
//
//  Two buffers of 40 lines. LVGL renders into one while the other is being
//  transferred, so rendering and DMA overlap instead of taking turns. 40 lines
//  is 1/8 of the screen, comfortably above LVGL's 1/10 guidance, and costs
//  2 x 19.2 KB of a 320 KB pool.
// ---------------------------------------------------------------------------
constexpr uint32_t BUF_LINES = 40;
constexpr size_t   BUF_PX    = SCR_W * BUF_LINES;

lv_color_t*        s_buf1 = nullptr;
lv_color_t*        s_buf2 = nullptr;
lv_disp_draw_buf_t s_drawBuf;
lv_disp_drv_t      s_dispDrv;
lv_indev_drv_t     s_indevDrv;
uint8_t            s_backlight  = 100;
volatile bool      s_capture    = false;   // mirror into the sprite this frame
bool               s_dmaStarted = false;

// One-shot: dump the bytes that are about to reach the panel, for the band that
// contains the header bar. If these match what LVGL resolved, the fault is
// downstream in the panel; if they do not, it is in this function.
static bool s_dumpArmed = true;

void flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* px) {
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;

    if (s_dumpArmed && area->y1 <= 12 && area->y2 >= 12) {
        s_dumpArmed = false;
        const lv_color_t* row = px + (12 - area->y1) * w;   // a row inside the header
        Serial.printf("[ui] flush area x%d..%d y%d..%d  header row y=12:",
                      area->x1, area->x2, area->y1, area->y2);
        for (int i = 0; i < 6 && i < w; i++) Serial.printf(" %04X", row[i].full);
        Serial.printf("   sizeof(lv_color_t)=%u\n", (unsigned)sizeof(lv_color_t));
    }

    // Wait for the PREVIOUS transfer, not this one. By the time we get here
    // LVGL has already rendered into the other buffer, so that wait is usually
    // already satisfied and costs nothing - which is the entire point of double
    // buffering. Waiting after the push instead would serialise the two.
    lcd.waitDMA();

    if (!s_dmaStarted) { lcd.startWrite(); s_dmaStarted = true; }
    lcd.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t*)px);

    // The shadow copy is only made when someone actually asked for a screenshot.
    // Doing it every frame would put a PSRAM write in the hot path for a feature
    // used a few times a day.
    if (s_capture && canvasReady)
        canvas.pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t*)px);

    // Hold the SPI transaction open across every area of one refresh and close
    // it on the last: one bus setup per frame instead of one per rectangle.
    // Closing it also matters because the legacy raw-drawn screens still share
    // this bus, and they must not find it held.
    if (lv_disp_flush_is_last(drv)) { lcd.endWrite(); s_dmaStarted = false; s_frame++; }

    lv_disp_flush_ready(drv);
}

uint32_t s_lastTouch = 0;
bool     s_asleep    = false;

// A tap queued over HTTP by /api/tap, so the interface can be driven and
// photographed without a finger. It is reported to LVGL exactly like a real
// touch: pressed for a few reads, then released, because a click LVGL can see
// is a press and a release at the same point.
//
// It never bypasses the sleep rule below: a synthetic tap on a dark screen
// wakes it and is consumed, the same as a real one. Anything else would let a
// remote tap send filament to a slot while nobody can see the screen.
volatile int32_t s_tapX = -1, s_tapY = -1;
volatile int     s_tapReads = 0;
// A swipe is the same mechanism walked between two points: LVGL decides a drag
// happened from the movement between reads, so a list scrolls exactly as it
// would under a finger. Without it half the settings menu is unreachable from a
// desk, which is half the point of being able to drive the screen at all.
volatile int32_t s_swipeToX = -1, s_swipeToY = -1;
volatile int     s_swipeSteps = 0;

void touchCb(lv_indev_drv_t*, lv_indev_data_t* data) {
    int32_t x, y;
    bool down = lcd.getTouch(&x, &y);

    if (!down && s_swipeSteps > 0) {
        // Walk from the start point towards the end, one step per read.
        int total = 12;
        int i = total - s_swipeSteps;
        x = s_tapX + (s_swipeToX - s_tapX) * i / total;
        y = s_tapY + (s_swipeToY - s_tapY) * i / total;
        down = true;
        s_swipeSteps--;
    } else if (!down && s_tapReads > 0) {
        x = s_tapX; y = s_tapY; down = true;
        s_tapReads--;
    }

    if (down) s_lastTouch = millis();

    // A dimmed or dark screen must not accept taps whose result cannot be
    // seen. The first touch wakes it and is CONSUMED by the wake - otherwise
    // reaching for a sleeping device sends a filament to whichever slot the
    // finger happened to land on.
    if (s_asleep) { data->state = LV_INDEV_STATE_RELEASED; return; }

    if (down) {
        data->point.x = x;
        data->point.y = y;
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

}  // namespace

namespace lvgl_port {

void drawSplash(bool alsoCanvas) {
    lcd.pushImage(0, 0, SPLASH_W, SPLASH_H, (lgfx::rgb565_t*)SPLASH_DATA);
    // The capture route serialises the sprite, not the panel, so a screenshot
    // of the boot screen needs it in both.
    if (alsoCanvas && canvasReady)
        canvas.pushImage(0, 0, SPLASH_W, SPLASH_H, (lgfx::rgb565_t*)SPLASH_DATA);
}

void injectTap(int x, int y) {
    s_tapX = x; s_tapY = y;
    s_swipeSteps = 0;
    s_tapReads = 3;          // held for a few reads so LVGL sees a real press
}

void injectSwipe(int x1, int y1, int x2, int y2) {
    s_tapX = x1; s_tapY = y1;
    s_swipeToX = x2; s_swipeToY = y2;
    s_tapReads = 0;
    s_swipeSteps = 12;
}

void begin() {
    lv_init();

    // MALLOC_CAP_DMA implies internal RAM on this chip and guarantees the
    // alignment the SPI DMA engine needs.
    s_buf1 = (lv_color_t*)heap_caps_malloc(BUF_PX * sizeof(lv_color_t), MALLOC_CAP_DMA);
    s_buf2 = (lv_color_t*)heap_caps_malloc(BUF_PX * sizeof(lv_color_t), MALLOC_CAP_DMA);
    if (!s_buf1 || !s_buf2) {
        // Half the buffers rather than half the screen: a single buffer still
        // draws correctly, it just cannot overlap render and transfer.
        Serial.println("[ui] DMA buffers unavailable at 40 lines - retrying at 20");
        if (s_buf1) heap_caps_free(s_buf1);
        if (s_buf2) heap_caps_free(s_buf2);
        s_buf1 = (lv_color_t*)heap_caps_malloc(BUF_PX / 2 * sizeof(lv_color_t), MALLOC_CAP_DMA);
        s_buf2 = nullptr;
        lv_disp_draw_buf_init(&s_drawBuf, s_buf1, nullptr, BUF_PX / 2);
    } else {
        lv_disp_draw_buf_init(&s_drawBuf, s_buf1, s_buf2, BUF_PX);
    }

    lv_disp_drv_init(&s_dispDrv);
    s_dispDrv.hor_res      = SCR_W;
    s_dispDrv.ver_res      = SCR_H;
    s_dispDrv.flush_cb     = flushCb;
    s_dispDrv.draw_buf     = &s_drawBuf;
    s_dispDrv.full_refresh = 0;      // dirty rectangles only
    lv_disp_drv_register(&s_dispDrv);

    lv_indev_drv_init(&s_indevDrv);
    s_indevDrv.type    = LV_INDEV_TYPE_POINTER;
    s_indevDrv.read_cb = touchCb;
    lv_indev_drv_register(&s_indevDrv);

    theme::init();

    // Diagnostic, printed once. A colour that looks wrong on the glass has two
    // possible causes and they need opposite fixes: either LVGL resolved the
    // wrong colour, or it resolved the right one and the panel path mangled it.
    // Asking LVGL what it thinks the ground is separates the two in one line.
    {
        lv_obj_t* scr = lv_scr_act();
        lv_color_t c  = lv_obj_get_style_bg_color(scr, LV_PART_MAIN);
        lv_color_t want = lv_color_hex(theme::BG);
        Serial.printf("[ui] screen bg: raw=0x%04X  r=%u g=%u b=%u   "
                      "expected raw=0x%04X r=%u g=%u b=%u\n",
                      c.full, c.ch.red, c.ch.green, c.ch.blue,
                      want.full, want.ch.red, want.ch.green, want.ch.blue);
        // And what the panel is actually fed for that colour.
        Serial.printf("[ui] LV_COLOR_16_SWAP=%d  LV_COLOR_DEPTH=%d\n",
                      LV_COLOR_16_SWAP, LV_COLOR_DEPTH);
    }
    Serial.printf("[ui] LVGL %d.%d ready - %ux%u, %u KB DMA draw buffer%s\n",
                  LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, SCR_W, SCR_H,
                  (unsigned)((s_buf2 ? 2 : 1) * (s_buf2 ? BUF_PX : BUF_PX / 2)
                             * sizeof(lv_color_t) / 1024),
                  s_buf2 ? " (double)" : " (single)");
}

uint32_t loop() { return lv_timer_handler(); }

uint32_t frameCounter() { return s_frame; }

void requestCapture(bool on) { s_capture = on; }
bool capturing()             { return s_capture; }

void setRotation(int rotation) {
    lcd.setRotation(rotation == 0 ? 0 : 2);
    lv_obj_invalidate(lv_scr_act());
}

void setBacklight(uint8_t percent) {
    if (percent > 100) percent = 100;
    s_backlight = percent;
    lcd.setBrightness(percent == 0 ? 0 : (uint8_t)(percent * 255 / 100));
}

uint8_t backlight() { return s_backlight; }

// Dim, then dark, then wake on the next touch.
//
// Nothing else stops: the reader keeps polling, the printers keep being
// probed, the account keeps syncing. Only the light goes - a box behind a
// printer should not glow at the ceiling all night, and it should not need
// waking up to be working.
void sleepTick(int timeoutSec, uint8_t awakeBrightness) {
    if (!s_lastTouch) s_lastTouch = millis();

    if (timeoutSec <= 0) {                       // "Never"
        if (s_asleep) { s_asleep = false; setBacklight(awakeBrightness); }
        return;
    }

    uint32_t idle = (millis() - s_lastTouch) / 1000;

    if (s_asleep) {
        // The wake is the touch itself, read straight from the panel: the LVGL
        // input driver is reporting released while asleep, on purpose.
        int32_t x, y;
        if (lcd.getTouch(&x, &y)) {
            s_asleep = false;
            s_lastTouch = millis();
            setBacklight(awakeBrightness);
        }
        return;
    }

    if (idle >= (uint32_t)timeoutSec + 5) {
        s_asleep = true;
        setBacklight(0);
    } else if (idle >= (uint32_t)timeoutSec) {
        // A dim step before dark, so the screen announces what it is about to
        // do instead of simply vanishing.
        setBacklight(awakeBrightness / 4 ? awakeBrightness / 4 : 5);
    } else if (s_backlight != awakeBrightness) {
        setBacklight(awakeBrightness);
    }
}

bool asleep() { return s_asleep; }

}  // namespace lvgl_port
