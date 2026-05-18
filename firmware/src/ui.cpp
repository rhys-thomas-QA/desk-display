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
  Serial.println("+-----------------------------+");
}

void ui_update_github_status(const GitHubStatusData& d) {
  if (!d.valid) {
    Serial.printf("[ui] github error: %s\n", d.errorMsg);
    return;
  }

  Serial.printf("[ui] github: %s / %s\n", d.summary, d.affected);
}

void ui_update_info(const InfoData& d) {
  Serial.printf("[ui] info: wifi=%s ip=%s email=%s\n",
                d.wifiConnected ? "connected" : "offline",
                d.ipAddress,
                d.email);
}

void ui_set_status(const char* msg) {
  Serial.printf("[ui] status: %s\n", msg);
}

void ui_next_screen() {
  Serial.println("[ui] next screen");
}

#else

#include <lvgl.h>

#define C_BG       0x050506
#define C_CARD     0x202124
#define C_TRACK    0x65476f
#define C_PILL     0x7a6684
#define C_TEXT     0xfff7ff
#define C_MUTED    0xd8d2de
#define C_MASCOT   0x1633b7
#define C_STATUS   0xffb485
#define C_OK       0x50b93f

static lv_obj_t* lbl_value;
static lv_obj_t* lbl_detail;
static lv_obj_t* lbl_reset;
static lv_obj_t* lbl_status;
static lv_obj_t* lbl_pct;
static lv_obj_t* bar_usage;
static lv_obj_t* spinner;
static lv_obj_t* scr_usage;
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
static uint8_t   s_screen_index = 0;
static uint32_t  s_last_screen_change = 0;

constexpr uint32_t SCREEN_ANIM_MS = 180;
constexpr uint32_t SCREEN_CHANGE_GUARD_MS = 260;
constexpr lv_coord_t CONTENT_PAD = 12;
constexpr lv_coord_t PILL_W = 72;

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

constexpr uint8_t MAX_MASCOTS = 3;
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

  lv_obj_t* lbl = make_label(pill, &lv_font_montserrat_16, C_TEXT, w);
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

static void build_usage_screen(lv_obj_t* scr) {
  style_screen(scr);

  create_claude_mascot(scr, 0, 2, 2);

  lv_obj_t* lbl_title = make_label(scr, &lv_font_montserrat_20, C_TEXT, 120);
  lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_title, "Usage");
  lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 14);

  lv_obj_t* card = lv_obj_create(scr);
  lv_obj_set_size(card, content_w(), 138);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 62);
  style_panel(card, C_CARD, C_CARD, 5);
  lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);

  lbl_pct = make_label(card, &lv_font_montserrat_20, C_TEXT, 82);
  lv_label_set_text(lbl_pct, "--%");
  lv_obj_set_pos(lbl_pct, CONTENT_PAD, 4);

  make_pill_label(card, "Used", pill_x(), 10, PILL_W);

  lbl_value = make_label(card, &lv_font_montserrat_16, C_TEXT, content_inner_w());
  lv_label_set_text(lbl_value, "--");
  lv_obj_set_pos(lbl_value, CONTENT_PAD, 40);

  bar_usage = lv_bar_create(card);
  lv_obj_set_size(bar_usage, content_inner_w(), 18);
  lv_obj_set_pos(bar_usage, CONTENT_PAD, 66);
  style_bar(bar_usage, C_MASCOT);

  lbl_reset = make_label(card, &lv_font_montserrat_16, C_TEXT, content_inner_w());
  lv_label_set_text(lbl_reset, "");
  lv_obj_set_pos(lbl_reset, CONTENT_PAD, 90);

  lbl_detail = make_label(card, &lv_font_montserrat_12, C_MUTED, content_inner_w());
  lv_label_set_text(lbl_detail, "");
  lv_obj_set_pos(lbl_detail, CONTENT_PAD, 116);

  lbl_status = make_label(scr, &lv_font_montserrat_16, C_STATUS, content_w());
  lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_status, "");
  lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_MID, 0, -12);

  spinner = make_rect(scr, 0, 0, 14, 14, C_STATUS, 7);
  lv_obj_align(spinner, LV_ALIGN_BOTTOM_RIGHT, -14, -14);
  lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
}

static void build_github_screen(lv_obj_t* scr) {
  style_screen(scr);

  create_claude_mascot(scr, 0, 2, 2);

  lv_obj_t* lbl_title = make_label(scr, &lv_font_montserrat_20, C_TEXT, 140);
  lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_title, "GitHub");
  lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 14);

  github_indicator = lv_obj_create(scr);
  lv_obj_set_size(github_indicator, 86, 86);
  lv_obj_align(github_indicator, LV_ALIGN_CENTER, 0, -24);
  style_panel(github_indicator, C_CARD, C_CARD, 43);
  lv_obj_set_style_border_width(github_indicator, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(github_indicator, 0, LV_PART_MAIN);

  lbl_github_indicator = make_label(github_indicator, &lv_font_montserrat_20, C_TEXT, 70);
  lv_obj_set_style_text_align(lbl_github_indicator, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_github_indicator, "--");
  lv_obj_align(lbl_github_indicator, LV_ALIGN_CENTER, 0, 0);

  lbl_github_summary = make_label(scr, &lv_font_montserrat_20, C_TEXT, content_w());
  lv_obj_set_style_text_align(lbl_github_summary, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_github_summary, "Checking GitHub");
  lv_obj_align(lbl_github_summary, LV_ALIGN_CENTER, 0, 42);

  lbl_github_affected = make_label(scr, &lv_font_montserrat_16, C_MUTED, content_w(), LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(lbl_github_affected, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_github_affected, "Status will appear here");
  lv_obj_align(lbl_github_affected, LV_ALIGN_CENTER, 0, 82);

  lbl_github_checked = make_label(scr, &lv_font_montserrat_12, C_MUTED, content_w());
  lv_obj_set_style_text_align(lbl_github_checked, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_label_set_text(lbl_github_checked, "");
  lv_obj_align(lbl_github_checked, LV_ALIGN_BOTTOM_MID, 0, -14);
}

static void make_info_row(lv_obj_t* parent, const char* label, lv_coord_t y,
                          lv_obj_t** value_out, lv_coord_t value_width) {
  lv_obj_t* lbl = make_label(parent, &lv_font_montserrat_12, C_MUTED, value_width);
  lv_label_set_text(lbl, label);
  lv_obj_set_pos(lbl, 12, y);

  lv_obj_t* value = make_label(parent, &lv_font_montserrat_16, C_TEXT, value_width);
  lv_label_set_text(value, "--");
  lv_obj_set_pos(value, 12, y + 18);
  if (value_out) *value_out = value;
}

static void build_info_screen(lv_obj_t* scr) {
  style_screen(scr);

  create_claude_mascot(scr, 0, 2, 2);

  lv_obj_t* lbl_title = make_label(scr, &lv_font_montserrat_20, C_TEXT, 140);
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
  scr_usage = lv_obj_create(nullptr);
  scr_github = lv_obj_create(nullptr);
  scr_info = lv_obj_create(nullptr);

  build_usage_screen(scr_usage);
  build_github_screen(scr_github);
  build_info_screen(scr_info);

  s_screen_index = 0;
  s_last_screen_change = millis();
  lv_scr_load(scr_usage);
}

void ui_update(const DisplayData& d) {
  lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(lbl_status, "");

  if (!d.valid) {
    lv_label_set_text(lbl_pct, "--%");
    lv_label_set_text(lbl_value, "--");
    lv_label_set_text(lbl_reset, "");
    lv_label_set_text(lbl_detail, "");
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
  lv_label_set_text(lbl_detail, "monthly budget used");
  lv_label_set_text(lbl_status, "");
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

  lv_obj_set_style_bg_color(info_wifi_dot,
                            lv_color_hex(d.wifiConnected ? C_OK : C_STATUS),
                            LV_PART_MAIN);
  lv_label_set_text(lbl_info_wifi, d.wifiConnected ? "Connected" : "Offline");
  lv_label_set_text(lbl_info_ip, d.wifiConnected ? d.ipAddress : "Unavailable");
  lv_label_set_text(lbl_info_email, d.email[0] ? d.email : "Unavailable");
}

void ui_set_status(const char* msg) {
  lv_obj_clear_flag(spinner, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(lbl_status, msg);
}

void ui_next_screen() {
  const uint32_t now = millis();
  if (!scr_usage || !scr_github || !scr_info || now - s_last_screen_change < SCREEN_CHANGE_GUARD_MS) {
    Serial.println("Screen switch ignored");
    return;
  }

  s_last_screen_change = now;
  s_screen_index = (s_screen_index + 1) % 3;

  lv_obj_t* next = scr_usage;
  if (s_screen_index == 1) next = scr_github;
  if (s_screen_index == 2) next = scr_info;
  Serial.printf("Switching to screen %u\n", s_screen_index);
  lv_scr_load_anim(next, LV_SCR_LOAD_ANIM_OVER_LEFT, SCREEN_ANIM_MS, 0, false);
}

#endif // NO_DISPLAY
