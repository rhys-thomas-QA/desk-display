#include "wifi_setup.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <esp_wifi.h>

static Preferences prefs;
static bool prefsOpen = false;

void wifi_init() {
  if (!prefsOpen) {
    prefs.begin("cfg", false);
    prefsOpen = true;
  }
}

static String configured_helper_host() {
  wifi_init();
  String host = prefs.isKey("helper")
    ? prefs.getString("helper")
    : String("desk-display.local");

  if (host.startsWith("192.168.4.")) {
    Serial.printf("Ignoring setup-AP helper address: %s\n", host.c_str());
    prefs.remove("helper");
    return "desk-display.local";
  }

  return host;
}

static String configured_setup_code() {
  wifi_init();
  return prefs.isKey("setup_code") ? prefs.getString("setup_code") : "";
}

bool wifi_connect() {
  WiFiManager wm;
  String helperHost = configured_helper_host();
  String setupCode = configured_setup_code();

  WiFiManagerParameter helperParam(
    "helper", "Helper app hostname (e.g. desk-display.local)",
    helperHost.c_str(), 64);
  WiFiManagerParameter setupCodeParam(
    "setup_code", "Setup code from helper app",
    setupCode.c_str(), 8);

  wm.addParameter(&helperParam);
  wm.addParameter(&setupCodeParam);

  wm.setSaveParamsCallback([&]() {
    String helper = helperParam.getValue();
    if (helper.startsWith("192.168.4.")) {
      prefs.remove("helper");
    } else {
      prefs.putString("helper", helper);
    }

    String code = setupCodeParam.getValue();
    code.trim();
    if (code.length()) {
      prefs.putString("setup_code", code);
    } else {
      prefs.remove("setup_code");
    }
  });

  wm.setConfigPortalTimeout(0);
  wm.setConnectTimeout(30);

  return wm.autoConnect("DeskDisplay-Setup");
}

String wifi_get_helper_host() {
#ifdef DEFAULT_HELPER_HOST
  return DEFAULT_HELPER_HOST;
#else
  return configured_helper_host();
#endif
}

String wifi_get_setup_code() {
  return configured_setup_code();
}

void wifi_reset_settings() {
  wifi_init();

  Serial.println("Clearing WiFi credentials and helper settings");
  prefs.clear();

  WiFiManager wm;
  wm.resetSettings();
  esp_wifi_restore();
  WiFi.disconnect(true, true);
  delay(200);
}
