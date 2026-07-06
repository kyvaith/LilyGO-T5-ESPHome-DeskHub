#include "t547.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include <esp32-hal-gpio.h>
#include <algorithm>
#include <cstring>

namespace esphome {
namespace t547 {

static const char *const TAG = "t547";

void T547::setup() {
  ESP_LOGV(TAG, "Initialize called");
  epd_init();
  uint32_t buffer_size = this->get_buffer_length_();

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

  memset(this->buffer_, 0xFF, buffer_size);
  memset(this->previous_buffer_, 0xFF, buffer_size);
  memset(this->partial_buffer_, 0xFF, buffer_size);
  ESP_LOGV(TAG, "Initialize complete");
}

float T547::get_setup_priority() const { return setup_priority::PROCESSOR; }
size_t T547::get_buffer_length_() {
    return this->get_width_internal() * this->get_height_internal() / 2;
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

  constexpr int margin = 48;
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

}  // namespace T547
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
