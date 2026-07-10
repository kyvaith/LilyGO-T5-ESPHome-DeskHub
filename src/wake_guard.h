#pragma once

#include "esphome/core/preferences.h"
#include "esphome/core/log.h"
#include <esp_sleep.h>
#include <esp_system.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <string>

RTC_DATA_ATTR static bool t5_preserve_screen_after_sleep = false;
static constexpr uint32_t T5_SCREEN_PRESERVE_PREF_KEY = 0x54535053UL;
static constexpr const char *T5_SCREEN_PRESERVE_NVS_NAMESPACE = "t5desk";
static constexpr const char *T5_SCREEN_PRESERVE_NVS_KEY = "preserve";

static inline bool t5_open_preserve_nvs(nvs_handle_t *handle) {
  esp_err_t err = nvs_open(T5_SCREEN_PRESERVE_NVS_NAMESPACE, NVS_READWRITE, handle);
  if (err == ESP_ERR_NVS_NOT_INITIALIZED) {
    nvs_flash_init();
    err = nvs_open(T5_SCREEN_PRESERVE_NVS_NAMESPACE, NVS_READWRITE, handle);
  }
  return err == ESP_OK;
}

static inline bool t5_nvs_screen_preserve_armed() {
  nvs_handle_t handle;
  if (!t5_open_preserve_nvs(&handle)) {
    return false;
  }
  uint8_t value = 0;
  const esp_err_t err = nvs_get_u8(handle, T5_SCREEN_PRESERVE_NVS_KEY, &value);
  nvs_close(handle);
  return err == ESP_OK && value != 0;
}

static inline bool t5_set_nvs_screen_preserve_armed(bool armed) {
  nvs_handle_t handle;
  if (!t5_open_preserve_nvs(&handle)) {
    ESP_LOGW("t5.wake", "NVS open failed while setting screen preserve flag");
    return false;
  }
  esp_err_t err = nvs_set_u8(handle, T5_SCREEN_PRESERVE_NVS_KEY, armed ? 1 : 0);
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }
  nvs_close(handle);
  if (err != ESP_OK) {
    ESP_LOGW("t5.wake", "NVS commit failed while setting screen preserve flag: %s", esp_err_to_name(err));
    return false;
  }
  const bool verified = t5_nvs_screen_preserve_armed() == armed;
  if (!verified) {
    ESP_LOGW("t5.wake", "NVS verify failed after setting screen preserve flag to %s", armed ? "true" : "false");
  }
  return verified;
}

static inline esphome::ESPPreferenceObject t5_screen_preserve_preference() {
  if (esphome::global_preferences == nullptr) {
    return esphome::ESPPreferenceObject();
  }
  return esphome::global_preferences->make_preference<bool>(T5_SCREEN_PRESERVE_PREF_KEY, true);
}

static inline bool t5_preference_screen_preserve_armed() {
  bool armed = false;
  auto pref = t5_screen_preserve_preference();
  pref.load(&armed);
  return armed;
}

static inline bool t5_flash_screen_preserve_armed() {
  return t5_preference_screen_preserve_armed() || t5_nvs_screen_preserve_armed();
}

static inline bool t5_set_flash_screen_preserve_armed(bool armed) {
  auto pref = t5_screen_preserve_preference();
  bool saved = pref.save(&armed);
  if (esphome::global_preferences != nullptr) {
    saved = esphome::global_preferences->sync() && saved;
  }
  const bool nvs_saved = t5_set_nvs_screen_preserve_armed(armed);
  ESP_LOGW("t5.wake", "Screen preserve flag set to %s: pref=%s nvs=%s",
           armed ? "true" : "false", saved ? "ok" : "fail", nvs_saved ? "ok" : "fail");
  return saved && nvs_saved;
}

static inline bool t5_woke_from_screen_preserving_sleep() {
  return t5_preserve_screen_after_sleep || t5_flash_screen_preserve_armed() ||
         esp_reset_reason() == ESP_RST_DEEPSLEEP;
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
  const bool pref_preserve = preserve_armed || t5_preference_screen_preserve_armed();
  const bool nvs_preserve = t5_nvs_screen_preserve_armed();
  const bool flash_preserve = pref_preserve || nvs_preserve;
  const bool screen_hold = t5_woke_from_screen_preserving_sleep();
  char buffer[224];
  snprintf(buffer, sizeof(buffer),
           "reset=%s wake=%s rtc_preserve=%s pref_preserve=%s nvs_preserve=%s flash_preserve=%s screen_hold=%s",
           t5_reset_reason_name(), t5_sleep_wakeup_cause_name(),
           t5_preserve_screen_after_sleep ? "yes" : "no", pref_preserve ? "yes" : "no",
           nvs_preserve ? "yes" : "no", flash_preserve ? "yes" : "no", screen_hold ? "yes" : "no");
  return std::string(buffer);
}

static inline void t5_sync_preferences() {
  if (esphome::global_preferences != nullptr) {
    esphome::global_preferences->sync();
  }
}

static inline void t5_enter_screen_preserving_deep_sleep(uint64_t sleep_duration_us) {
  t5_mark_screen_preserving_sleep();
  const bool saved = t5_set_flash_screen_preserve_armed(true);
  t5_sync_preferences();
  ESP_LOGW("t5.wake", "Entering direct ESP deep sleep for %llu us (preserve save=%s)",
           static_cast<unsigned long long>(sleep_duration_us), saved ? "ok" : "fail");
  delay(250);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_timer_wakeup(sleep_duration_us);
  esp_deep_sleep_start();
}

static inline bool t5_enter_light_sleep(uint64_t sleep_duration_us) {
  ESP_LOGW("t5.wake", "Entering ESP light sleep for %llu us",
           static_cast<unsigned long long>(sleep_duration_us));
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_timer_wakeup(sleep_duration_us);
  const esp_err_t err = esp_light_sleep_start();
  if (err != ESP_OK) {
    ESP_LOGW("t5.wake", "Light sleep failed: %s", esp_err_to_name(err));
    return false;
  }
  ESP_LOGW("t5.wake", "Returned from ESP light sleep");
  return true;
}
