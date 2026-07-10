#pragma once

#include "esphome/core/preferences.h"
#include <esp_sleep.h>
#include <esp_system.h>
#include <string>

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

static inline const char *t5_sleep_wakeup_cause_name() {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0:
      return "ext0";
    case ESP_SLEEP_WAKEUP_EXT1:
      return "ext1";
    case ESP_SLEEP_WAKEUP_TIMER:
      return "timer";
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      return "touchpad";
    case ESP_SLEEP_WAKEUP_ULP:
      return "ulp";
    case ESP_SLEEP_WAKEUP_GPIO:
      return "gpio";
    case ESP_SLEEP_WAKEUP_UART:
      return "uart";
    case ESP_SLEEP_WAKEUP_WIFI:
      return "wifi";
    case ESP_SLEEP_WAKEUP_COCPU:
      return "cocpu";
    case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
      return "cocpu_trap";
    case ESP_SLEEP_WAKEUP_BT:
      return "bt";
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
      return "undefined";
  }
}

static inline const char *t5_reset_reason_name() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      return "poweron";
    case ESP_RST_EXT:
      return "external";
    case ESP_RST_SW:
      return "software";
    case ESP_RST_PANIC:
      return "panic";
    case ESP_RST_INT_WDT:
      return "int_wdt";
    case ESP_RST_TASK_WDT:
      return "task_wdt";
    case ESP_RST_WDT:
      return "wdt";
    case ESP_RST_DEEPSLEEP:
      return "deepsleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "sdio";
    case ESP_RST_USB:
      return "usb";
    case ESP_RST_JTAG:
      return "jtag";
    case ESP_RST_EFUSE:
      return "efuse";
    case ESP_RST_PWR_GLITCH:
      return "power_glitch";
    case ESP_RST_CPU_LOCKUP:
      return "cpu_lockup";
    case ESP_RST_UNKNOWN:
    default:
      return "unknown";
  }
}

static inline std::string t5_boot_diagnostic(bool preserve_armed) {
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "reset=%s wake=%s rtc_preserve=%s flash_preserve=%s",
           t5_reset_reason_name(), t5_sleep_wakeup_cause_name(),
           t5_preserve_screen_after_sleep ? "yes" : "no", preserve_armed ? "yes" : "no");
  return std::string(buffer);
}

static inline void t5_sync_preferences() {
  if (esphome::global_preferences != nullptr) {
    esphome::global_preferences->sync();
  }
}
