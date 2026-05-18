#include "nav_button.h"
#include <Arduino.h>

#ifndef NAV_BUTTON_PIN
#define NAV_BUTTON_PIN 27
#endif

#ifndef NAV_BUTTON_ACTIVE_LOW
#define NAV_BUTTON_ACTIVE_LOW 1
#endif

#ifndef NAV_BUTTON_DEBOUNCE_MS
#define NAV_BUTTON_DEBOUNCE_MS 45
#endif

#ifndef NAV_BUTTON_LONG_PRESS_MS
#define NAV_BUTTON_LONG_PRESS_MS 3000
#endif

static bool s_lastRawPressed = false;
static bool s_stablePressed = false;
static uint32_t s_lastEdgeMs = 0;
static uint32_t s_pressedSinceMs = 0;
static bool s_shortPressPending = false;
static bool s_longPressPending = false;
static bool s_longPressFired = false;

static bool read_pressed() {
#if NAV_BUTTON_ACTIVE_LOW
  return digitalRead(NAV_BUTTON_PIN) == LOW;
#else
  return digitalRead(NAV_BUTTON_PIN) == HIGH;
#endif
}

void nav_button_init() {
#if NAV_BUTTON_ACTIVE_LOW
  pinMode(NAV_BUTTON_PIN, INPUT_PULLUP);
#else
  pinMode(NAV_BUTTON_PIN, INPUT_PULLDOWN);
#endif

  s_lastRawPressed = read_pressed();
  s_stablePressed = s_lastRawPressed;
  s_lastEdgeMs = millis();
  s_pressedSinceMs = s_stablePressed ? s_lastEdgeMs : 0;
  s_shortPressPending = false;
  s_longPressPending = false;
  s_longPressFired = false;

  Serial.printf("Navigation button on GPIO %d (%s)\n",
                NAV_BUTTON_PIN,
#if NAV_BUTTON_ACTIVE_LOW
                "active low");
#else
                "active high");
#endif
}

static void nav_button_update() {
  const uint32_t now = millis();
  const bool rawPressed = read_pressed();

  if (rawPressed != s_lastRawPressed) {
    s_lastRawPressed = rawPressed;
    s_lastEdgeMs = now;
  }

  if (now - s_lastEdgeMs < NAV_BUTTON_DEBOUNCE_MS) {
    return;
  }

  if (rawPressed == s_stablePressed) {
    if (s_stablePressed && !s_longPressFired &&
        now - s_pressedSinceMs >= NAV_BUTTON_LONG_PRESS_MS) {
      s_longPressFired = true;
      s_longPressPending = true;
    }
    return;
  }

  s_stablePressed = rawPressed;

  if (s_stablePressed) {
    s_pressedSinceMs = now;
    s_longPressFired = false;
  } else {
    if (!s_longPressFired) {
      s_shortPressPending = true;
    }
    s_pressedSinceMs = 0;
  }
}

bool nav_button_was_pressed() {
  nav_button_update();
  if (!s_shortPressPending) return false;
  s_shortPressPending = false;
  return true;
}

bool nav_button_was_long_pressed() {
  nav_button_update();
  if (!s_longPressPending) return false;
  s_longPressPending = false;
  return true;
}
