#pragma once
#include <stdint.h>
#include <stddef.h>

struct StatusData {
  bool     valid;
  char     valueText[16];   // e.g. "$45.16"
  char     detailText[16];  // e.g. "of $200"
  uint8_t  percent;         // 0-100
  uint32_t barColor;        // 0xRRGGBB
  int      httpStatus;
  char     resetText[32];   // e.g. "As of 12:13 BST"
  char     resetHint[24];   // e.g. "Resets in 19 days"
  char     errorMsg[64];
};

struct GitHubStatusData {
  bool valid;
  bool degraded;
  char summary[40];
  char affected[96];
  char checkedAt[24];
  char errorMsg[48];
};

bool       api_is_provisioned();
bool       api_has_email();
bool       api_has_openai_key();
bool       api_provision();
bool       api_refresh_email();
void       api_get_email(char* buf, size_t len);
void       api_get_last_error(char* buf, size_t len);
void       api_reset_settings();
StatusData api_poll();
StatusData api_poll_openai();
GitHubStatusData api_poll_github_status();
