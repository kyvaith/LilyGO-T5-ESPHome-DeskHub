#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/version.h"
#include "esphome/components/display/display_buffer.h"

#include "epd_driver.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

namespace esphome {
namespace t547 {

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2023, 12, 0)
class T547 : public display::DisplayBuffer {
#else
class T547 : public PollingComponent, public display::DisplayBuffer {
#endif  // VERSION_CODE(2023, 12, 0)
 public:
  void set_greyscale(bool greyscale) {
    this->greyscale_ = greyscale;
  }
  void set_fast_refresh(bool fast_refresh) { this->fast_refresh_ = fast_refresh; }
  void set_full_update_every(uint32_t full_update_every) { this->full_update_every_ = full_update_every; }
  void set_partial_clear_cycles(uint8_t partial_clear_cycles) { this->partial_clear_cycles_ = partial_clear_cycles; }
  void set_partial_threshold_percent(uint8_t partial_threshold_percent) {
    this->partial_threshold_percent_ = partial_threshold_percent;
  }

  float get_setup_priority() const override;

  void dump_config() override;

  void display();
  void clean();
  void update() override;

  void setup() override;

  uint8_t get_panel_state() { return this->panel_on_; }
  bool get_greyscale() { return this->greyscale_; }

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2022,6,0)
  display::DisplayType get_display_type() override {
    return get_greyscale() ? display::DisplayType::DISPLAY_TYPE_GRAYSCALE : display::DisplayType::DISPLAY_TYPE_BINARY;
  }
#endif

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  void eink_off_();
  void eink_on_();


  int get_width_internal() override { return 960; }

  int get_height_internal() override { return 540; }

  size_t get_buffer_length_();
  bool find_dirty_area_(Rect_t *area);
  void copy_area_to_partial_(Rect_t area);
  void display_full_();
  void display_partial_(Rect_t area);
  bool allocate_psram_buffer_(uint8_t **target, size_t size, const char *name);


  uint8_t panel_on_ = 0;
  uint8_t temperature_;

  bool greyscale_{false};
  bool fast_refresh_{true};
  bool first_update_{true};
  uint32_t update_count_{0};
  uint32_t full_update_every_{24};
  uint8_t partial_clear_cycles_{1};
  uint8_t partial_threshold_percent_{70};
  uint8_t *previous_buffer_{nullptr};
  uint8_t *partial_buffer_{nullptr};

};

}  // namespace T547
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
