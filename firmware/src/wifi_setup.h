#pragma once
#include <Arduino.h>

void    wifi_init();
bool    wifi_connect();
String  wifi_get_helper_host();
String  wifi_get_setup_code();
void    wifi_reset_settings();
