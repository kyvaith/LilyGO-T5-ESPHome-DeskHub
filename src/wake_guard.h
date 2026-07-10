#pragma once

#include <esp_sleep.h>

RTC_DATA_ATTR static bool t5_preserve_screen_after_sleep = false;

static inline bool t5_woke_from_screen_preserving_sleep() {
  return t5_preserve_screen_after_sleep || esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;
}

static inline bool t5_allow_automatic_screen_refresh() {
  return !t5_woke_from_screen_preserving_sleep();
}

static inline void t5_mark_screen_preserving_sleep() {
  t5_preserve_screen_after_sleep = true;
}

static inline void t5_clear_screen_preserving_sleep() {
  t5_preserve_screen_after_sleep = false;
}
