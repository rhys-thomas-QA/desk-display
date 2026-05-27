#include "api_client.h"
#include "wifi_setup.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <mbedtls/base64.h>
#include <mbedtls/gcm.h>
#include <mbedtls/sha256.h>

static Preferences s_prefs;
static bool        s_prefsOpen = false;
static char        s_lastError[64] = "";
static const char  PROVISIONING_KDF_PREFIX[] = "desk-display provisioning v1";
static const unsigned char PROVISIONING_AAD[] = "desk-display-provision-v1";

static const char PUBLIC_ROOT_CA_PEM[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYDVQQGEwJV
UzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIGA1UEAxMLR1RTIFJv
b3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAwWjBHMQswCQYDVQQGEwJVUzEi
MCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3Qg
UjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvV
YRg1rkDdc/eJkTBa6zzuhXyiQHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHc
i8nHc8iMai/lxKvRHYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0G
A1UdDgQWBBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D
9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8p/SgguMh
1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAwTzELMAkG
A1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2VhcmNoIEdyb3VwMRUw
EwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4WhcNMzUwNjA0MTEwNDM4WjBP
MQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3Jv
dXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoC
ggIBAK3oJHP0FDfzm54rVygch77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj
/RQSa78f0uoxmyF+0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7i
S4+3mX6UA5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyHB5T0Y3Hs
LuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UCB5iPNgiV5+I3lg02
dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUvKBds0pjBqAlkd25HN7rOrFle
aJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWnOlFuhjuefXKnEgV4We0+UXgVCwOPjdAv
BbI+e0ocS3MFEvzG6uBQE3xDk3SzynTnjh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymC
zLq9gwQbooMDQaHWBfEbwrbwqHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC
1CLQJ13hef4Y53CIrU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIB
BjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZLubhzEFnT
IZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ3BebYhtF8GaV0nxv
wuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KKNFtY2PwByVS5uCbMiogziUwt
hDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5ORAzI4JMPJ+GslWYHb4phowim57iaztX
OoJwTdwJx4nLCgdNbOhdjsnvzqvHu7UrTkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIu
vtd7u+Nxe5AW0wdeRlN8NwdCjNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1N
bdWhscdCb+ZAJzVcoyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4k
qKOJ2qxq4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57demyPxgcY
xn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

static void configureSecureClient(WiFiClientSecure& client) {
  client.setCACert(PUBLIC_ROOT_CA_PEM);
}

static void deriveProvisioningKey(const String& setupCode, uint8_t key[32]) {
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);
  mbedtls_sha256_update_ret(&ctx,
                            reinterpret_cast<const unsigned char*>(PROVISIONING_KDF_PREFIX),
                            strlen(PROVISIONING_KDF_PREFIX));
  mbedtls_sha256_update_ret(&ctx,
                            reinterpret_cast<const unsigned char*>(setupCode.c_str()),
                            setupCode.length());
  mbedtls_sha256_finish_ret(&ctx, key);
  mbedtls_sha256_free(&ctx);
}

static bool decodeBase64Field(JsonDocument& doc, const char* name,
                              uint8_t* out, size_t capacity, size_t* outLen) {
  const char* encoded = doc[name];
  if (!encoded || !encoded[0]) {
    Serial.printf("Provision envelope missing %s\n", name);
    return false;
  }

  size_t decodedLen = 0;
  int rc = mbedtls_base64_decode(out, capacity, &decodedLen,
                                 reinterpret_cast<const unsigned char*>(encoded),
                                 strlen(encoded));
  if (rc != 0) {
    Serial.printf("Provision base64 error on %s: %d\n", name, rc);
    return false;
  }

  *outLen = decodedLen;
  return true;
}

static bool decryptProvisioningPayload(const String& setupCode,
                                       JsonDocument& envelope,
                                       DynamicJsonDocument& provisionDoc) {
  int version = envelope["version"] | 0;
  const char* alg = envelope["alg"] | "";
  if (version != 1 || strcmp(alg, "A256GCM") != 0) {
    Serial.println("Unsupported provision envelope");
    strlcpy(s_lastError, "Provision format error", sizeof(s_lastError));
    return false;
  }

  uint8_t iv[12];
  uint8_t tag[16];
  uint8_t ciphertext[768];
  uint8_t plaintext[sizeof(ciphertext) + 1];
  size_t ivLen = 0;
  size_t tagLen = 0;
  size_t ciphertextLen = 0;

  if (!decodeBase64Field(envelope, "iv", iv, sizeof(iv), &ivLen) ||
      !decodeBase64Field(envelope, "tag", tag, sizeof(tag), &tagLen) ||
      !decodeBase64Field(envelope, "ciphertext", ciphertext, sizeof(ciphertext), &ciphertextLen)) {
    strlcpy(s_lastError, "Provision decode error", sizeof(s_lastError));
    return false;
  }

  if (ivLen != sizeof(iv) || tagLen != sizeof(tag) || ciphertextLen >= sizeof(plaintext)) {
    Serial.println("Provision envelope length error");
    strlcpy(s_lastError, "Provision length error", sizeof(s_lastError));
    return false;
  }

  uint8_t key[32];
  deriveProvisioningKey(setupCode, key);

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
  if (rc == 0) {
    rc = mbedtls_gcm_auth_decrypt(&gcm, ciphertextLen,
                                  iv, ivLen,
                                  PROVISIONING_AAD, strlen(reinterpret_cast<const char*>(PROVISIONING_AAD)),
                                  tag, tagLen,
                                  ciphertext, plaintext);
  }
  mbedtls_gcm_free(&gcm);

  if (rc != 0) {
    Serial.printf("Provision decrypt error: %d\n", rc);
    strlcpy(s_lastError, "Provision decrypt error", sizeof(s_lastError));
    return false;
  }

  plaintext[ciphertextLen] = '\0';
  DeserializationError err = deserializeJson(provisionDoc, reinterpret_cast<const char*>(plaintext));
  if (err) {
    Serial.println("Provision plaintext JSON error");
    strlcpy(s_lastError, "Provision JSON error", sizeof(s_lastError));
    return false;
  }

  return true;
}

static Preferences& prefs() {
  if (!s_prefsOpen) {
    s_prefs.begin("api", false);
    s_prefsOpen = true;
  }
  return s_prefs;
}

bool api_is_provisioned() {
  return prefs().isKey("api_key") &&
         prefs().getString("api_key", "").length() > 0 &&
         api_has_email();
}

bool api_has_email() {
  return prefs().isKey("email") && prefs().getString("email", "").length() > 0;
}

void api_get_email(char* buf, size_t len) {
  String email = prefs().isKey("email") ? prefs().getString("email", "") : "";
  strlcpy(buf, email.length() ? email.c_str() : "Unavailable", len);
}

void api_get_last_error(char* buf, size_t len) {
  strlcpy(buf, s_lastError, len);
}

void api_reset_settings() {
  Serial.println("Clearing helper profile and legacy API credentials");
  if (prefs().isKey("email")) prefs().remove("email");
  if (prefs().isKey("api_key")) prefs().remove("api_key");
  if (prefs().isKey("user_id")) prefs().remove("user_id");
}

static bool isSetupApAddress(const IPAddress& ip) {
  return ip[0] == 192 && ip[1] == 168 && ip[2] == 4;
}

static bool localIsSetupSubnet() {
  IPAddress local = WiFi.localIP();
  return local[0] == 192 && local[1] == 168 && local[2] == 4;
}

static bool resolveFallback(IPAddress& ip) {
#ifdef HELPER_HOST_FALLBACK
  String fallback = HELPER_HOST_FALLBACK;
  if (fallback.isEmpty()) return false;

  Serial.printf("Trying helper fallback: %s\n", fallback.c_str());
  if (ip.fromString(fallback)) return true;

  String name = fallback.endsWith(".local")
    ? fallback.substring(0, fallback.length() - 6) : fallback;
  ip = MDNS.queryHost(name.c_str(), 3000);
  return (uint32_t)ip != 0;
#else
  (void)ip;
  return false;
#endif
}

static bool resolveHelper(IPAddress& ip) {
  String host = wifi_get_helper_host();

  if (!ip.fromString(host)) {
    String name = host.endsWith(".local")
      ? host.substring(0, host.length() - 6) : host;
    ip = MDNS.queryHost(name.c_str(), 3000);
  }

  if ((uint32_t)ip != 0 && isSetupApAddress(ip) && !localIsSetupSubnet()) {
    Serial.printf("Ignoring setup-AP helper address: %s\n", ip.toString().c_str());
    ip = IPAddress();
  }

  if ((uint32_t)ip == 0 && !resolveFallback(ip)) {
    if (!host.endsWith(".local")) {
      String name = host.endsWith(".local")
        ? host.substring(0, host.length() - 6) : host;
      ip = MDNS.queryHost(name.c_str(), 3000);
    }
    if ((uint32_t)ip == 0) {
      Serial.printf("Cannot resolve helper: %s\n", host.c_str());
      strlcpy(s_lastError, "Helper not found", sizeof(s_lastError));
      return false;
    }
  }

  return true;
}

bool api_provision() {
  IPAddress ip;
  if (!resolveHelper(ip)) return false;

  String setupCode = wifi_get_setup_code();
  setupCode.trim();
  if (setupCode.isEmpty()) {
    strlcpy(s_lastError, "Enter setup code", sizeof(s_lastError));
    Serial.println("Provision skipped: missing setup code");
    return false;
  }

  HTTPClient http;
  String url = "http://" + ip.toString() + ":3333/provision";
  Serial.printf("Provisioning from: %s\n", url.c_str());
  http.begin(url);
  http.addHeader("X-Setup-Code", setupCode);
  http.setTimeout(10000);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("Provision HTTP %d\n", code);
    if (code <= 0) {
      strlcpy(s_lastError, "Helper unreachable", sizeof(s_lastError));
    } else if (code == 403) {
      strlcpy(s_lastError, "Check setup code", sizeof(s_lastError));
    } else if (code == 409) {
      strlcpy(s_lastError, "Restart helper app", sizeof(s_lastError));
    } else if (code == 410) {
      strlcpy(s_lastError, "Helper code expired", sizeof(s_lastError));
    } else if (code == 429) {
      strlcpy(s_lastError, "Try setup later", sizeof(s_lastError));
    } else if (code == 503) {
      strlcpy(s_lastError, "Setup helper key", sizeof(s_lastError));
    } else {
      snprintf(s_lastError, sizeof(s_lastError), "Provision HTTP %d", code);
    }
    http.end();
    return false;
  }

  DynamicJsonDocument envelope(1280);
  DeserializationError err = deserializeJson(envelope, http.getStream());
  http.end();

  if (err) {
    Serial.println("Provision envelope JSON error");
    strlcpy(s_lastError, "Provision JSON error", sizeof(s_lastError));
    return false;
  }

  DynamicJsonDocument doc(768);
  if (!decryptProvisioningPayload(setupCode, envelope, doc)) {
    return false;
  }

  const char* email = doc["email"];
  const char* apiKey = doc["api_key"];
  const char* userId = doc["user_id"];
  if (!email || !email[0]) {
    Serial.println("Provision: missing email");
    strlcpy(s_lastError, "Provision missing email", sizeof(s_lastError));
    return false;
  }
  if (!apiKey || !apiKey[0]) {
    Serial.println("Provision: missing API key");
    strlcpy(s_lastError, "Provision missing key", sizeof(s_lastError));
    return false;
  }

  prefs().putString("api_key", apiKey);
  prefs().putString("email", email);
  if (userId && userId[0]) {
    prefs().putString("user_id", userId);
  } else if (prefs().isKey("user_id")) {
    prefs().remove("user_id");
  }
  Serial.println("Provisioned analytics credentials");
  wifi_clear_setup_code();
  s_lastError[0] = '\0';
  return true;
}

bool api_refresh_email() {
  IPAddress ip;
  if (!resolveHelper(ip)) return false;

  String setupCode = wifi_get_setup_code();
  setupCode.trim();
  if (setupCode.isEmpty()) {
    Serial.println("Profile refresh skipped: missing setup code");
    return false;
  }

  HTTPClient http;
  String url = "http://" + ip.toString() + ":3333/profile";
  Serial.printf("Refreshing profile from: %s\n", url.c_str());
  http.begin(url);
  http.addHeader("X-Setup-Code", setupCode);
  http.setTimeout(10000);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("Profile HTTP %d\n", code);
    http.end();
    return false;
  }

  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();

  if (err) { Serial.println("Profile JSON error"); return false; }

  const char* email = doc["email"];
  if (!email || !email[0]) { Serial.println("Profile: missing email"); return false; }

  prefs().putString("email", email);
  wifi_clear_setup_code();
  Serial.println("Stored profile email");
  return true;
}

static String billingStartISO() {
  time_t now = time(nullptr);
  struct tm t;
  gmtime_r(&now, &t);
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-01T00:00:00Z", t.tm_year + 1900, t.tm_mon + 1);
  return String(buf);
}

static String nowISO() {
  time_t now = time(nullptr);
  struct tm t;
  gmtime_r(&now, &t);
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
           t.tm_hour, t.tm_min, t.tm_sec);
  return String(buf);
}

static void formatResetText(char* buf, size_t len) {
  time_t now = time(nullptr);
  struct tm t;
  gmtime_r(&now, &t);

  // Calculate seconds remaining in the current UTC month
  // Days in each month (non-leap year; close enough for a display countdown)
  static const int daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  int year  = t.tm_year + 1900;
  int month = t.tm_mon;  // 0-based
  int dim   = daysInMonth[month];
  if (month == 1 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) dim = 29;

  uint32_t secsLeft = (uint32_t)(dim - t.tm_mday) * 86400
                    + (uint32_t)(23 - t.tm_hour)  * 3600
                    + (uint32_t)(59 - t.tm_min)   * 60
                    + (uint32_t)(59 - t.tm_sec)   + 1;
  uint32_t days  = secsLeft / 86400;
  uint32_t hours = (secsLeft % 86400) / 3600;
  snprintf(buf, len, "Resets in %ud %uh", (unsigned)days, (unsigned)hours);
}

static const char* localZoneName(const struct tm& t) {
  return t.tm_isdst > 0 ? "BST" : "GMT";
}

static int parseFixedUInt(const char* s, int digits) {
  int value = 0;
  for (int i = 0; i < digits; i++) {
    if (s[i] < '0' || s[i] > '9') return -1;
    value = value * 10 + (s[i] - '0');
  }
  return value;
}

static bool isLeapYear(int year) {
  return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

static int daysInMonth(int year, int month) {
  static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (month == 2 && isLeapYear(year)) return 29;
  return days[month - 1];
}

static time_t epochFromUtc(int year, int month, int day, int hour, int minute, int second) {
  int64_t days = 0;
  for (int y = 1970; y < year; y++) {
    days += isLeapYear(y) ? 366 : 365;
  }
  for (int m = 1; m < month; m++) {
    days += daysInMonth(year, m);
  }
  days += day - 1;
  return (time_t)(days * 86400 + hour * 3600 + minute * 60 + second);
}

static bool parseIsoUtc(const char* iso, time_t* out) {
  if (!iso || !out) return false;
  if (iso[4] != '-' || iso[7] != '-' || iso[10] != 'T' ||
      iso[13] != ':' || iso[16] != ':') {
    return false;
  }

  int year = parseFixedUInt(iso, 4);
  int month = parseFixedUInt(iso + 5, 2);
  int day = parseFixedUInt(iso + 8, 2);
  int hour = parseFixedUInt(iso + 11, 2);
  int minute = parseFixedUInt(iso + 14, 2);
  int second = parseFixedUInt(iso + 17, 2);
  if (year < 1970 || month < 1 || month > 12 || day < 1 || day > daysInMonth(year, month)) {
    return false;
  }
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 60) {
    return false;
  }

  *out = epochFromUtc(year, month, day, hour, minute, second);
  return true;
}

static uint32_t barColorFor(int pct) {
  if (pct >= 80) return 0xff7b72;
  if (pct >= 60) return 0xf0883e;
  return 0x3fb950;
}

static void formatCheckedAt(char* buf, size_t len) {
  time_t now = time(nullptr);
  if (now < 1577836800UL) {
    strlcpy(buf, "Checked just now", len);
    return;
  }

  struct tm t;
  localtime_r(&now, &t);
  snprintf(buf, len, "Checked %02d:%02d %s", t.tm_hour, t.tm_min, localZoneName(t));
}

static void appendAffected(char* dst, size_t len, const char* name) {
  if (!name || !name[0]) return;
  if (dst[0]) strlcat(dst, ", ", len);
  strlcat(dst, name, len);
}

static bool githubComponentDegraded(JsonObject component) {
  const char* status = component["status"] | "operational";
  return strcmp(status, "operational") != 0;
}

static void appendDegradedComponents(char* dst, size_t len, JsonArray components) {
  for (JsonObject component : components) {
    if (!githubComponentDegraded(component)) continue;
    appendAffected(dst, len, component["name"]);
  }
}

GitHubStatusData api_poll_github_status() {
  GitHubStatusData result = {};

  WiFiClientSecure wifiClient;
  configureSecureClient(wifiClient);

  HTTPClient http;
  http.begin(wifiClient, "https://www.githubstatus.com/api/v2/summary.json");
  http.addHeader("User-Agent", "DeskDisplay");
  http.setTimeout(10000);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    snprintf(result.errorMsg, sizeof(result.errorMsg), "GitHub HTTP %d", code);
    Serial.println(result.errorMsg);
    http.end();
    return result;
  }

  StaticJsonDocument<512> filter;
  filter["status"]["indicator"] = true;
  filter["status"]["description"] = true;
  filter["components"][0]["name"] = true;
  filter["components"][0]["status"] = true;
  filter["incidents"][0]["name"] = true;
  filter["incidents"][0]["components"][0]["name"] = true;
  filter["incidents"][0]["components"][0]["status"] = true;

  DynamicJsonDocument doc(6144);
  DeserializationError err = deserializeJson(doc, http.getStream(),
                                            DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    snprintf(result.errorMsg, sizeof(result.errorMsg), "GitHub JSON %s", err.c_str());
    Serial.println(result.errorMsg);
    return result;
  }

  const char* indicator = doc["status"]["indicator"] | "";
  const char* description = doc["status"]["description"] | "GitHub status";
  result.degraded = strcmp(indicator, "none") != 0;

  strlcpy(result.summary, description, sizeof(result.summary));
  formatCheckedAt(result.checkedAt, sizeof(result.checkedAt));

  appendDegradedComponents(result.affected, sizeof(result.affected), doc["components"]);
  if (result.affected[0]) result.degraded = true;

  if (!result.degraded) {
    result.affected[0] = '\0';
  } else if (!result.affected[0]) {
    for (JsonObject incident : doc["incidents"].as<JsonArray>()) {
      appendDegradedComponents(result.affected, sizeof(result.affected), incident["components"]);
      if (result.affected[0]) break;
      appendAffected(result.affected, sizeof(result.affected), incident["name"]);
      if (result.affected[0]) break;
    }
    if (!result.affected[0]) {
      strlcpy(result.affected, description[0] ? description : "Details unavailable",
              sizeof(result.affected));
    }
  }

  result.valid = true;
  Serial.printf("GitHub status: %s (%s)\n", result.summary, result.affected);
  return result;
}

constexpr int64_t MICROCENTS_PER_CENT = 1000000LL;

static int64_t amountAsMicroCents(JsonVariantConst value) {
  const char* s = value.as<const char*>();
  if (!s || !s[0]) {
    if (value.is<long>()) return (int64_t)value.as<long>() * MICROCENTS_PER_CENT;
    return 0;
  }

  bool negative = false;
  if (*s == '-') {
    negative = true;
    s++;
  }

  int64_t whole = 0;
  while (*s >= '0' && *s <= '9') {
    whole = whole * 10 + (*s - '0');
    s++;
  }

  int64_t frac = 0;
  int fracDigits = 0;
  if (*s == '.') {
    s++;
    while (*s >= '0' && *s <= '9' && fracDigits < 6) {
      frac = frac * 10 + (*s - '0');
      fracDigits++;
      s++;
    }
  }
  while (fracDigits < 6) {
    frac *= 10;
    fracDigits++;
  }

  int64_t amount = whole * MICROCENTS_PER_CENT + frac;
  return negative ? -amount : amount;
}

static void formatMoney(char* buf, size_t len, uint64_t cents) {
  unsigned long dollars = (unsigned long)(cents / 100ULL);
  unsigned centsPart = (unsigned)(cents % 100ULL);
  snprintf(buf, len, "$%lu.%02u", dollars, centsPart);
}

static void applyUsageResult(StatusData& result, int64_t totalMicroCents, int limit, const char* refreshedAt) {
  if (totalMicroCents < 0) totalMicroCents = 0;

  uint64_t displayCents = ((uint64_t)totalMicroCents + (MICROCENTS_PER_CENT / 2)) /
                          MICROCENTS_PER_CENT;
  uint64_t limitCents = (uint64_t)limit * 100ULL;
  uint32_t pct = limitCents > 0 ? (uint32_t)((displayCents * 100ULL) / limitCents) : 0;
  if (pct > 100) pct = 100;

  result.valid    = true;
  result.percent  = (uint8_t)pct;
  result.barColor = barColorFor(pct);
  formatMoney(result.valueText, sizeof(result.valueText), displayCents);
  snprintf(result.detailText, sizeof(result.detailText), "of $%d", limit);

  if (refreshedAt && refreshedAt[0]) {
    time_t refreshed = 0;
    struct tm local;
    if (parseIsoUtc(refreshedAt, &refreshed) && localtime_r(&refreshed, &local)) {
      snprintf(result.resetText, sizeof(result.resetText), "As of %02d:%02d %s",
               local.tm_hour, local.tm_min, localZoneName(local));
    } else {
      strlcpy(result.resetText, "As of latest data", sizeof(result.resetText));
    }
  } else {
    formatResetText(result.resetText, sizeof(result.resetText));
  }
}

StatusData api_poll() {
  StatusData result = {};

  String apiKey = prefs().getString("api_key", "");
  String email = prefs().isKey("email") ? prefs().getString("email", "") : "";
  String userId = prefs().isKey("user_id") ? prefs().getString("user_id", "") : "";
  if (apiKey.isEmpty() || email.isEmpty()) {
    strlcpy(result.errorMsg, "Not provisioned", sizeof(result.errorMsg));
    return result;
  }

  if (time(nullptr) < 1577836800UL) {
    strlcpy(result.errorMsg, "Time not synced", sizeof(result.errorMsg));
    return result;
  }

  String baseUrl = String("https://api.anthropic.com/v1/organizations/analytics/user_cost_report")
    + "?starting_at=" + billingStartISO()
    + "&ending_at="   + nowISO()
    + "&order_by=amount"
    + "&limit=1000";
  if (!userId.isEmpty()) {
    baseUrl += "&user_ids[]=" + userId;
  }

  int64_t totalMicroCents = 0;
  char refreshedAt[32] = "";
  String page;

  for (int pageNum = 0; pageNum < 4; pageNum++) {
    String url = baseUrl;
    if (!page.isEmpty()) url += "&page=" + page;
    Serial.printf("Polling Anthropic: %s\n", url.c_str());

    WiFiClientSecure wifiClient;
    configureSecureClient(wifiClient);
    HTTPClient http;
    http.begin(wifiClient, url);
    http.addHeader("x-api-key", apiKey);
    http.addHeader("User-Agent", "DeskDisplay");
    http.setTimeout(15000);

    int code = http.GET();
    result.httpStatus = code;
    if (code != HTTP_CODE_OK) {
      snprintf(result.errorMsg, sizeof(result.errorMsg), "HTTP %d", code);
      Serial.printf("Anthropic fetch failed: HTTP %d\n", code);
      http.end();
      return result;
    }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(12288);
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
      Serial.printf("Anthropic JSON error: %s\n", err.c_str());
      strlcpy(result.errorMsg, "JSON error", sizeof(result.errorMsg));
      return result;
    }

    const char* refreshed = doc["data_refreshed_at"];
    if (refreshed && refreshed[0]) strlcpy(refreshedAt, refreshed, sizeof(refreshedAt));

    JsonArray data = doc["data"];
    for (JsonObject row : data) {
      if (userId.isEmpty()) {
        const char* rowEmail = row["actor"]["email_address"];
        if (!rowEmail) rowEmail = row["actor"]["email"];
        if (!rowEmail || !String(rowEmail).equalsIgnoreCase(email)) continue;
      }
      totalMicroCents += amountAsMicroCents(row["amount"]);
    }

    const bool hasMore = doc["has_more"] | false;
    const char* nextPage = doc["next_page"] | "";
    if (!hasMore || !nextPage[0]) break;
    page = nextPage;
  }

  applyUsageResult(result, totalMicroCents, 200, refreshedAt);

  Serial.printf("Cost: %s %s (%d%%)\n", result.valueText, result.detailText, result.percent);
  return result;
}
