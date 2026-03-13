#pragma once

#include <array>
#include <cstdint>

#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/display/display.h"
#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "esphome/core/string_ref.h"

#include "thermostat_ui.h"

namespace esphome
{
  namespace m5dial_thermostat
  {

    class M5DialThermostat;

    class UnitSelect : public select::Select, public Component
    {
    public:
      void set_parent(M5DialThermostat *parent) { this->parent_ = parent; }

    protected:
      void control(const std::string &value) override;

      M5DialThermostat *parent_{nullptr};
    };

    class M5DialThermostat : public Component, public api::CustomAPIDevice
    {
    public:
      static constexpr int kMaxSupportedModes = 8;

      void set_entity_id(const std::string &entity_id) { this->entity_id_ = entity_id; }
      void set_display(display::Display *display) { this->display_ = display; }
      void set_font_mode(display::BaseFont *font) { this->font_mode_ = font; }
      void set_font_setpoint(display::BaseFont *font)
      {
        this->font_setpoint_ = font;
      }
      void set_font_temp(display::BaseFont *font) { this->font_temp_ = font; }
      void set_font_error(display::BaseFont *font) { this->font_error_ = font; }
      void set_unit_select(UnitSelect *unit_select) { this->unit_select_ = unit_select; }
      void set_active_brightness(int brightness)
      {
        this->active_brightness_ = static_cast<uint8_t>(brightness);
      }
      void set_idle_brightness(int brightness)
      {
        this->idle_brightness_ = static_cast<uint8_t>(brightness);
      }
      void set_idle_timeout(uint32_t timeout_ms) { this->idle_timeout_ms_ = timeout_ms; }
      void set_comms_timeout(uint32_t timeout_ms) { this->comms_timeout_ms_ = timeout_ms; }
      void set_enable_sounds(bool enable_sounds) { this->enable_sounds_ = enable_sounds; }

      void set_fahrenheit(bool fahrenheit) { this->display_fahrenheit_ = fahrenheit; }

      void setup() override;
      void loop() override;
      void dump_config() override;

      float get_setup_priority() const override
      {
#ifdef DEBUG_TEST
        return setup_priority::HARDWARE;
#else
        return setup_priority::AFTER_CONNECTION;
#endif
      }

      static HvacMode parse_hvac_mode(const char *state);
      static HvacAction parse_hvac_action(const char *state);

    protected:
      void on_hvac_mode(StringRef state);
      void on_current_temp(StringRef state);
      void on_target_temp(StringRef state);
      void on_hvac_action(StringRef state);
      void on_supported_modes(StringRef state);
      void on_min_temp(StringRef state);
      void on_max_temp(StringRef state);
      void on_temp_step(StringRef state);

      void setup_input_pins_();
      static void encoder_isr_handler_(void *arg);
      void process_encoder_counts_();
      void on_encoder_tick_(int direction);
      void on_button_tick_(int button_state);
      void on_mode_button_();
      int find_mode_index_(HvacMode mode) const;

      void subscribe_ha_state_();
      void set_writer_();
      void render_(display::Display &it);
      void setup_backlight_();
      void set_backlight_level_direct_(uint8_t level);
      void setup_buzzer_();
      void start_buzzer_tone_(uint32_t frequency_hz);
      void stop_buzzer_tone_();
      void set_display_brightness_(bool active);
      void set_backlight_level_(uint8_t level);
      void send_setpoint_to_ha_();
      void send_mode_to_ha_(HvacMode mode);
      bool parse_float_(StringRef value, float *out) const;
      void parse_supported_modes_(const char *value);
      void play_sound_(const char *tone);
      bool update_comms_timeout_state_(uint32_t now_ms);
      void process_user_input_(bool allow_user_input);
      void apply_backlight_policy_(uint32_t now_ms);
      void update_no_connection_animation_(uint32_t now_ms);
      bool try_redraw_(uint32_t now_ms);

      static constexpr uint8_t kEncoderPinA = 40;
      static constexpr uint8_t kEncoderPinB = 41;
      static constexpr uint8_t kButtonPin = 42;
      static constexpr uint8_t kHoldPin = 46;

      // Number of raw quadrature counts required per tick (half-quad
      // equivalent): each mechanical detent produces ~2 counts.
      static constexpr int8_t kEncoderCountsPerTick = 2;
      static constexpr uint8_t kMaxRedrawHz = 30;
      static constexpr uint16_t kRedrawIntervalMs = 1000 / kMaxRedrawHz;
      static constexpr uint16_t kButtonDebounceMs = 60;
      static constexpr uint16_t kSetpointDebounceMs = 500;
      static constexpr uint8_t kNoConnectionAnimHz = 10;
      static constexpr uint16_t kNoConnectionAnimIntervalMs =
          1000 / kNoConnectionAnimHz;
      static constexpr uint16_t kNoConnectionSpinnerStepDeg = 18;

      static constexpr int8_t kEncoderTable[4][4] = {
          {0, -1, 1, 0},
          {1, 0, 0, -1},
          {-1, 0, 0, 1},
          {0, 1, -1, 0},
      };

      std::string entity_id_;
      display::Display *display_{nullptr};
      display::BaseFont *font_mode_{nullptr};
      display::BaseFont *font_setpoint_{nullptr};
      display::BaseFont *font_temp_{nullptr};
      display::BaseFont *font_error_{nullptr};
      UnitSelect *unit_select_{nullptr};

      float current_temp_{NAN};
      float target_temp_{NAN};
      float local_setpoint_{NAN};
      bool local_setpoint_dirty_{false};
      HvacMode hvac_mode_{HvacMode::kOff};
      HvacAction hvac_action_{HvacAction::kUnknown};

      std::array<HvacMode, kMaxSupportedModes> supported_modes_{};
      int supported_modes_count_{0};
      float min_temp_{15.0f};
      float max_temp_{30.0f};
      float temp_step_{0.5f};

      bool display_fahrenheit_{false};
      bool comms_ok_{false};
      bool needs_redraw_{false};
      bool enable_sounds_{true};

      uint32_t last_ha_update_{0};
      uint32_t last_interaction_{0};
      uint32_t last_button_ms_{0};
      uint32_t last_redraw_ms_{0};
      uint32_t last_no_connection_anim_ms_{0};
      float reconnect_spinner_start_deg_{0.0f};

      volatile uint8_t encoder_isr_state_{0};
      volatile int32_t encoder_delta_counts_{0};
      // Accumulates raw quadrature deltas; fires a tick every
      // kEncoderCountsPerTick counts to align with detents.
      int32_t encoder_accumulator_{0};
      int prev_button_state_{1};

      uint8_t active_brightness_{255};
      uint8_t idle_brightness_{50};
      uint32_t idle_timeout_ms_{30000};
      uint32_t comms_timeout_ms_{30000};
      bool backlight_ready_{false};
      bool buzzer_ready_{false};
      int16_t last_backlight_level_{-1};
    };

  } // namespace m5dial_thermostat
} // namespace esphome
