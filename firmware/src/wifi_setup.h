#pragma once
#include <Arduino.h>

using WifiSetupPortalCallback = void (*)(const char* ssid, const char* password);

void    wifi_init();
bool    wifi_connect(WifiSetupPortalCallback onPortalStarted = nullptr,
                     bool requireSetupCode = false);
String  wifi_get_helper_host();
String  wifi_get_setup_code();
void    wifi_clear_setup_code();
void    wifi_reset_settings();
