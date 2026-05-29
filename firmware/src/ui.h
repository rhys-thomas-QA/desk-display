#pragma once
#include <stdint.h>

struct DisplayData {
  bool     valid;
  char     valueText[16];   // "$31"
  char     detailText[16];  // "of $200"
  uint8_t  percent;         // 0-100, drives the bar
  uint32_t barColor;        // 0xRRGGBB from server
  char     resetText[32];   // "As of 12:13 BST"
  char     resetHint[24];   // "Resets in 19 days"
  char     statusMsg[64];   // shown when !valid
};

struct GitHubStatusData;

struct InfoData {
  bool wifiConnected;
  int  rssi;
  char ipAddress[16];
  char email[64];
};

void ui_init();
void ui_update(const DisplayData& data);
void ui_update_openai(const DisplayData& data);
void ui_update_github_status(const GitHubStatusData& data);
void ui_update_info(const InfoData& data);
void ui_show_setup_ap(const char* ssid, const char* password);
void ui_set_openai_enabled(bool enabled);
void ui_set_wifi_signal(bool connected, int rssi);
void ui_set_reset_confirmation(bool active, uint8_t percent);
void ui_set_status(const char* msg);
void ui_next_screen();
