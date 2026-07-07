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
  void set_fast_mono(bool fast_mono) { this->fast_mono_ = fast_mono; }
  void set_fast_mono_passes(uint8_t fast_mono_passes) { this->fast_mono_passes_ = fast_mono_passes; }
  void set_fast_mono_clear_passes(uint8_t fast_mono_clear_passes) {
    this->fast_mono_clear_passes_ = fast_mono_clear_passes;
  }
  void set_fast_mono_threshold(uint8_t fast_mono_threshold) { this->fast_mono_threshold_ = fast_mono_threshold; }
  void set_fast_mono_time(uint16_t fast_mono_time) { this->fast_mono_time_ = fast_mono_time; }
  void set_fast_mono_clear_time(uint16_t fast_mono_clear_time) { this->fast_mono_clear_time_ = fast_mono_clear_time; }
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
  bool display_fast_mono_(Rect_t area);
  bool build_fast_mono_row_(Rect_t area, int y, uint8_t pass, uint8_t *row, uint32_t *row_time);
  bool mono_pixel_is_black_(int x, int y);
  bool previous_mono_pixel_is_black_(int x, int y);
  bool buffer_mono_pixel_is_black_(const uint8_t *buffer, int x, int y);
  uint8_t required_mono_passes_(bool target_black) const;
  uint16_t mono_drive_time_(bool target_black) const;
  size_t get_mono_state_length_();
  void sync_mono_state_from_buffer_();
  void output_mono_noop_row_(uint32_t pipeline_finish_time);
  void reorder_mono_line_(uint32_t *line_data);
  bool allocate_psram_buffer_(uint8_t **target, size_t size, const char *name);


  uint8_t panel_on_ = 0;
  uint8_t temperature_;

  bool greyscale_{false};
  bool fast_refresh_{true};
  bool fast_mono_{false};
  bool first_update_{true};
  uint32_t update_count_{0};
  uint32_t full_update_every_{24};
  uint8_t partial_clear_cycles_{1};
  uint8_t partial_threshold_percent_{70};
  uint8_t fast_mono_passes_{4};
  uint8_t fast_mono_clear_passes_{10};
  uint8_t fast_mono_threshold_{14};
  uint16_t fast_mono_time_{300};
  uint16_t fast_mono_clear_time_{550};
  uint32_t mono_skipping_{0};
  uint8_t *previous_buffer_{nullptr};
  uint8_t *partial_buffer_{nullptr};
  uint8_t *mono_state_buffer_{nullptr};

};

}  // namespace T547
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
