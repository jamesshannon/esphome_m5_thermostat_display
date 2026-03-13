#include "m5dial_thermostat.h"

#include <algorithm>
#include <map>
#include <cstdlib>
#include <cstring>
#include <esp_err.h>
#include <driver/gpio.h>
#include <driver/ledc.h>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "runtime_logic.h"

namespace
{
  constexpr char kModeOff[] = "off";
  constexpr char kModeHeat[] = "heat";
  constexpr char kModeCool[] = "cool";
  constexpr char kModeHeatCool[] = "heat_cool";
  constexpr char kModeFanOnly[] = "fan_only";
  constexpr char kModeAuto[] = "auto";
  constexpr char kModeDry[] = "dry";

  constexpr char kActionHeating[] = "heating";
  constexpr char kActionCooling[] = "cooling";
  constexpr char kActionDrying[] = "drying";
  constexpr char kActionFan[] = "fan";
  constexpr char kActionIdle[] = "idle";

  constexpr char kNoteRotateUp[] = "up:d=64,o=6,b=255:c";
  constexpr char kNoteRotateDown[] = "down:d=64,o=4,b=255:c";
  constexpr char kNoteClick[] = "click:d=64,o=5,b=255:c,p,c";

  constexpr uint8_t kPinModeInputPullup = static_cast<uint8_t>(GPIO_MODE_INPUT);
  constexpr uint8_t kPinModeOutput = static_cast<uint8_t>(GPIO_MODE_OUTPUT);
  constexpr bool kBacklightActiveLow = false;
  constexpr gpio_num_t kBacklightPin = GPIO_NUM_9;
  constexpr uint32_t kBacklightPwmFrequencyHz = 5000;
  constexpr ledc_mode_t kBacklightMode = LEDC_LOW_SPEED_MODE;
  constexpr ledc_timer_t kBacklightTimer = LEDC_TIMER_2;
  constexpr ledc_channel_t kBacklightChannel = LEDC_CHANNEL_2;
  constexpr ledc_timer_bit_t kBacklightResolution = LEDC_TIMER_10_BIT;
  constexpr gpio_num_t kBuzzerPin = GPIO_NUM_3;
  constexpr uint32_t kBuzzerBaseFrequencyHz = 1800;
  constexpr ledc_mode_t kBuzzerMode = LEDC_LOW_SPEED_MODE;
  constexpr ledc_timer_t kBuzzerTimer = LEDC_TIMER_1;
  constexpr ledc_channel_t kBuzzerChannel = LEDC_CHANNEL_1;
  constexpr ledc_timer_bit_t kBuzzerResolution = LEDC_TIMER_10_BIT;
  // ~6.25% duty cycle (64/1023) keeps click tones short and subtle.
  constexpr uint32_t kBuzzerDuty = 64;

  inline bool is_mode_separator(char c)
  {
    return !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
             (c >= '0' && c <= '9') || c == '_');
  }

  esphome::m5dial_thermostat::HvacMode parse_hvac_mode_span(
      const char *start, size_t len)
  {
    if (start == nullptr)
    {
      return esphome::m5dial_thermostat::HvacMode::kUnknown;
    }
    if (len == sizeof(kModeOff) - 1 && std::strncmp(start, kModeOff, len) == 0)
    {
      return esphome::m5dial_thermostat::HvacMode::kOff;
    }
    if (len == sizeof(kModeHeat) - 1 && std::strncmp(start, kModeHeat, len) == 0)
    {
      return esphome::m5dial_thermostat::HvacMode::kHeat;
    }
    if (len == sizeof(kModeCool) - 1 && std::strncmp(start, kModeCool, len) == 0)
    {
      return esphome::m5dial_thermostat::HvacMode::kCool;
    }
    if (len == sizeof(kModeHeatCool) - 1 &&
        std::strncmp(start, kModeHeatCool, len) == 0)
    {
      return esphome::m5dial_thermostat::HvacMode::kHeatCool;
    }
    if (len == sizeof(kModeFanOnly) - 1 &&
        std::strncmp(start, kModeFanOnly, len) == 0)
    {
      return esphome::m5dial_thermostat::HvacMode::kFanOnly;
    }
    if (len == sizeof(kModeAuto) - 1 && std::strncmp(start, kModeAuto, len) == 0)
    {
      return esphome::m5dial_thermostat::HvacMode::kAuto;
    }
    if (len == sizeof(kModeDry) - 1 && std::strncmp(start, kModeDry, len) == 0)
    {
      return esphome::m5dial_thermostat::HvacMode::kDry;
    }
    return esphome::m5dial_thermostat::HvacMode::kUnknown;
  }

} // namespace

namespace esphome
{
  namespace m5dial_thermostat
  {

    static const char *const TAG = "m5dial_thermostat";

    void UnitSelect::control(const std::string &value)
    {
      this->publish_state(value);
      if (this->parent_ != nullptr)
      {
        this->parent_->set_fahrenheit(value == "fahrenheit");
      }
    }

    HvacMode M5DialThermostat::parse_hvac_mode(const char *state)
    {
      if (state == nullptr)
      {
        return HvacMode::kUnknown;
      }
      return parse_hvac_mode_span(state, std::strlen(state));
    }

    HvacAction M5DialThermostat::parse_hvac_action(const char *state)
    {
      if (state == nullptr)
      {
        return HvacAction::kUnknown;
      }
      if (std::strncmp(state, kModeOff, sizeof(kModeOff) - 1) == 0)
      {
        return HvacAction::kOff;
      }
      if (std::strncmp(state, kActionHeating, sizeof(kActionHeating) - 1) == 0)
      {
        return HvacAction::kHeating;
      }
      if (std::strncmp(state, kActionCooling, sizeof(kActionCooling) - 1) == 0)
      {
        return HvacAction::kCooling;
      }
      if (std::strncmp(state, kActionIdle, sizeof(kActionIdle) - 1) == 0)
      {
        return HvacAction::kIdle;
      }
      if (std::strncmp(state, kActionFan, sizeof(kActionFan) - 1) == 0)
      {
        return HvacAction::kFan;
      }
      if (std::strncmp(state, kActionDrying, sizeof(kActionDrying) - 1) == 0)
      {
        return HvacAction::kDrying;
      }
      return HvacAction::kUnknown;
    }

    void M5DialThermostat::setup_input_pins_()
    {
      gpio_set_direction(static_cast<gpio_num_t>(kEncoderPinA),
                         static_cast<gpio_mode_t>(kPinModeInputPullup));
      gpio_set_pull_mode(static_cast<gpio_num_t>(kEncoderPinA), GPIO_PULLUP_ONLY);

      gpio_set_direction(static_cast<gpio_num_t>(kEncoderPinB),
                         static_cast<gpio_mode_t>(kPinModeInputPullup));
      gpio_set_pull_mode(static_cast<gpio_num_t>(kEncoderPinB), GPIO_PULLUP_ONLY);

      gpio_set_direction(static_cast<gpio_num_t>(kButtonPin),
                         static_cast<gpio_mode_t>(kPinModeInputPullup));
      gpio_set_pull_mode(static_cast<gpio_num_t>(kButtonPin), GPIO_PULLUP_ONLY);

      gpio_set_direction(static_cast<gpio_num_t>(kHoldPin),
                         static_cast<gpio_mode_t>(kPinModeInputPullup));
      gpio_set_pull_mode(static_cast<gpio_num_t>(kHoldPin), GPIO_PULLUP_ONLY);

      this->encoder_isr_state_ =
          static_cast<uint8_t>((gpio_get_level(static_cast<gpio_num_t>(kEncoderPinA)) << 1) |
                               gpio_get_level(static_cast<gpio_num_t>(kEncoderPinB)));
      this->encoder_delta_counts_ = 0;
      this->encoder_accumulator_ = 0;
      this->prev_button_state_ =
          gpio_get_level(static_cast<gpio_num_t>(kButtonPin));

      const esp_err_t install_err = gpio_install_isr_service(0);
      if (install_err != ESP_OK && install_err != ESP_ERR_INVALID_STATE)
      {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: err=%d", install_err);
        return;
      }

      gpio_set_intr_type(static_cast<gpio_num_t>(kEncoderPinA), GPIO_INTR_ANYEDGE);
      gpio_set_intr_type(static_cast<gpio_num_t>(kEncoderPinB), GPIO_INTR_ANYEDGE);

      const esp_err_t add_a_err = gpio_isr_handler_add(
          static_cast<gpio_num_t>(kEncoderPinA),
          &M5DialThermostat::encoder_isr_handler_, this);
      if (add_a_err != ESP_OK)
      {
        ESP_LOGE(TAG, "Failed to register encoder ISR A: err=%d", add_a_err);
      }

      const esp_err_t add_b_err = gpio_isr_handler_add(
          static_cast<gpio_num_t>(kEncoderPinB),
          &M5DialThermostat::encoder_isr_handler_, this);
      if (add_b_err != ESP_OK)
      {
        ESP_LOGE(TAG, "Failed to register encoder ISR B: err=%d", add_b_err);
      }
    }

    void M5DialThermostat::set_backlight_level_(uint8_t level)
    {
      if (this->last_backlight_level_ == static_cast<int16_t>(level))
      {
        return;
      }
      this->last_backlight_level_ = static_cast<int16_t>(level);

      this->set_backlight_level_direct_(level);
#ifdef DEBUG_TEST
      ESP_LOGD(TAG, "Backlight level set to %u", level);
      return;
#endif

      // Backlight is controlled directly through LEDC/GPIO in this component.
      // Avoid driving the same pin through a second LEDCOutput path.
      ESP_LOGD(TAG, "Backlight level set to %u", level);
    }

    void M5DialThermostat::setup_backlight_()
    {
      ledc_timer_config_t timer_conf{};
      timer_conf.speed_mode = kBacklightMode;
      timer_conf.timer_num = kBacklightTimer;
      timer_conf.duty_resolution = kBacklightResolution;
      timer_conf.freq_hz = kBacklightPwmFrequencyHz;
      timer_conf.clk_cfg = LEDC_AUTO_CLK;
      if (ledc_timer_config(&timer_conf) != ESP_OK)
      {
        this->backlight_ready_ = false;
        return;
      }

      ledc_channel_config_t channel_conf{};
      channel_conf.gpio_num = kBacklightPin;
      channel_conf.speed_mode = kBacklightMode;
      channel_conf.channel = kBacklightChannel;
      channel_conf.intr_type = LEDC_INTR_DISABLE;
      channel_conf.timer_sel = kBacklightTimer;
      channel_conf.duty = 0;
      channel_conf.hpoint = 0;
      if (ledc_channel_config(&channel_conf) != ESP_OK)
      {
        this->backlight_ready_ = false;
        return;
      }
      this->backlight_ready_ = true;
    }

    void M5DialThermostat::set_backlight_level_direct_(uint8_t level)
    {
      const uint8_t hw_level = map_backlight_level(level, kBacklightActiveLow);
      if (!this->backlight_ready_)
      {
        // Fallback: force backlight pin as digital output if LEDC setup failed.
        gpio_set_direction(kBacklightPin,
                           static_cast<gpio_mode_t>(kPinModeOutput));
        gpio_set_level(kBacklightPin, hw_level > 0 ? 1 : 0);
        return;
      }
      const uint32_t duty = level_to_ledc_duty_10bit(hw_level);
      ledc_set_duty(kBacklightMode, kBacklightChannel, duty);
      ledc_update_duty(kBacklightMode, kBacklightChannel);
    }

    void M5DialThermostat::set_display_brightness_(bool active)
    {
      const uint8_t level =
          active ? this->active_brightness_ : this->idle_brightness_;
      this->set_backlight_level_(level);
    }

    void M5DialThermostat::setup_buzzer_()
    {
      ledc_timer_config_t timer_conf{};
      timer_conf.speed_mode = kBuzzerMode;
      timer_conf.timer_num = kBuzzerTimer;
      timer_conf.duty_resolution = kBuzzerResolution;
      timer_conf.freq_hz = kBuzzerBaseFrequencyHz;
      timer_conf.clk_cfg = LEDC_AUTO_CLK;
      if (ledc_timer_config(&timer_conf) != ESP_OK)
      {
        this->buzzer_ready_ = false;
        return;
      }

      ledc_channel_config_t channel_conf{};
      channel_conf.gpio_num = kBuzzerPin;
      channel_conf.speed_mode = kBuzzerMode;
      channel_conf.channel = kBuzzerChannel;
      channel_conf.intr_type = LEDC_INTR_DISABLE;
      channel_conf.timer_sel = kBuzzerTimer;
      channel_conf.duty = 0;
      channel_conf.hpoint = 0;
      if (ledc_channel_config(&channel_conf) != ESP_OK)
      {
        this->buzzer_ready_ = false;
        return;
      }

      this->buzzer_ready_ = true;
    }

    void M5DialThermostat::start_buzzer_tone_(uint32_t frequency_hz)
    {
      if (!this->buzzer_ready_)
      {
        return;
      }
      ledc_set_freq(kBuzzerMode, kBuzzerTimer, frequency_hz);
      ledc_set_duty(kBuzzerMode, kBuzzerChannel, kBuzzerDuty);
      ledc_update_duty(kBuzzerMode, kBuzzerChannel);
    }

    void M5DialThermostat::stop_buzzer_tone_()
    {
      if (!this->buzzer_ready_)
      {
        return;
      }
      ledc_set_duty(kBuzzerMode, kBuzzerChannel, 0);
      ledc_update_duty(kBuzzerMode, kBuzzerChannel);
    }

    void M5DialThermostat::play_sound_(const char *tone)
    {
      if (!this->enable_sounds_ || tone == nullptr || !this->buzzer_ready_)
      {
        return;
      }

      SoundEvent sound_event = SoundEvent::kClick;
      if (std::strcmp(tone, kNoteRotateUp) == 0)
      {
        sound_event = SoundEvent::kRotateUp;
      }
      else if (std::strcmp(tone, kNoteRotateDown) == 0)
      {
        sound_event = SoundEvent::kRotateDown;
      }

      const ToneSpec tone_spec = get_tone_spec(sound_event);
      if (should_retrigger_buzzer(sound_event))
      {
        // Force an off->on edge so rapid encoder ticks sound discrete.
        this->stop_buzzer_tone_();
      }
      this->start_buzzer_tone_(tone_spec.frequency_hz);
      // Intentionally block for these very short tones (<=20 ms). A scheduler-
      // based stop can add jitter on fast knob twists, which sounds less natural.
      delay(tone_spec.duration_ms);
      this->stop_buzzer_tone_();
    }

    bool M5DialThermostat::parse_float_(StringRef value, float *out) const
    {
      if (out == nullptr)
      {
        return false;
      }

      const char *text = value.c_str();
      if (text == nullptr)
      {
        return false;
      }

      char *end_ptr = nullptr;
      const float parsed = std::strtof(text, &end_ptr);
      if (end_ptr == text)
      {
        return false;
      }

      *out = parsed;
      return true;
    }

    void M5DialThermostat::parse_supported_modes_(const char *value)
    {
      this->supported_modes_count_ = 0;
      if (value == nullptr)
      {
        return;
      }

      const size_t length = std::strlen(value);
      size_t i = 0;
      while (i < length && this->supported_modes_count_ < kMaxSupportedModes)
      {
        while (i < length && is_mode_separator(value[i]))
        {
          ++i;
        }
        if (i >= length)
        {
          break;
        }

        const size_t start = i;
        while (i < length && !is_mode_separator(value[i]))
        {
          ++i;
        }
        const size_t token_length = i - start;
        if (token_length == 0)
        {
          continue;
        }

        const HvacMode mode = parse_hvac_mode_span(value + start, token_length);
        if (mode == HvacMode::kUnknown)
        {
          continue;
        }

        const bool exists = std::find(this->supported_modes_.begin(),
                                      this->supported_modes_.begin() +
                                          this->supported_modes_count_,
                                      mode) !=
                            this->supported_modes_.begin() +
                                this->supported_modes_count_;
        if (!exists)
        {
          this->supported_modes_[this->supported_modes_count_++] = mode;
        }
      }
    }

    void M5DialThermostat::on_hvac_mode(StringRef state)
    {
      this->last_ha_update_ = millis();
      this->comms_ok_ = true;

      const HvacMode mode = this->parse_hvac_mode(state.c_str());
      if (this->hvac_mode_ != mode)
      {
        this->hvac_mode_ = mode;
        this->needs_redraw_ = true;
      }
    }

    void M5DialThermostat::on_hvac_action(StringRef state)
    {
      this->last_ha_update_ = millis();
      this->comms_ok_ = true;

      const HvacAction action = this->parse_hvac_action(state.c_str());
      if (this->hvac_action_ != action)
      {
        this->hvac_action_ = action;
        this->needs_redraw_ = true;
      }
    }

    void M5DialThermostat::on_current_temp(StringRef state)
    {
      this->last_ha_update_ = millis();
      this->comms_ok_ = true;

      float value = NAN;
      if (!this->parse_float_(state, &value))
      {
        this->current_temp_ = NAN;
      }
      else
      {
        this->current_temp_ = value;
      }
      this->needs_redraw_ = true;
    }

    void M5DialThermostat::on_target_temp(StringRef state)
    {
      this->last_ha_update_ = millis();
      this->comms_ok_ = true;

      float value = NAN;
      if (!this->parse_float_(state, &value))
      {
        this->target_temp_ = NAN;
        if (!this->local_setpoint_dirty_)
        {
          this->local_setpoint_ = NAN;
        }
        this->needs_redraw_ = true;
        return;
      }

      this->target_temp_ = value;
      if (!this->local_setpoint_dirty_)
      {
        this->local_setpoint_ = this->target_temp_;
      }
      this->needs_redraw_ = true;
    }

    void M5DialThermostat::on_supported_modes(StringRef state)
    {
      this->last_ha_update_ = millis();
      this->comms_ok_ = true;
      this->parse_supported_modes_(state.c_str());
      this->needs_redraw_ = true;
    }

    void M5DialThermostat::on_min_temp(StringRef state)
    {
      this->last_ha_update_ = millis();
      this->comms_ok_ = true;

      float value = NAN;
      if (!this->parse_float_(state, &value))
      {
        return;
      }

      this->min_temp_ = value;
    }

    void M5DialThermostat::on_max_temp(StringRef state)
    {
      this->last_ha_update_ = millis();
      this->comms_ok_ = true;

      float value = NAN;
      if (!this->parse_float_(state, &value))
      {
        return;
      }

      this->max_temp_ = value;
    }

    void M5DialThermostat::on_temp_step(StringRef state)
    {
      this->last_ha_update_ = millis();
      this->comms_ok_ = true;

      float value = NAN;
      if (!this->parse_float_(state, &value))
      {
        return;
      }

      if (value > 0.0f)
      {
        this->temp_step_ = value;
      }
    }

    void M5DialThermostat::send_setpoint_to_ha_()
    {
#ifndef DEBUG_TEST
      if (!this->local_setpoint_dirty_)
      {
        return;
      }
      if (std::isnan(this->local_setpoint_))
      {
        return;
      }
      if (!this->comms_ok_)
      {
        return;
      }

      std::map<std::string, std::string> data;
      data["entity_id"] = this->entity_id_;
      data["temperature"] = std::to_string(this->local_setpoint_);
      this->call_homeassistant_service("climate.set_temperature", data);
      this->local_setpoint_dirty_ = false;
#else
      this->local_setpoint_dirty_ = false;
#endif
    }

    void M5DialThermostat::send_mode_to_ha_(HvacMode mode)
    {
#ifndef DEBUG_TEST
      if (mode == HvacMode::kUnknown)
      {
        return;
      }
      if (!this->comms_ok_)
      {
        return;
      }

      std::map<std::string, std::string> data;
      data["entity_id"] = this->entity_id_;

      switch (mode)
      {
      case HvacMode::kOff:
        data["hvac_mode"] = "off";
        break;
      case HvacMode::kHeat:
        data["hvac_mode"] = "heat";
        break;
      case HvacMode::kCool:
        data["hvac_mode"] = "cool";
        break;
      case HvacMode::kHeatCool:
        data["hvac_mode"] = "heat_cool";
        break;
      case HvacMode::kFanOnly:
        data["hvac_mode"] = "fan_only";
        break;
      case HvacMode::kAuto:
        data["hvac_mode"] = "auto";
        break;
      case HvacMode::kDry:
        data["hvac_mode"] = "dry";
        break;
      case HvacMode::kUnknown:
        return;
      }

      this->call_homeassistant_service("climate.set_hvac_mode", data);
#else
      (void)mode;
#endif
    }

    void M5DialThermostat::on_encoder_tick_(int direction)
    {
#ifndef DEBUG_TEST
      if (!this->comms_ok_)
      {
        return;
      }
#endif
      const SetpointAdjustResult result =
          adjust_setpoint(this->local_setpoint_, this->target_temp_,
                          this->min_temp_, this->max_temp_, this->temp_step_,
                          direction);
      if (!result.changed)
      {
        return;
      }

      this->local_setpoint_ = result.new_setpoint_c;
      this->local_setpoint_dirty_ = true;
      this->last_interaction_ = millis();
      this->set_display_brightness_(true);
      this->needs_redraw_ = true;

      if (direction > 0)
      {
        this->play_sound_(kNoteRotateUp);
      }
      else
      {
        this->play_sound_(kNoteRotateDown);
      }

      this->set_timeout("send_setpoint", kSetpointDebounceMs, [this]()
                        { this->send_setpoint_to_ha_(); });
    }

    void M5DialThermostat::encoder_isr_handler_(void *arg)
    {
      // Decode one quadrature transition and enqueue raw delta for loop().
      M5DialThermostat *const thermostat =
          static_cast<M5DialThermostat *>(arg);
      const int next =
          ((gpio_get_level(static_cast<gpio_num_t>(kEncoderPinA)) << 1) |
           gpio_get_level(static_cast<gpio_num_t>(kEncoderPinB))) &
          0x03;
      const uint8_t prev = thermostat->encoder_isr_state_;
      if (next == static_cast<int>(prev))
      {
        return;
      }

      const int8_t delta = kEncoderTable[prev][next];
      thermostat->encoder_isr_state_ = static_cast<uint8_t>(next);

      if (delta == 0)
      {
        return;
      }

      __atomic_fetch_add(&thermostat->encoder_delta_counts_, delta,
                         __ATOMIC_RELAXED);
    }

    void M5DialThermostat::process_encoder_counts_()
    {
      // Drain raw ISR counts and convert them to detent-level ticks.
      const int32_t delta_counts =
          __atomic_exchange_n(&this->encoder_delta_counts_, 0,
                              __ATOMIC_ACQ_REL);
      if (delta_counts == 0)
      {
        return;
      }

      const EncoderTickResult result = consume_encoder_counts(
          this->encoder_accumulator_, delta_counts, kEncoderCountsPerTick);
      this->encoder_accumulator_ = result.accumulator;
      for (int32_t i = 0; i < result.clockwise_ticks; ++i)
      {
        this->on_encoder_tick_(+1);
      }
      for (int32_t i = 0; i < result.counterclockwise_ticks; ++i)
      {
        this->on_encoder_tick_(-1);
      }
    }

    void M5DialThermostat::on_mode_button_()
    {
#ifndef DEBUG_TEST
      if (!this->comms_ok_)
      {
        return;
      }
#endif
      if (this->supported_modes_count_ <= 1)
      {
        return;
      }

      const int idx = this->find_mode_index_(this->hvac_mode_);
      if (idx < 0)
      {
        return;
      }

      const int next_idx = next_wrapped_index(idx, this->supported_modes_count_);
      if (next_idx < 0)
      {
        return;
      }
      const HvacMode mode = this->supported_modes_[next_idx];
      this->hvac_mode_ = mode;
      // Reset stale action so label shows mode name until HA responds.
      this->hvac_action_ = HvacAction::kUnknown;
      this->send_mode_to_ha_(mode);

      this->last_interaction_ = millis();
      this->set_display_brightness_(true);
      this->needs_redraw_ = true;
      this->play_sound_(kNoteClick);
    }

    void M5DialThermostat::on_button_tick_(int button_state)
    {
      const uint32_t now = millis();
      if (button_state == 0 && this->prev_button_state_ == 1 &&
          now - this->last_button_ms_ >= kButtonDebounceMs)
      {
        this->on_mode_button_();
        this->last_button_ms_ = now;
      }
      this->prev_button_state_ = button_state;
    }

    int M5DialThermostat::find_mode_index_(HvacMode mode) const
    {
      for (int i = 0; i < this->supported_modes_count_; ++i)
      {
        if (this->supported_modes_[i] == mode)
        {
          return i;
        }
      }
      return -1;
    }

    void M5DialThermostat::subscribe_ha_state_()
    {
#ifndef DEBUG_TEST
      this->subscribe_homeassistant_state(&M5DialThermostat::on_hvac_mode,
                                          this->entity_id_, "");
      this->subscribe_homeassistant_state(
          &M5DialThermostat::on_current_temp, this->entity_id_,
          "current_temperature");
      this->subscribe_homeassistant_state(&M5DialThermostat::on_target_temp,
                                          this->entity_id_, "temperature");
      this->subscribe_homeassistant_state(
          &M5DialThermostat::on_hvac_action, this->entity_id_, "hvac_action");
      this->subscribe_homeassistant_state(
          &M5DialThermostat::on_supported_modes, this->entity_id_,
          "hvac_modes");
      this->subscribe_homeassistant_state(&M5DialThermostat::on_min_temp,
                                          this->entity_id_, "min_temp");
      this->subscribe_homeassistant_state(&M5DialThermostat::on_max_temp,
                                          this->entity_id_, "max_temp");
      this->subscribe_homeassistant_state(
          &M5DialThermostat::on_temp_step, this->entity_id_, "target_temp_step");
#endif
    }

    void M5DialThermostat::render_(display::Display &it)
    {
      if (this->display_ == nullptr)
      {
        ESP_LOGW(TAG, "Render skipped: display is null");
        return;
      }

      ESP_LOGD(TAG, "Render: comms_ok=%s current=%.2f setpoint=%.2f mode=%d action=%d",
               this->comms_ok_ ? "true" : "false", this->current_temp_,
               this->local_setpoint_, static_cast<int>(this->hvac_mode_),
               static_cast<int>(this->hvac_action_));

      ThermostatState state{};
      state.current_temp = this->current_temp_;
      state.local_setpoint = this->local_setpoint_;
      state.min_temp = this->min_temp_;
      state.max_temp = this->max_temp_;
      state.hvac_mode = this->hvac_mode_;
      state.hvac_action = this->hvac_action_;
      state.display_fahrenheit = this->display_fahrenheit_;
      state.comms_ok = this->comms_ok_;
      state.reconnect_spinner_start_deg = this->reconnect_spinner_start_deg_;

      ThermostatFonts fonts{
          .mode = this->font_mode_,
          .setpoint = this->font_setpoint_,
          .temp = this->font_temp_,
          .error = this->font_error_,
      };

      if (!this->comms_ok_)
      {
        render_no_connection(it, state, fonts);
        return;
      }

      render_thermostat(it, state, fonts);
    }

    void M5DialThermostat::set_writer_()
    {
      if (this->display_ == nullptr)
      {
        return;
      }

      this->display_->set_writer([this](display::Display &it)
                                 { this->render_(it); });
    }

    void M5DialThermostat::setup()
    {
      if (this->display_ == nullptr || this->entity_id_.empty())
      {
        ESP_LOGE(TAG, "Missing display or entity_id");
        return;
      }

      this->setup_input_pins_();
      this->setup_backlight_();
      this->setup_buzzer_();
      this->set_writer_();
      this->set_display_brightness_(true);
      this->needs_redraw_ = true;
      this->last_interaction_ = millis();
      this->last_ha_update_ = this->last_interaction_;
      this->last_redraw_ms_ = 0;
      this->last_no_connection_anim_ms_ = 0;
      this->reconnect_spinner_start_deg_ = 0.0f;
      ESP_LOGI(TAG, "Setup complete: display=%p buzzer_ready=%s",
               this->display_, this->buzzer_ready_ ? "yes" : "no");

#ifndef DEBUG_TEST
      this->subscribe_ha_state_();
#else
      this->comms_ok_ = true;
      this->current_temp_ = 25.0f;
      this->target_temp_ = 22.0f;
      this->local_setpoint_ = 22.0f;
      this->supported_modes_[0] = HvacMode::kHeat;
      this->supported_modes_[1] = HvacMode::kCool;
      this->supported_modes_[2] = HvacMode::kHeatCool;
      this->supported_modes_[3] = HvacMode::kAuto;
      this->supported_modes_count_ = 4;
      this->hvac_mode_ = HvacMode::kHeat;
      this->hvac_action_ = HvacAction::kHeating;
      this->min_temp_ = 15.0f;
      this->max_temp_ = 30.0f;
      this->temp_step_ = 0.5f;
      this->needs_redraw_ = true;
#endif
    }

    void M5DialThermostat::loop()
    {
      const uint32_t now_ms = millis();
      this->update_comms_timeout_state_(now_ms);

#ifndef DEBUG_TEST
      const bool allow_user_input = this->comms_ok_;
#else
      const bool allow_user_input = true;
#endif

      this->process_user_input_(allow_user_input);
      this->apply_backlight_policy_(now_ms);
      this->update_no_connection_animation_(now_ms);
      this->try_redraw_(now_ms);
    }

    bool M5DialThermostat::update_comms_timeout_state_(uint32_t now_ms)
    {
#ifdef DEBUG_TEST
      (void)now_ms;
      return false;
#else
      if (!should_mark_comms_offline(this->comms_ok_, now_ms, this->last_ha_update_,
                                     this->comms_timeout_ms_))
      {
        return false;
      }
      this->comms_ok_ = false;
      this->needs_redraw_ = true;
      return true;
#endif
    }

    void M5DialThermostat::process_user_input_(bool allow_user_input)
    {
      if (!allow_user_input)
      {
        return;
      }
      this->process_encoder_counts_();
      const int button_state =
          gpio_get_level(static_cast<gpio_num_t>(kButtonPin));
      this->on_button_tick_(button_state);
    }

    void M5DialThermostat::apply_backlight_policy_(uint32_t now_ms)
    {
      if (!this->comms_ok_)
      {
        this->set_display_brightness_(true);
      }
      else if (should_idle_dim(now_ms, this->last_interaction_,
                               this->idle_timeout_ms_))
      {
        this->set_display_brightness_(false);
      }
    }

    void M5DialThermostat::update_no_connection_animation_(uint32_t now_ms)
    {
      if (!should_tick_no_connection_animation(
              this->comms_ok_, now_ms, this->last_no_connection_anim_ms_,
              kNoConnectionAnimIntervalMs))
      {
        return;
      }
      this->last_no_connection_anim_ms_ = now_ms;
      const uint32_t frame = now_ms / kNoConnectionAnimIntervalMs;
      const uint32_t angle = (frame * kNoConnectionSpinnerStepDeg) % 360U;
      this->reconnect_spinner_start_deg_ = static_cast<float>(angle);
      this->needs_redraw_ = true;
    }

    bool M5DialThermostat::try_redraw_(uint32_t now_ms)
    {
      if (!should_trigger_redraw(this->needs_redraw_, this->display_ != nullptr,
                                 this->last_redraw_ms_, now_ms,
                                 kRedrawIntervalMs))
      {
        return false;
      }
      ESP_LOGD(TAG, "Display update triggered");
      this->display_->update();
      this->last_redraw_ms_ = now_ms;
      this->needs_redraw_ = false;
      return true;
    }

    void M5DialThermostat::dump_config()
    {
      ESP_LOGCONFIG(TAG, "M5Dial thermostat");
      ESP_LOGCONFIG(TAG, "  Entity ID: %s", this->entity_id_.c_str());
      ESP_LOGCONFIG(TAG, "  Display: %p", this->display_);
      ESP_LOGCONFIG(TAG, "  Active brightness: %u", this->active_brightness_);
      ESP_LOGCONFIG(TAG, "  Idle brightness: %u", this->idle_brightness_);
      ESP_LOGCONFIG(TAG, "  Idle timeout: %u ms", this->idle_timeout_ms_);
      ESP_LOGCONFIG(TAG, "  Comms timeout: %u ms", this->comms_timeout_ms_);
      ESP_LOGCONFIG(TAG, "  Sounds: %s", this->enable_sounds_ ? "yes" : "no");
    }

  } // namespace m5dial_thermostat
} // namespace esphome
