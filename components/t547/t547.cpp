#include "t547.h"
#include "ed047tc1.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include <esp32-hal-gpio.h>
#include <Arduino.h>
#include <algorithm>
#include <cstring>

namespace esphome {
namespace t547 {

static const char *const TAG = "t547";
static constexpr int EPD_FAST_LINE_BYTES = EPD_WIDTH / 4;
static constexpr uint8_t FAST_MONO_TARGET_BLACK = 0x01;
static constexpr uint8_t FAST_MONO_COMMAND_DARK = 0x01;
static constexpr uint8_t FAST_MONO_COMMAND_LIGHT = 0x02;

void T547::setup() {
  ESP_LOGV(TAG, "Initialize called");
  epd_init();
  uint32_t buffer_size = this->get_buffer_length_();
  uint32_t mono_state_size = this->get_mono_state_length_();

  if (this->buffer_ != nullptr) {
    free(this->buffer_);  // NOLINT
  }

  if (!this->allocate_psram_buffer_(&this->buffer_, buffer_size, "display")) {
    this->mark_failed();
    return;
  }
  if (!this->allocate_psram_buffer_(&this->previous_buffer_, buffer_size, "previous")) {
    this->mark_failed();
    return;
  }
  if (!this->allocate_psram_buffer_(&this->partial_buffer_, buffer_size, "partial")) {
    this->mark_failed();
    return;
  }
  if (this->fast_mono_ &&
      !this->allocate_psram_buffer_(&this->mono_state_buffer_, mono_state_size, "fast mono state")) {
    this->mark_failed();
    return;
  }

  memset(this->buffer_, 0xFF, buffer_size);
  memset(this->previous_buffer_, 0xFF, buffer_size);
  memset(this->partial_buffer_, 0xFF, buffer_size);
  if (this->mono_state_buffer_ != nullptr) {
    memset(this->mono_state_buffer_, this->fast_mono_clear_passes_ << 1, mono_state_size);
  }
  ESP_LOGV(TAG, "Initialize complete");
}

float T547::get_setup_priority() const { return setup_priority::PROCESSOR; }
size_t T547::get_buffer_length_() {
    return this->get_width_internal() * this->get_height_internal() / 2;
}

size_t T547::get_mono_state_length_() {
    return this->get_width_internal() * this->get_height_internal();
}

bool T547::allocate_psram_buffer_(uint8_t **target, size_t size, const char *name) {
  if (*target != nullptr) {
    free(*target);  // NOLINT
  }
  *target = (uint8_t *) ps_malloc(size);
  if (*target == nullptr) {
    ESP_LOGE(TAG, "Could not allocate %s buffer (%u bytes)", name, static_cast<unsigned>(size));
    return false;
  }
  return true;
}

void T547::update() {
  this->do_update_();
  this->display();
}

void HOT T547::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;
  uint8_t gs = (color.red * 2126 / 10000) + (color.green * 7152 / 10000) + (color.blue * 722 / 10000);
  epd_draw_pixel(x, y, gs, this->buffer_);
}

void T547::dump_config() {
  LOG_DISPLAY("", "T547", this);
  LOG_UPDATE_INTERVAL(this);
}

void T547::eink_off_() {
  ESP_LOGV(TAG, "Eink off called");
  if (panel_on_ == 0)
    return;
  epd_poweroff();
  panel_on_ = 0;
}

void T547::eink_on_() {
  ESP_LOGV(TAG, "Eink on called");
  if (panel_on_ == 1)
    return;
  epd_poweron();
  panel_on_ = 1;
}

void T547::display() {
  ESP_LOGV(TAG, "Display called");
  uint32_t start_time = millis();

  Rect_t dirty;
  const uint32_t screen_area = this->get_width_internal() * this->get_height_internal();
  const bool has_dirty = this->find_dirty_area_(&dirty);
  if (!has_dirty && !this->first_update_) {
    ESP_LOGV(TAG, "Display skipped, no dirty pixels");
    return;
  }

  const bool periodic_full_due = this->full_update_every_ != 0 &&
                                 this->update_count_ >= this->full_update_every_;
  const bool dirty_too_large = this->partial_threshold_percent_ < 100 &&
                               dirty.width * dirty.height * 100u >
                                   screen_area * static_cast<uint32_t>(this->partial_threshold_percent_);
  const bool force_full = this->first_update_ || !this->fast_refresh_ ||
                          periodic_full_due || dirty_too_large;

  if (force_full) {
    this->display_full_();
    ESP_LOGI(TAG, "Display finished (full) (%ums)", millis() - start_time);
    return;
  }

  if (this->fast_mono_ && this->mono_state_buffer_ != nullptr) {
    const bool any_pixel_driven = this->display_fast_mono_(dirty);
    memcpy(this->previous_buffer_, this->buffer_, this->get_buffer_length_());
    this->update_count_++;
    ESP_LOGI(TAG, "Display finished (fast mono %dx%d at %d,%d, driven=%s) (%ums)",
             dirty.width, dirty.height, dirty.x, dirty.y, any_pixel_driven ? "yes" : "no",
             millis() - start_time);
    return;
  }

  this->display_partial_(dirty);
  ESP_LOGI(TAG, "Display finished (partial %dx%d at %d,%d) (%ums)",
           dirty.width, dirty.height, dirty.x, dirty.y, millis() - start_time);
}

bool T547::find_dirty_area_(Rect_t *area) {
  if (this->previous_buffer_ == nullptr || this->buffer_ == nullptr) {
    return false;
  }

  const int width = this->get_width_internal();
  const int height = this->get_height_internal();
  const int stride = width / 2;
  int min_byte = stride;
  int max_byte = -1;
  int min_y = height;
  int max_y = -1;

  for (int y = 0; y < height; y++) {
    const uint8_t *current = this->buffer_ + y * stride;
    const uint8_t *previous = this->previous_buffer_ + y * stride;
    for (int bx = 0; bx < stride; bx++) {
      if (current[bx] == previous[bx]) {
        continue;
      }
      min_byte = std::min(min_byte, bx);
      max_byte = std::max(max_byte, bx);
      min_y = std::min(min_y, y);
      max_y = std::max(max_y, y);
    }
  }

  if (max_byte < min_byte || max_y < min_y) {
    return false;
  }

  constexpr int margin = 12;
  int x = std::max(0, min_byte * 2 - margin);
  int right = std::min(width, (max_byte + 1) * 2 + margin);
  int y = std::max(0, min_y - margin);
  int bottom = std::min(height, max_y + 1 + margin);

  x &= ~1;
  if (right & 1) {
    right++;
  }
  right = std::min(width, right);

  area->x = x;
  area->y = y;
  area->width = std::max(2, right - x);
  area->height = std::max(1, bottom - y);
  return true;
}

void T547::copy_area_to_partial_(Rect_t area) {
  const int stride = this->get_width_internal() / 2;
  const int row_bytes = area.width / 2;
  const int src_x = area.x / 2;
  for (int y = 0; y < area.height; y++) {
    const uint8_t *src = this->buffer_ + (area.y + y) * stride + src_x;
    uint8_t *dst = this->partial_buffer_ + y * row_bytes;
    memcpy(dst, src, row_bytes);
  }
}

void T547::display_full_() {
  epd_poweron();
  epd_clear();
  epd_draw_grayscale_image(epd_full_screen(), this->buffer_);
  epd_poweroff();
  memcpy(this->previous_buffer_, this->buffer_, this->get_buffer_length_());
  this->sync_mono_state_from_buffer_();
  this->first_update_ = false;
  this->update_count_ = 1;
}

void T547::display_partial_(Rect_t area) {
  this->copy_area_to_partial_(area);
  epd_poweron();
  if (this->partial_clear_cycles_ > 0) {
    epd_clear_area_cycles(area, this->partial_clear_cycles_, 25);
  }
  epd_draw_grayscale_image(area, this->partial_buffer_);
  epd_poweroff();
  memcpy(this->previous_buffer_, this->buffer_, this->get_buffer_length_());
  this->update_count_++;
}

bool T547::buffer_mono_pixel_is_black_(const uint8_t *buffer, int x, int y) {
  if (buffer == nullptr) {
    return false;
  }
  const int stride = this->get_width_internal() / 2;
  const uint8_t packed = buffer[y * stride + x / 2];
  const uint8_t nibble = (x & 1) ? (packed >> 4) : (packed & 0x0F);
  return nibble < this->fast_mono_threshold_;
}

bool T547::mono_pixel_is_black_(int x, int y) {
  return this->buffer_mono_pixel_is_black_(this->buffer_, x, y);
}

bool T547::previous_mono_pixel_is_black_(int x, int y) {
  return this->buffer_mono_pixel_is_black_(this->previous_buffer_, x, y);
}

uint8_t T547::required_mono_passes_(bool target_black) const {
  return target_black ? this->fast_mono_passes_ : this->fast_mono_clear_passes_;
}

uint16_t T547::mono_drive_time_(bool target_black) const {
  return target_black ? this->fast_mono_time_ : this->fast_mono_clear_time_;
}

void T547::sync_mono_state_from_buffer_() {
  if (this->mono_state_buffer_ == nullptr) {
    return;
  }
  const int width = this->get_width_internal();
  const int height = this->get_height_internal();
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      const bool black = this->mono_pixel_is_black_(x, y);
      const uint8_t saturated = this->required_mono_passes_(black) << 1;
      this->mono_state_buffer_[y * width + x] = saturated | (black ? FAST_MONO_TARGET_BLACK : 0);
    }
  }
}

void T547::reorder_mono_line_(uint32_t *line_data) {
  for (uint32_t i = 0; i < EPD_FAST_LINE_BYTES / 4; i++) {
    uint32_t val = *line_data;
    *(line_data++) = val >> 16 | ((val & 0x0000FFFF) << 16);
  }
}

void T547::output_mono_noop_row_(uint32_t pipeline_finish_time) {
  if (this->mono_skipping_ == 0) {
    epd_switch_buffer();
    memset(epd_get_current_buffer(), 0, EPD_FAST_LINE_BYTES);
    epd_switch_buffer();
    memset(epd_get_current_buffer(), 0, EPD_FAST_LINE_BYTES);
    epd_output_row(pipeline_finish_time);
  } else if (this->mono_skipping_ < 2) {
    epd_output_row(10);
  } else {
    epd_skip();
  }
  this->mono_skipping_++;
}

bool T547::build_fast_mono_row_(Rect_t area, int y, uint8_t pass, uint8_t *row, uint32_t *row_time) {
  memset(row, 0, EPD_FAST_LINE_BYTES);

  const int width = this->get_width_internal();
  const int start_x = std::max(0, static_cast<int>(area.x));
  const int end_x = std::min(width, static_cast<int>(area.x + area.width));
  bool any = false;
  uint32_t max_time = this->fast_mono_time_;

  for (int x = start_x; x < end_x; x++) {
    const bool target_black = this->mono_pixel_is_black_(x, y);
    const bool previous_buffer_black = this->previous_mono_pixel_is_black_(x, y);
    const size_t index = static_cast<size_t>(y) * width + x;
    uint8_t state = this->mono_state_buffer_[index];
    uint8_t count = state >> 1;
    const bool previous_target_black = (state & FAST_MONO_TARGET_BLACK) != 0;

    if (target_black != previous_target_black || target_black != previous_buffer_black) {
      count = 0;
    }

    const uint8_t required_passes = this->required_mono_passes_(target_black);
    if (count < required_passes) {
      const uint8_t command = target_black ? FAST_MONO_COMMAND_DARK : FAST_MONO_COMMAND_LIGHT;
      row[x / 4] |= command << (2 * (x & 0x03));
      count++;
      any = true;
      max_time = std::max(max_time, static_cast<uint32_t>(this->mono_drive_time_(target_black)));
    }

    this->mono_state_buffer_[index] = (count << 1) | (target_black ? FAST_MONO_TARGET_BLACK : 0);
  }

  if (any) {
    *row_time = max_time;
    this->reorder_mono_line_(reinterpret_cast<uint32_t *>(row));
  }
  return any;
}

bool T547::display_fast_mono_(Rect_t area) {
  uint8_t row[EPD_FAST_LINE_BYTES];
  bool any_pixel_driven = false;
  const uint8_t max_passes = std::max(this->fast_mono_passes_, this->fast_mono_clear_passes_);
  const uint32_t noop_time = this->fast_mono_time_;

  epd_poweron();
  for (uint8_t pass = 0; pass < max_passes; pass++) {
    bool any_in_pass = false;
    this->mono_skipping_ = 0;
    epd_start_frame();

    for (int y = 0; y < this->get_height_internal(); y++) {
      if (y < area.y || y >= area.y + area.height) {
        this->output_mono_noop_row_(noop_time);
        continue;
      }

      uint32_t row_time = this->fast_mono_time_;
      if (this->build_fast_mono_row_(area, y, pass, row, &row_time)) {
        if (this->mono_skipping_ > 0) {
          epd_switch_buffer();
          memcpy(epd_get_current_buffer(), row, EPD_FAST_LINE_BYTES);
          epd_switch_buffer();
        }
        memcpy(epd_get_current_buffer(), row, EPD_FAST_LINE_BYTES);
        this->mono_skipping_ = 0;
        epd_output_row(row_time);
        any_in_pass = true;
        any_pixel_driven = true;
      } else {
        this->output_mono_noop_row_(noop_time);
      }
    }

    if (this->mono_skipping_ == 0) {
      epd_output_row(noop_time);
    }
    epd_end_frame();
    App.feed_wdt();
    delay(1);

    if (!any_in_pass) {
      break;
    }
  }
  epd_poweroff();
  return any_pixel_driven;
}

}  // namespace T547
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
