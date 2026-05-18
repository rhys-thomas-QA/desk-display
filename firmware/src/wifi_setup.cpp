#include "wifi_setup.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <esp_system.h>

static Preferences prefs;
static bool prefsOpen = false;
static constexpr uint32_t SETUP_PORTAL_TIMEOUT_SECONDS = 15UL * 60UL;

void wifi_init() {
  if (!prefsOpen) {
    prefs.begin("cfg", false);
    prefsOpen = true;
  }
}

static String setup_ap_suffix() {
  uint64_t mac = ESP.getEfuseMac();
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06X", (uint32_t)(mac & 0xFFFFFF));
  return String(suffix);
}

static String setup_ap_ssid() {
  return "DeskDisplay-" + setup_ap_suffix();
}

static String generate_setup_ap_password() {
  static const char alphabet[] =
    "ABCDEFGHJKLMNPQRSTUVWXYZ"
    "abcdefghijkmnopqrstuvwxyz"
    "23456789";
  char pass[15];
  for (size_t i = 0; i < sizeof(pass) - 1; i++) {
    pass[i] = alphabet[esp_random() % (sizeof(alphabet) - 1)];
  }
  pass[sizeof(pass) - 1] = '\0';
  return String(pass);
}

static String configured_setup_ap_password() {
  wifi_init();
  String password = prefs.isKey("ap_pass") ? prefs.getString("ap_pass") : "";
  if (password.length() >= 8) return password;

  password = generate_setup_ap_password();
  prefs.putString("ap_pass", password);
  return password;
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

bool wifi_connect(WifiSetupPortalCallback onPortalStarted) {
  WiFiManager wm;
  String helperHost = configured_helper_host();
  String setupCode = configured_setup_code();
  String apSsid = setup_ap_ssid();
  String apPassword = configured_setup_ap_password();

  WiFiManagerParameter helperParam(
    "helper", "Helper app hostname (e.g. desk-display.local)",
    helperHost.c_str(), 64);
  WiFiManagerParameter setupCodeParam(
    "setup_code", "10-digit setup code from helper app",
    setupCode.c_str(), 11,
    "pattern='[0-9]{10}' minlength='10' maxlength='10' inputmode='numeric' "
    "title='Enter the 10-digit code shown on the helper ready page'");

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
    bool validCode = code.length() == 8;
    for (int i = 0; validCode && i < 8; i++) validCode = isdigit(code[i]);
    if (validCode) {
      prefs.putString("setup_code", code);
    } else {
      prefs.remove("setup_code");
    }
  });

  wm.setAPCallback([&](WiFiManager*) {
    Serial.printf("Setup portal started: %s\n", apSsid.c_str());
    if (onPortalStarted) onPortalStarted(apSsid.c_str(), apPassword.c_str());
  });

  wm.setConfigPortalTimeout(SETUP_PORTAL_TIMEOUT_SECONDS);
  wm.setConnectTimeout(30);

  return wm.autoConnect(apSsid.c_str(), apPassword.c_str());
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

void wifi_clear_setup_code() {
  wifi_init();
  if (prefs.isKey("setup_code")) prefs.remove("setup_code");
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
