#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <time.h>
#include "display.h"
#ifndef NO_DISPLAY
#include <lvgl.h>
#endif
#include "nav_button.h"
#include "ui.h"
#include "wifi_setup.h"
#include "api_client.h"

#define POLL_INTERVAL_MS  (15UL * 60UL * 1000UL)
#define RETRY_INTERVAL_MS (30UL * 1000UL)
#define GITHUB_STATUS_INTERVAL_MS  (5UL * 60UL * 1000UL)
#define INFO_INTERVAL_MS  (10UL * 1000UL)
#define LVGL_TICK_MS      2
#define LOCAL_TIMEZONE    "GMT0BST,M3.5.0/1,M10.5.0/2"

static uint32_t s_lastPoll  = 0;
static uint32_t s_lastGitHubPoll = 0;
static uint32_t s_lastInfoUpdate = 0;
static uint32_t s_lastTick  = 0;
static bool     s_firstPoll = true;
static bool     s_firstGitHubPoll = true;
static bool     s_firstInfoUpdate = true;
static bool     s_navReady  = false;
static bool     s_factoryResetting = false;

static void lvgl_tick();

static String device_hostname() {
  uint64_t mac = ESP.getEfuseMac();
  char host[24];
  snprintf(host, sizeof(host), "desk-display-%06X", (uint32_t)(mac & 0xFFFFFF));
  return String(host);
}

static void setup_portal_started(const char* ssid, const char* password) {
  Serial.printf("Setup AP SSID: %s\n", ssid);
  Serial.printf("Setup AP password: %s\n", password);
  ui_show_setup_ap(ssid, password);
  lvgl_tick();
}

static void factory_reset() {
  if (s_factoryResetting) return;
  s_factoryResetting = true;

  Serial.println("Long press detected: resetting setup");
  ui_set_status("Resetting setup...");
  lvgl_tick();

  api_reset_settings();
  wifi_reset_settings();

  ui_set_status("Setup cleared.\nRestarting...");
  lvgl_tick();
  delay(800);
  ESP.restart();
}

static void lvgl_tick() {
#ifndef NO_DISPLAY
  uint32_t now = millis();
  uint32_t elapsed = now - s_lastTick;
  if (elapsed >= LVGL_TICK_MS) {
    lv_tick_inc(elapsed);
    s_lastTick = now;
  }
  lv_timer_handler();
#endif
  if (s_navReady) {
    if (nav_button_was_long_pressed()) {
      factory_reset();
    } else if (nav_button_was_pressed()) {
      ui_next_screen();
    }
  }
}

static void refresh_info_screen() {
  InfoData info = {};
  info.wifiConnected = WiFi.status() == WL_CONNECTED;

  if (info.wifiConnected) {
    String ip = WiFi.localIP().toString();
    strlcpy(info.ipAddress, ip.c_str(), sizeof(info.ipAddress));
  } else {
    strlcpy(info.ipAddress, "Unavailable", sizeof(info.ipAddress));
  }

  api_get_email(info.email, sizeof(info.email));
  ui_update_info(info);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nDesk Display booting...");

  display_init();
  ui_init();
  nav_button_init();
  s_navReady = true;
  ui_set_status("Booting...");
  lvgl_tick();

  wifi_init();
  ui_set_status("Connecting WiFi...");
  lvgl_tick();

  if (!wifi_connect(setup_portal_started)) {
    ui_set_status("WiFi failed.\nReset to retry.");
    while (true) lvgl_tick();
  }

  Serial.printf("WiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
  ui_set_status("WiFi connected");
  lvgl_tick();

  String hostname = device_hostname();
  MDNS.begin(hostname.c_str());
  Serial.printf("mDNS hostname: %s.local\n", hostname.c_str());

  ui_set_status("Syncing time...");
  lvgl_tick();
  configTzTime(LOCAL_TIMEZONE, "time.google.com", "pool.ntp.org");
  uint32_t ntpStart = millis();
  while (time(nullptr) < 1577836800UL && millis() - ntpStart < 30000) {
    delay(100);
    lvgl_tick();
  }
  if (time(nullptr) < 1577836800UL) Serial.println("Warning: NTP sync timed out");

  if (!api_is_provisioned()) {
    ui_set_status("Provisioning...");
    lvgl_tick();
    for (int i = 0; i < 3 && !api_is_provisioned(); i++) {
      if (api_provision()) break;
      delay(3000);
      lvgl_tick();
    }
  }

  if (!api_has_email()) {
    ui_set_status("Refreshing info...");
    lvgl_tick();
    api_refresh_email();
  }

  ui_set_status("Fetching...");
  refresh_info_screen();
  lvgl_tick();
}

void loop() {
  lvgl_tick();

  uint32_t now = millis();
  if (s_firstInfoUpdate || (now - s_lastInfoUpdate >= INFO_INTERVAL_MS)) {
    s_firstInfoUpdate = false;
    s_lastInfoUpdate = now;
    refresh_info_screen();
  }

  if (s_firstGitHubPoll || (now - s_lastGitHubPoll >= GITHUB_STATUS_INTERVAL_MS)) {
    s_firstGitHubPoll = false;
    s_lastGitHubPoll = now;
    ui_update_github_status(api_poll_github_status());
  }

  if (s_firstPoll || (now - s_lastPoll >= POLL_INTERVAL_MS)) {
    s_firstPoll = false;
    s_lastPoll  = now;

    if (time(nullptr) < 1577836800UL) {
      ui_set_status("Syncing time...");
      lvgl_tick();
      uint32_t t = millis();
      while (time(nullptr) < 1577836800UL && millis() - t < 15000) {
        delay(200);
        lvgl_tick();
      }
      if (time(nullptr) < 1577836800UL) {
        DisplayData dd = {};
        strlcpy(dd.statusMsg, "NTP sync failed", sizeof(dd.statusMsg));
        ui_update(dd);
        s_firstPoll = true; // retry next iteration
        return;
      }
    }

    if (!api_is_provisioned()) {
      ui_set_status("Provisioning...");
      lvgl_tick();
      if (!api_provision()) {
        DisplayData dd = {};
        api_get_last_error(dd.statusMsg, sizeof(dd.statusMsg));
        if (!dd.statusMsg[0]) {
          strlcpy(dd.statusMsg, "Run helper app to provision", sizeof(dd.statusMsg));
        }
        ui_update(dd);
        s_lastPoll = millis() - (POLL_INTERVAL_MS - RETRY_INTERVAL_MS);
        return;
      }
    }

    ui_set_status("Fetching...");
    lvgl_tick();

    StatusData status = api_poll();
    if (!status.valid && (status.httpStatus == 401 || status.httpStatus == 404)) {
      ui_set_status("Reprovisioning...");
      lvgl_tick();
      if (api_provision()) {
        status = api_poll();
      }
    }

    DisplayData dd = {};
    dd.valid = status.valid;

    if (status.valid) {
      dd.percent  = status.percent;
      dd.barColor = status.barColor;
      strlcpy(dd.valueText,  status.valueText,  sizeof(dd.valueText));
      strlcpy(dd.detailText, status.detailText, sizeof(dd.detailText));
      strlcpy(dd.resetText,  status.resetText,  sizeof(dd.resetText));
    } else {
      strlcpy(dd.statusMsg, status.errorMsg[0] ? status.errorMsg : "Fetch failed", sizeof(dd.statusMsg));
      s_lastPoll = millis() - (POLL_INTERVAL_MS - RETRY_INTERVAL_MS);
    }

    ui_update(dd);
  }

  delay(LVGL_TICK_MS);
}
