#pragma once
#include <stdint.h>

void nav_button_init();
bool nav_button_is_pressed();
uint32_t nav_button_press_elapsed_ms();
uint32_t nav_button_long_press_ms();
bool nav_button_was_pressed();
bool nav_button_was_long_pressed();
