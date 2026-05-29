#include "ui.h"
#include "api_client.h"
#include <Arduino.h>

#ifdef NO_DISPLAY

void ui_init() {
  Serial.println("[ui] init (no display)");
}

void ui_update(const DisplayData& d) {
  if (!d.valid) {
    Serial.printf("[ui] error: %s\n", d.statusMsg);
    return;
  }

  Serial.println("+-----------------------------+");
  Serial.printf("|  %-10s  %s\n", d.valueText, d.detailText);
  Serial.print("|  [");
  int bars = d.percent / 4;
  for (int i = 0; i < 25; i++) Serial.print(i < bars ? '#' : '-');
  Serial.printf("] %d%%\n", d.percent);
  Serial.printf("|  %s\n", d.resetText);
  Serial.printf("|  %s\n", d.resetHint);
  Serial.println("+-----------------------------+");
}

void ui_update_openai(const DisplayData& d) {
  if (!d.valid) {
    Serial.printf("[ui] openai error: %s\n", d.statusMsg);
    return;
  }

  Serial.printf("[ui] openai: %s %s (%d%%)\n", d.valueText, d.detailText, d.percent);
}

void ui_update_github_status(const GitHubStatusData& d) {
  if (!d.valid) {
    Serial.printf("[ui] github error: %s\n", d.errorMsg);
    return;
  }

  Serial.printf("[ui] github: %s / %s\n", d.summary, d.affected);
}

void ui_update_info(const InfoData& d) {
  Serial.printf("[ui] info: wifi=%s rssi=%d ip=%s email=%s\n",
                d.wifiConnected ? "connected" : "offline",
                d.rssi,
                d.ipAddress,
                d.email);
}

void ui_show_setup_ap(const char* ssid, const char* password) {
  Serial.printf("[ui] setup ap: ssid=%s password=%s\n", ssid, password);
}

void ui_set_openai_enabled(bool enabled) {
  Serial.printf("[ui] openai screen: %s\n", enabled ? "enabled" : "disabled");
}

void ui_set_wifi_signal(bool connected, int rssi) {
  Serial.printf("[ui] wifi signal: %s %d\n", connected ? "connected" : "offline", rssi);
}

void ui_set_reset_confirmation(bool active, uint8_t percent) {
  if (active) Serial.printf("[ui] reset hold: %u%%\n", (unsigned)percent);
}

void ui_set_status(const char* msg) {
  if (msg && msg[0]) Serial.printf("[ui] status: %s\n", msg);
}

void ui_next_screen() {
  Serial.println("[ui] next screen");
}

#else

#include <lvgl.h>

LV_FONT_DECLARE(poppins_12);
LV_FONT_DECLARE(poppins_16);
LV_FONT_DECLARE(poppins_20);
LV_FONT_DECLARE(lora_24);

#define C_BG       0x050506
#define C_CARD     0x202124
#define C_TRACK    0x65476f
#define C_PILL     0x7a6684
#define C_TEXT     0xfff7ff
#define C_MUTED    0xd8d2de
#define C_MASCOT   0x1633b7
#define C_STATUS   0xffb485
#define C_OK       0x50b93f

#define FONT_SMALL  &poppins_12
#define FONT_BODY   &poppins_16
#define FONT_LARGE  &poppins_20
#define FONT_TITLE  &lora_24

static lv_obj_t* lbl_value;
static lv_obj_t* lbl_detail;
static lv_obj_t* lbl_reset;
static lv_obj_t* lbl_usage_reset_hint;
static lv_obj_t* lbl_status;
static lv_obj_t* lbl_pct;
static lv_obj_t* bar_usage;
static lv_obj_t* spinner;
static lv_obj_t* scr_splash;
static lv_obj_t* scr_reset;
static lv_obj_t* scr_usage;
static lv_obj_t* lbl_openai_value;
static lv_obj_t* lbl_openai_detail;
static lv_obj_t* lbl_openai_reset;
static lv_obj_t* lbl_openai_reset_hint;
static lv_obj_t* lbl_openai_status;
static lv_obj_t* lbl_openai_pct;
static lv_obj_t* bar_openai;
static lv_obj_t* scr_openai;
static lv_obj_t* scr_github;
static lv_obj_t* scr_info;
static lv_obj_t* github_indicator;
static lv_obj_t* lbl_github_indicator;
static lv_obj_t* lbl_github_summary;
static lv_obj_t* lbl_github_affected;
static lv_obj_t* lbl_github_checked;
static lv_obj_t* info_wifi_dot;
static lv_obj_t* lbl_info_wifi;
static lv_obj_t* lbl_info_ip;
static lv_obj_t* lbl_info_email;
static lv_obj_t* reset_bar;
static lv_obj_t* lbl_reset_confirm_detail;
static lv_obj_t* wifi_bars[4][3];
static lv_obj_t* screen_dots[4][4];
static lv_timer_t* s_splash_timer = nullptr;
static uint8_t   s_screen_index = 0;
static bool      s_openai_enabled = false;
static bool      s_splash_active = false;
static bool      s_reset_confirm_active = false;
static uint32_t  s_last_screen_change = 0;

constexpr uint32_t SPLASH_MS = 5000;
constexpr uint32_t SCREEN_ANIM_MS = 180;
constexpr uint32_t SCREEN_CHANGE_GUARD_MS = 260;
constexpr lv_coord_t CONTENT_PAD = 12;
constexpr lv_coord_t PILL_W = 72;
constexpr lv_coord_t RESET_HINT_W = 112;

static lv_coord_t content_w() {
  return lv_disp_get_hor_res(nullptr) >= 300 ? 272 : 228;
}

static lv_coord_t content_inner_w() {
  return content_w() - (CONTENT_PAD * 2);
}

static lv_coord_t pill_x() {
  return content_w() - CONTENT_PAD - PILL_W;
}

static lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, uint32_t color,
                            lv_coord_t width = LV_SIZE_CONTENT,
                            lv_label_long_mode_t mode = LV_LABEL_LONG_DOT) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
  if (width != LV_SIZE_CONTENT) lv_obj_set_width(lbl, width);
  lv_label_set_long_mode(lbl, mode);
  return lbl;
}

static void style_panel(lv_obj_t* obj, uint32_t bg, uint32_t border, int radius) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(bg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(obj, lv_color_hex(border), LV_PART_MAIN);
  lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void style_pill(lv_obj_t* obj) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(C_PILL), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(obj, 14, LV_PART_MAIN);
  lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t* make_rect(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                           lv_coord_t w, lv_coord_t h, uint32_t color,
                           int radius = 0) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
  lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  return obj;
}

constexpr int MASCOT_COLS = 30;
constexpr int MASCOT_ROWS = 30;

struct MascotRun {
  uint8_t x;
  uint8_t y;
  uint8_t w;
};

struct MascotAnim {
  lv_obj_t* container;
  lv_obj_t* left_lid;
  lv_obj_t* right_lid;
  uint8_t blink_ticks;
  uint16_t blink_countdown;
};

constexpr uint8_t MAX_MASCOTS = 5;
static MascotAnim s_mascots[MAX_MASCOTS];
static uint8_t s_mascot_count = 0;
static lv_timer_t* s_mascot_timer = nullptr;
static uint8_t s_blink_seed = 0;

static const MascotRun MASCOT_RUNS[] = {
  {6, 5, 17},
  {6, 6, 17},
  {6, 7, 17},
  {6, 8, 17},
  {6, 9, 4},
  {11, 9, 7},
  {19, 9, 4},
  {3, 10, 7},
  {11, 10, 7},
  {19, 10, 7},
  {3, 11, 7},
  {11, 11, 7},
  {19, 11, 7},
  {3, 12, 23},
  {3, 13, 23},
  {3, 14, 23},
  {6, 15, 17},
  {6, 16, 17},
  {6, 17, 17},
  {7, 18, 2},
  {10, 18, 2},
  {17, 18, 2},
  {20, 18, 2},
  {7, 19, 2},
  {10, 19, 2},
  {17, 19, 2},
  {20, 19, 2},
  {7, 20, 2},
  {10, 20, 2},
  {17, 20, 2},
  {20, 20, 2},
};

static void mascot_set_y(void* obj, int32_t y) {
  lv_obj_set_y((lv_obj_t*)obj, (lv_coord_t)y);
}

static void mascot_blink_timer(lv_timer_t*) {
  for (uint8_t i = 0; i < s_mascot_count; i++) {
    MascotAnim& m = s_mascots[i];
    if (!m.container || !m.left_lid || !m.right_lid) continue;

    if (m.blink_ticks > 0) {
      m.blink_ticks--;
      if (m.blink_ticks == 0) {
        lv_obj_add_flag(m.left_lid, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(m.right_lid, LV_OBJ_FLAG_HIDDEN);
      }
      continue;
    }

    if (m.blink_countdown > 0) {
      m.blink_countdown--;
      continue;
    }

    lv_obj_clear_flag(m.left_lid, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(m.right_lid, LV_OBJ_FLAG_HIDDEN);
    m.blink_ticks = 1;
    s_blink_seed += 7;
    m.blink_countdown = 28 + (s_blink_seed % 22) + (i * 5);
  }
}

static void animate_mascot(lv_obj_t* container, lv_obj_t* left_lid, lv_obj_t* right_lid,
                           lv_coord_t base_y) {
  if (s_mascot_count < MAX_MASCOTS) {
    MascotAnim& m = s_mascots[s_mascot_count];
    m.container = container;
    m.left_lid = left_lid;
    m.right_lid = right_lid;
    m.blink_ticks = 0;
    m.blink_countdown = 16 + (s_mascot_count * 9);
    s_mascot_count++;
  }

  if (!s_mascot_timer) {
    s_mascot_timer = lv_timer_create(mascot_blink_timer, 120, nullptr);
  }

  lv_anim_t bob;
  lv_anim_init(&bob);
  lv_anim_set_var(&bob, container);
  lv_anim_set_exec_cb(&bob, mascot_set_y);
  lv_anim_set_values(&bob, base_y, base_y + 5);
  lv_anim_set_time(&bob, 1450);
  lv_anim_set_playback_time(&bob, 1450);
  lv_anim_set_path_cb(&bob, lv_anim_path_ease_in_out);
  lv_anim_set_repeat_count(&bob, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_delay(&bob, s_mascot_count * 160);
  lv_anim_start(&bob);
}

static lv_obj_t* create_claude_mascot(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                      lv_coord_t pixel_size) {
  lv_obj_t* container = lv_obj_create(parent);
  lv_obj_remove_style_all(container);
  lv_obj_set_size(container, MASCOT_COLS * pixel_size, MASCOT_ROWS * pixel_size);
  lv_obj_set_pos(container, x, y);
  lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
  lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

  for (size_t i = 0; i < sizeof(MASCOT_RUNS) / sizeof(MASCOT_RUNS[0]); i++) {
    const MascotRun& run = MASCOT_RUNS[i];
    make_rect(container, run.x * pixel_size, run.y * pixel_size,
              run.w * pixel_size, pixel_size, C_MASCOT);
  }

  lv_obj_t* left_lid = make_rect(container, 10 * pixel_size, 9 * pixel_size,
                                 pixel_size, 3 * pixel_size, C_MASCOT);
  lv_obj_t* right_lid = make_rect(container, 18 * pixel_size, 9 * pixel_size,
                                  pixel_size, 3 * pixel_size, C_MASCOT);
  lv_obj_add_flag(left_lid, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(right_lid, LV_OBJ_FLAG_HIDDEN);
  animate_mascot(container, left_lid, right_lid, y);

  return container;
}

static lv_obj_t* make_pill_label(lv_obj_t* parent, const char* text, lv_coord_t x, lv_coord_t y, lv_coord_t w) {
  lv_obj_t* pill = lv_obj_create(parent);
  lv_obj_set_size(pill, w, 28);
  lv_obj_set_pos(pill, x, y);
  style_pill(pill);

  lv_obj_t* lbl = make_label(pill, FONT_BODY, C_TEXT, w);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl, text);
  lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
  return pill;
}

static void style_bar(lv_obj_t* bar, uint32_t indicator) {
  lv_bar_set_range(bar, 0, 100);
  lv_bar_set_value(bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, lv_color_hex(C_TRACK), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, lv_color_hex(indicator), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar, 5, LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 5, LV_PART_INDICATOR);
  lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
}

static void style_screen(lv_obj_t* scr) {
  lv_obj_set_size(scr, lv_disp_get_hor_res(nullptr), lv_disp_get_ver_res(nullptr));
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
}

static void add_wifi_indicator(lv_obj_t* scr, uint8_t screen_index) {
  const lv_coord_t x = lv_disp_get_hor_res(nullptr) - 28;
  const lv_coord_t y = 15;
  for (uint8_t i = 0; i < 3; i++) {
    lv_coord_t h = 5 + (i * 4);
    wifi_bars[screen_index][i] = make_rect(scr, x + (i * 7), y + (13 - h),
                                           5, h, C_CARD, 1);
    lv_obj_set_style_bg_opa(wifi_bars[screen_index][i], LV_OPA_60, LV_PART_MAIN);
  }
}

static uint8_t wifi_level_from_rssi(bool connected, int rssi) {
  if (!connected) return 0;
  if (rssi >= -60) return 3;
  if (rssi >= -72) return 2;
  return 1;
}

static uint8_t visible_screen_count() {
  return s_openai_enabled ? 4 : 3;
}

static int visible_dot_index(uint8_t screen_index) {
  if (screen_index == 0) return 0;
  if (screen_index == 1) return s_openai_enabled ? 1 : -1;
  if (screen_index == 2) return s_openai_enabled ? 2 : 1;
  if (screen_index == 3) return s_openai_enabled ? 3 : 2;
  return 0;
}

static void add_screen_dots(lv_obj_t* scr, uint8_t screen_index) {
  for (uint8_t i = 0; i < 4; i++) {
    screen_dots[screen_index][i] = make_rect(scr, 0, 0, 6, 6, C_CARD, 3);
    lv_obj_set_style_bg_opa(screen_dots[screen_index][i], LV_OPA_70, LV_PART_MAIN);
  }
}

static void update_screen_dots() {
  const uint8_t count = visible_screen_count();
  const int active = visible_dot_index(s_screen_index);
  const lv_coord_t dot = 6;
  const lv_coord_t gap = 7;
  const lv_coord_t total_w = count * dot + (count - 1) * gap;
  const lv_coord_t x0 = (lv_disp_get_hor_res(nullptr) - total_w) / 2;
  const lv_coord_t y = lv_disp_get_ver_res(nullptr) - 11;

  for (uint8_t screen = 0; screen < 4; screen++) {
    for (uint8_t i = 0; i < 4; i++) {
      lv_obj_t* d = screen_dots[screen][i];
      if (!d) continue;

      if (i >= count) {
        lv_obj_add_flag(d, LV_OBJ_FLAG_HIDDEN);
        continue;
      }

      lv_obj_clear_flag(d, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(d, x0 + i * (dot + gap), y);
      lv_obj_set_style_bg_color(d,
                                lv_color_hex(i == active ? C_MASCOT : C_CARD),
                                LV_PART_MAIN);
      lv_obj_set_style_bg_opa(d, i == active ? LV_OPA_COVER : LV_OPA_70, LV_PART_MAIN);
    }
  }
}

static void splash_timeout_cb(lv_timer_t* timer) {
  if (timer == s_splash_timer) s_splash_timer = nullptr;
  if (!s_splash_active || !scr_usage) return;

  s_splash_active = false;
  s_last_screen_change = millis();
  lv_scr_load_anim(scr_usage, LV_SCR_LOAD_ANIM_FADE_ON, 320, 0, false);
}

static void finish_splash_now(bool animate) {
  if (!s_splash_active) return;

  if (s_splash_timer) {
    lv_timer_del(s_splash_timer);
    s_splash_timer = nullptr;
  }

  s_splash_active = false;
  s_last_screen_change = millis();
  if (animate) {
    lv_scr_load_anim(scr_usage, LV_SCR_LOAD_ANIM_FADE_ON, 220, 0, false);
  } else {
    lv_scr_load(scr_usage);
  }
}

static void build_splash_screen(lv_obj_t* scr) {
  style_screen(scr);

  const lv_coord_t mascot_size = 3;
  const lv_coord_t mascot_w = MASCOT_COLS * mascot_size;
  const lv_coord_t mascot_x = (lv_disp_get_hor_res(nullptr) - mascot_w) / 2;
  create_claude_mascot(scr, mascot_x, 40, mascot_size);

  lv_obj_t* lbl_title = make_label(scr, FONT_TITLE, C_TEXT, content_w());
  lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_title, "Desk Display");
  lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 142);

  lv_obj_t* lbl_subtitle = make_label(scr, FONT_SMALL, C_MUTED, content_w());
  lv_obj_set_style_text_align(lbl_subtitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_subtitle, "Starting up");
  lv_obj_align(lbl_subtitle, LV_ALIGN_TOP_MID, 0, 176);
}

static void build_reset_screen(lv_obj_t* scr) {
  style_screen(scr);

  lv_obj_t* lbl_title = make_label(scr, FONT_TITLE, C_TEXT, content_w());
  lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_title, "Reset setup?");
  lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 42);

  lv_obj_t* lbl_body = make_label(scr, FONT_BODY, C_TEXT, content_w(), LV_LABEL_LONG_WRAP);
  lv_obj_set_height(lbl_body, 54);
  lv_obj_set_style_text_align(lbl_body, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_body, "Keep holding to clear WiFi, email, and API keys.");
  lv_obj_align(lbl_body, LV_ALIGN_TOP_MID, 0, 84);

  reset_bar = lv_bar_create(scr);
  lv_obj_set_size(reset_bar, content_w(), 16);
  lv_obj_align(reset_bar, LV_ALIGN_TOP_MID, 0, 150);
  style_bar(reset_bar, C_STATUS);

  lbl_reset_confirm_detail = make_label(scr, FONT_SMALL, C_MUTED, content_w());
  lv_obj_set_style_text_align(lbl_reset_confirm_detail, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_reset_confirm_detail, "Release to cancel");
  lv_obj_align(lbl_reset_confirm_detail, LV_ALIGN_TOP_MID, 0, 178);
}

static void build_usage_screen(lv_obj_t* scr) {
  style_screen(scr);
  add_wifi_indicator(scr, 0);
  add_screen_dots(scr, 0);

  create_claude_mascot(scr, 0, 2, 2);

  lv_obj_t* lbl_title = make_label(scr, FONT_TITLE, C_TEXT, 120);
  lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_title, "Usage");
  lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 14);

  lv_obj_t* card = lv_obj_create(scr);
  lv_obj_set_size(card, content_w(), 138);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 62);
  style_panel(card, C_CARD, C_CARD, 5);
  lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);

  lbl_pct = make_label(card, FONT_LARGE, C_TEXT, 82);
  lv_label_set_text(lbl_pct, "--%");
  lv_obj_set_pos(lbl_pct, CONTENT_PAD, 4);

  make_pill_label(card, "Used", pill_x(), 10, PILL_W);

  lbl_value = make_label(card, FONT_BODY, C_TEXT, content_inner_w());
  lv_label_set_text(lbl_value, "--");
  lv_obj_set_pos(lbl_value, CONTENT_PAD, 40);

  bar_usage = lv_bar_create(card);
  lv_obj_set_size(bar_usage, content_inner_w(), 18);
  lv_obj_set_pos(bar_usage, CONTENT_PAD, 66);
  style_bar(bar_usage, C_MASCOT);

  lbl_reset = make_label(card, FONT_BODY, C_TEXT, content_inner_w());
  lv_label_set_text(lbl_reset, "");
  lv_obj_set_pos(lbl_reset, CONTENT_PAD, 90);

  lbl_detail = make_label(card, FONT_SMALL, C_MUTED, content_inner_w() - RESET_HINT_W);
  lv_label_set_text(lbl_detail, "");
  lv_obj_set_pos(lbl_detail, CONTENT_PAD, 116);

  lbl_usage_reset_hint = make_label(card, FONT_SMALL, C_MUTED, RESET_HINT_W);
  lv_obj_set_style_text_align(lbl_usage_reset_hint, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  lv_label_set_text(lbl_usage_reset_hint, "");
  lv_obj_set_pos(lbl_usage_reset_hint, CONTENT_PAD + content_inner_w() - RESET_HINT_W, 116);

  lbl_status = make_label(scr, FONT_BODY, C_STATUS, content_w());
  lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_long_mode(lbl_status, LV_LABEL_LONG_WRAP);
  lv_obj_set_height(lbl_status, 38);
  lv_label_set_text(lbl_status, "");
  lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_MID, 0, -20);

  spinner = make_rect(scr, 0, 0, 14, 14, C_STATUS, 7);
  lv_obj_align(spinner, LV_ALIGN_BOTTOM_RIGHT, -14, -14);
  lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
}

static void build_openai_screen(lv_obj_t* scr) {
  style_screen(scr);
  add_wifi_indicator(scr, 1);
  add_screen_dots(scr, 1);

  create_claude_mascot(scr, 0, 2, 2);

  lv_obj_t* lbl_title = make_label(scr, FONT_TITLE, C_TEXT, 150);
  lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_title, "OpenAI");
  lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 14);

  lv_obj_t* card = lv_obj_create(scr);
  lv_obj_set_size(card, content_w(), 138);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 62);
  style_panel(card, C_CARD, C_CARD, 5);
  lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);

  lbl_openai_pct = make_label(card, FONT_LARGE, C_TEXT, 82);
  lv_label_set_text(lbl_openai_pct, "--%");
  lv_obj_set_pos(lbl_openai_pct, CONTENT_PAD, 4);

  make_pill_label(card, "Used", pill_x(), 10, PILL_W);

  lbl_openai_value = make_label(card, FONT_BODY, C_TEXT, content_inner_w());
  lv_label_set_text(lbl_openai_value, "--");
  lv_obj_set_pos(lbl_openai_value, CONTENT_PAD, 40);

  bar_openai = lv_bar_create(card);
  lv_obj_set_size(bar_openai, content_inner_w(), 18);
  lv_obj_set_pos(bar_openai, CONTENT_PAD, 66);
  style_bar(bar_openai, C_MASCOT);

  lbl_openai_reset = make_label(card, FONT_BODY, C_TEXT, content_inner_w());
  lv_label_set_text(lbl_openai_reset, "");
  lv_obj_set_pos(lbl_openai_reset, CONTENT_PAD, 90);

  lbl_openai_detail = make_label(card, FONT_SMALL, C_MUTED, content_inner_w() - RESET_HINT_W);
  lv_label_set_text(lbl_openai_detail, "");
  lv_obj_set_pos(lbl_openai_detail, CONTENT_PAD, 116);

  lbl_openai_reset_hint = make_label(card, FONT_SMALL, C_MUTED, RESET_HINT_W);
  lv_obj_set_style_text_align(lbl_openai_reset_hint, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  lv_label_set_text(lbl_openai_reset_hint, "");
  lv_obj_set_pos(lbl_openai_reset_hint, CONTENT_PAD + content_inner_w() - RESET_HINT_W, 116);

  lbl_openai_status = make_label(scr, FONT_BODY, C_STATUS, content_w());
  lv_obj_set_style_text_align(lbl_openai_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_long_mode(lbl_openai_status, LV_LABEL_LONG_WRAP);
  lv_obj_set_height(lbl_openai_status, 38);
  lv_label_set_text(lbl_openai_status, "");
  lv_obj_align(lbl_openai_status, LV_ALIGN_BOTTOM_MID, 0, -20);
}

static void build_github_screen(lv_obj_t* scr) {
  style_screen(scr);
  add_wifi_indicator(scr, 2);
  add_screen_dots(scr, 2);

  create_claude_mascot(scr, 0, 2, 2);

  lv_obj_t* lbl_title = make_label(scr, FONT_TITLE, C_TEXT, 140);
  lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_title, "GitHub");
  lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 14);

  github_indicator = lv_obj_create(scr);
  lv_obj_set_size(github_indicator, 66, 66);
  lv_obj_align(github_indicator, LV_ALIGN_TOP_MID, 0, 54);
  style_panel(github_indicator, C_CARD, C_CARD, 33);
  lv_obj_set_style_border_width(github_indicator, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(github_indicator, 0, LV_PART_MAIN);

  lbl_github_indicator = make_label(github_indicator, FONT_LARGE, C_TEXT, 58);
  lv_obj_set_style_text_align(lbl_github_indicator, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_github_indicator, "--");
  lv_obj_align(lbl_github_indicator, LV_ALIGN_CENTER, 0, 0);

  lbl_github_summary = make_label(scr, FONT_LARGE, C_TEXT, content_w());
  lv_obj_set_style_text_align(lbl_github_summary, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_github_summary, "Checking GitHub");
  lv_obj_align(lbl_github_summary, LV_ALIGN_TOP_MID, 0, 130);

  lbl_github_affected = make_label(scr, FONT_SMALL, C_MUTED, content_w(), LV_LABEL_LONG_WRAP);
  lv_obj_set_height(lbl_github_affected, 42);
  lv_obj_set_style_text_align(lbl_github_affected, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_github_affected, "Status will appear here");
  lv_obj_align(lbl_github_affected, LV_ALIGN_TOP_MID, 0, 164);

  lbl_github_checked = make_label(scr, FONT_SMALL, C_MUTED, content_w());
  lv_obj_set_style_text_align(lbl_github_checked, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_github_checked, "");
  lv_obj_align(lbl_github_checked, LV_ALIGN_BOTTOM_MID, 0, -24);
}

static void make_info_row(lv_obj_t* parent, const char* label, lv_coord_t y,
                          lv_obj_t** value_out, lv_coord_t value_width) {
  lv_obj_t* lbl = make_label(parent, FONT_SMALL, C_MUTED, value_width);
  lv_label_set_text(lbl, label);
  lv_obj_set_pos(lbl, 12, y);

  lv_obj_t* value = make_label(parent, FONT_BODY, C_TEXT, value_width);
  lv_label_set_text(value, "--");
  lv_obj_set_pos(value, 12, y + 18);
  if (value_out) *value_out = value;
}

static void build_info_screen(lv_obj_t* scr) {
  style_screen(scr);
  add_wifi_indicator(scr, 3);
  add_screen_dots(scr, 3);

  create_claude_mascot(scr, 0, 2, 2);

  lv_obj_t* lbl_title = make_label(scr, FONT_TITLE, C_TEXT, 140);
  lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_title, "Info");
  lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 14);

  lv_obj_t* panel = lv_obj_create(scr);
  lv_obj_set_size(panel, content_w(), 154);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 62);
  style_panel(panel, C_CARD, C_CARD, 5);
  lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);

  make_info_row(panel, "WiFi", 10, &lbl_info_wifi, content_inner_w() - 40);
  info_wifi_dot = make_rect(panel, content_w() - CONTENT_PAD - 14, 26, 14, 14, C_STATUS, 7);
  make_rect(panel, CONTENT_PAD, 52, content_inner_w(), 1, 0x343038);

  make_info_row(panel, "IP address", 60, &lbl_info_ip, content_inner_w());
  make_rect(panel, CONTENT_PAD, 102, content_inner_w(), 1, 0x343038);

  lbl_info_email = nullptr;
  make_info_row(panel, "Claude email", 110, &lbl_info_email, content_inner_w());
  lv_label_set_long_mode(lbl_info_email, LV_LABEL_LONG_SCROLL_CIRCULAR);
}

void ui_init() {
  scr_splash = lv_obj_create(nullptr);
  scr_reset = lv_obj_create(nullptr);
  scr_usage = lv_obj_create(nullptr);
  scr_openai = lv_obj_create(nullptr);
  scr_github = lv_obj_create(nullptr);
  scr_info = lv_obj_create(nullptr);

  build_splash_screen(scr_splash);
  build_reset_screen(scr_reset);
  build_usage_screen(scr_usage);
  build_openai_screen(scr_openai);
  build_github_screen(scr_github);
  build_info_screen(scr_info);

  s_screen_index = 0;
  s_last_screen_change = millis();
  update_screen_dots();
  s_splash_active = true;
  s_splash_timer = lv_timer_create(splash_timeout_cb, SPLASH_MS, nullptr);
  lv_timer_set_repeat_count(s_splash_timer, 1);
  lv_scr_load(scr_splash);
  Serial.println("Startup splash active");
}

static lv_obj_t* screen_for_index(uint8_t index) {
  if (index == 1 && s_openai_enabled) return scr_openai;
  if (index == 2) return scr_github;
  if (index == 3) return scr_info;
  return scr_usage;
}

void ui_set_openai_enabled(bool enabled) {
  s_openai_enabled = enabled;
  if (!enabled && s_screen_index == 1) {
    s_screen_index = 0;
    lv_scr_load(scr_usage);
  }
  update_screen_dots();
  Serial.printf("OpenAI screen %s\n", enabled ? "enabled" : "disabled");
}

void ui_set_wifi_signal(bool connected, int rssi) {
  const uint8_t level = wifi_level_from_rssi(connected, rssi);
  for (uint8_t screen = 0; screen < 4; screen++) {
    for (uint8_t i = 0; i < 3; i++) {
      lv_obj_t* bar = wifi_bars[screen][i];
      if (!bar) continue;
      const bool active = connected && i < level;
      lv_obj_set_style_bg_color(bar,
                                lv_color_hex(active ? C_OK : C_CARD),
                                LV_PART_MAIN);
      lv_obj_set_style_bg_opa(bar, active ? LV_OPA_COVER : LV_OPA_60, LV_PART_MAIN);
    }
  }
}

void ui_set_reset_confirmation(bool active, uint8_t percent) {
  if (!scr_reset || !reset_bar) return;

  if (!active) {
    lv_bar_set_value(reset_bar, 0, LV_ANIM_OFF);
    lv_label_set_text(lbl_reset_confirm_detail, "Release to cancel");
    if (s_reset_confirm_active) {
      s_reset_confirm_active = false;
      lv_obj_t* target = s_splash_active ? scr_splash : screen_for_index(s_screen_index);
      lv_scr_load(target);
    }
    return;
  }

  if (percent > 100) percent = 100;
  if (!s_reset_confirm_active && lv_scr_act() != scr_reset) {
    s_reset_confirm_active = true;
    lv_scr_load(scr_reset);
  }
  lv_bar_set_value(reset_bar, percent, LV_ANIM_OFF);
  lv_label_set_text(lbl_reset_confirm_detail,
                    percent >= 100 ? "Resetting..." : "Release to cancel");
}

void ui_update(const DisplayData& d) {
  lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(lbl_status, "");

  if (!d.valid) {
    lv_label_set_text(lbl_pct, "--%");
    lv_label_set_text(lbl_value, "--");
    lv_label_set_text(lbl_reset, "");
    lv_label_set_text(lbl_detail, "");
    lv_label_set_text(lbl_usage_reset_hint, "");
    lv_label_set_text(lbl_status, d.statusMsg);
    lv_bar_set_value(bar_usage, 0, LV_ANIM_OFF);
    return;
  }

  lv_bar_set_value(bar_usage, d.percent, LV_ANIM_ON);
  lv_obj_set_style_bg_color(bar_usage, lv_color_hex(C_MASCOT), LV_PART_INDICATOR);

  char pctBuf[8];
  snprintf(pctBuf, sizeof(pctBuf), "%u%%", (unsigned)d.percent);
  lv_label_set_text(lbl_pct, pctBuf);

  char valueBuf[40];
  snprintf(valueBuf, sizeof(valueBuf), "%s %s used", d.valueText, d.detailText);
  lv_label_set_text(lbl_value, valueBuf);

  lv_label_set_text(lbl_reset, d.resetText);
  lv_label_set_text(lbl_detail, "monthly used");
  lv_label_set_text(lbl_usage_reset_hint, d.resetHint);
  lv_label_set_text(lbl_status, "");
}

void ui_update_openai(const DisplayData& d) {
  if (!lbl_openai_pct || !bar_openai) return;

  lv_label_set_text(lbl_openai_status, "");

  if (!d.valid) {
    lv_label_set_text(lbl_openai_pct, "--%");
    lv_label_set_text(lbl_openai_value, "--");
    lv_label_set_text(lbl_openai_reset, "");
    lv_label_set_text(lbl_openai_detail, "");
    lv_label_set_text(lbl_openai_reset_hint, "");
    lv_label_set_text(lbl_openai_status, d.statusMsg);
    lv_bar_set_value(bar_openai, 0, LV_ANIM_OFF);
    return;
  }

  lv_bar_set_value(bar_openai, d.percent, LV_ANIM_ON);
  lv_obj_set_style_bg_color(bar_openai, lv_color_hex(C_MASCOT), LV_PART_INDICATOR);

  char pctBuf[8];
  snprintf(pctBuf, sizeof(pctBuf), "%u%%", (unsigned)d.percent);
  lv_label_set_text(lbl_openai_pct, pctBuf);

  char valueBuf[40];
  snprintf(valueBuf, sizeof(valueBuf), "%s %s used", d.valueText, d.detailText);
  lv_label_set_text(lbl_openai_value, valueBuf);

  lv_label_set_text(lbl_openai_reset, d.resetText);
  lv_label_set_text(lbl_openai_detail, "OpenAI spend");
  lv_label_set_text(lbl_openai_reset_hint, d.resetHint);
}

void ui_update_github_status(const GitHubStatusData& d) {
  if (!github_indicator || !lbl_github_indicator) return;

  if (!d.valid) {
    lv_obj_set_style_bg_color(github_indicator, lv_color_hex(C_STATUS), LV_PART_MAIN);
    lv_label_set_text(lbl_github_indicator, "!");
    lv_label_set_text(lbl_github_summary, "GitHub unavailable");
    lv_label_set_text(lbl_github_affected, d.errorMsg[0] ? d.errorMsg : "Status check failed");
    lv_obj_clear_flag(lbl_github_affected, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lbl_github_checked, "");
    return;
  }

  lv_obj_set_style_bg_color(github_indicator,
                            lv_color_hex(d.degraded ? C_MASCOT : C_OK),
                            LV_PART_MAIN);
  lv_label_set_text(lbl_github_indicator, d.degraded ? "!" : "OK");
  lv_label_set_text(lbl_github_summary, d.degraded ? "GitHub degraded" : d.summary);
  lv_label_set_text(lbl_github_affected, d.affected[0] ? d.affected : "Details unavailable");
  if (d.degraded || d.affected[0]) {
    lv_obj_clear_flag(lbl_github_affected, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(lbl_github_affected, LV_OBJ_FLAG_HIDDEN);
  }
  lv_label_set_text(lbl_github_checked, d.checkedAt);
}

void ui_update_info(const InfoData& d) {
  if (!lbl_info_wifi || !lbl_info_ip || !lbl_info_email) return;

  ui_set_wifi_signal(d.wifiConnected, d.rssi);

  lv_obj_set_style_bg_color(info_wifi_dot,
                            lv_color_hex(d.wifiConnected ? C_OK : C_STATUS),
                            LV_PART_MAIN);
  char wifiText[28];
  if (d.wifiConnected) {
    const char* quality = d.rssi >= -60 ? "Strong" : (d.rssi >= -72 ? "Good" : "Weak");
    snprintf(wifiText, sizeof(wifiText), "%s %d dBm", quality, d.rssi);
  } else {
    strlcpy(wifiText, "Offline", sizeof(wifiText));
  }
  lv_label_set_text(lbl_info_wifi, wifiText);
  lv_label_set_text(lbl_info_ip, d.wifiConnected ? d.ipAddress : "Unavailable");
  lv_label_set_text(lbl_info_email, d.email[0] ? d.email : "Unavailable");
}

void ui_show_setup_ap(const char* ssid, const char* password) {
  finish_splash_now(false);

  if (scr_usage && lv_scr_act() != scr_usage) {
    s_screen_index = 0;
    lv_scr_load(scr_usage);
  }

  lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(lbl_pct, "Setup");
  lv_label_set_text(lbl_value, ssid && ssid[0] ? ssid : "DeskDisplay setup WiFi");

  char passText[32];
  snprintf(passText, sizeof(passText), "Pass: %s",
           password && password[0] ? password : "none");
  lv_label_set_text(lbl_reset, passText);
  lv_label_set_text(lbl_detail, "one-time code");
  lv_label_set_text(lbl_usage_reset_hint, "192.168.4.1");
  lv_label_set_text(lbl_status, "Join setup WiFi, then open 192.168.4.1");
  lv_bar_set_value(bar_usage, 0, LV_ANIM_OFF);
}

void ui_set_status(const char* msg) {
  if (!msg || !msg[0]) {
    lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lbl_status, "");
    return;
  }

  lv_obj_clear_flag(spinner, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(lbl_status, msg);
}

void ui_next_screen() {
  const uint32_t now = millis();
  if (s_splash_active) {
    finish_splash_now(true);
    return;
  }

  if (!scr_usage || !scr_openai || !scr_github || !scr_info ||
      now - s_last_screen_change < SCREEN_CHANGE_GUARD_MS) {
    Serial.println("Screen switch ignored");
    return;
  }

  s_last_screen_change = now;
  do {
    s_screen_index = (s_screen_index + 1) % 4;
  } while (s_screen_index == 1 && !s_openai_enabled);

  lv_obj_t* next = screen_for_index(s_screen_index);
  update_screen_dots();
  Serial.printf("Switching to screen %u\n", s_screen_index);
  lv_scr_load_anim(next, LV_SCR_LOAD_ANIM_OVER_LEFT, SCREEN_ANIM_MS, 0, false);
}

#endif // NO_DISPLAY
