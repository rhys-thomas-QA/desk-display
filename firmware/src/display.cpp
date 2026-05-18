#include "display.h"

#ifdef NO_DISPLAY
void display_init() {}
#else

#include <TFT_eSPI.h>
#include <lvgl.h>

#ifndef DISPLAY_TFT_ROTATION
#define DISPLAY_TFT_ROTATION 3
#endif

constexpr int DISPLAY_W = 320;
constexpr int DISPLAY_H = 240;

static TFT_eSPI tft = TFT_eSPI(DISPLAY_W, DISPLAY_H);

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DISPLAY_W * DISPLAY_BUFFER_ROWS];
static lv_color_t buf2[DISPLAY_W * DISPLAY_BUFFER_ROWS];
static lv_disp_drv_t disp_drv;

static void flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  int32_t x1 = area->x1;
  int32_t y1 = area->y1;
  int32_t x2 = area->x2;
  int32_t y2 = area->y2;
  const uint32_t src_w = area->x2 - area->x1 + 1;

  if (x2 < 0 || y2 < 0 || x1 >= DISPLAY_W || y1 >= DISPLAY_H) {
    lv_disp_flush_ready(drv);
    return;
  }

  if (x1 < 0) {
    color_p += -x1;
    x1 = 0;
  }
  if (y1 < 0) {
    color_p += (-y1) * src_w;
    y1 = 0;
  }
  if (x2 >= DISPLAY_W) x2 = DISPLAY_W - 1;
  if (y2 >= DISPLAY_H) y2 = DISPLAY_H - 1;

  const uint32_t w = x2 - x1 + 1;
  const uint32_t h = y2 - y1 + 1;

  tft.startWrite();
  if (w == src_w) {
    tft.setAddrWindow(x1, y1, w, h);
    tft.pushColors((uint16_t*)&color_p->full, w * h, false);
  } else {
    for (uint32_t row = 0; row < h; row++) {
      tft.setAddrWindow(x1, y1 + row, w, 1);
      tft.pushColors((uint16_t*)&color_p->full, w, false);
      color_p += src_w;
    }
  }
  tft.endWrite();

  lv_disp_flush_ready(drv);
}

void display_init() {
  tft.begin();
  tft.setRotation(DISPLAY_TFT_ROTATION);
  tft.invertDisplay(false);
  tft.fillScreen(TFT_BLACK);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISPLAY_W * DISPLAY_BUFFER_ROWS);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res   = DISPLAY_W;
  disp_drv.ver_res   = DISPLAY_H;
  disp_drv.flush_cb  = flush_cb;
  disp_drv.draw_buf  = &draw_buf;
  lv_disp_drv_register(&disp_drv);
}

#endif // NO_DISPLAY
