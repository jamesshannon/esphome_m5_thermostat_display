#include "runtime_logic.h"

#include <cmath>

namespace esphome
{
  namespace m5dial_thermostat
  {

    namespace
    {
      constexpr uint32_t kRotateUpFrequencyHz = 6000;
      constexpr uint32_t kRotateUpToneDurationMs = 4;
      constexpr uint32_t kRotateDownFrequencyHz = 3000;
      constexpr uint32_t kRotateDownToneDurationMs = 5;
      constexpr uint32_t kClickFrequencyHz = 2000;
      constexpr uint32_t kClickToneDurationMs = 20;

      float to_display_temp(float temp_c, bool display_fahrenheit)
      {
        if (!display_fahrenheit)
        {
          return temp_c;
        }
        return (temp_c * 9.0f / 5.0f) + 32.0f;
      }

      int32_t quantize_tenths(float value)
      {
        return static_cast<int32_t>(std::lround(value * 10.0f));
      }
    } // namespace

    ToneSpec get_tone_spec(SoundEvent event)
    {
      switch (event)
      {
      case SoundEvent::kRotateUp:
        return ToneSpec{kRotateUpFrequencyHz, kRotateUpToneDurationMs};
      case SoundEvent::kRotateDown:
        return ToneSpec{kRotateDownFrequencyHz, kRotateDownToneDurationMs};
      case SoundEvent::kClick:
      default:
        return ToneSpec{kClickFrequencyHz, kClickToneDurationMs};
      }
    }

    bool should_retrigger_buzzer(SoundEvent event)
    {
      return event == SoundEvent::kRotateUp ||
             event == SoundEvent::kRotateDown;
    }

    EncoderTickResult consume_encoder_counts(int32_t accumulator,
                                             int32_t delta_counts,
                                             int8_t counts_per_tick)
    {
      EncoderTickResult result{
          .accumulator = accumulator,
          .clockwise_ticks = 0,
          .counterclockwise_ticks = 0,
      };
      if (counts_per_tick <= 0)
      {
        return result;
      }

      result.accumulator += delta_counts;
      while (result.accumulator >= counts_per_tick)
      {
        ++result.clockwise_ticks;
        result.accumulator -= counts_per_tick;
      }
      while (result.accumulator <= -counts_per_tick)
      {
        ++result.counterclockwise_ticks;
        result.accumulator += counts_per_tick;
      }
      return result;
    }

    bool should_idle_dim(uint32_t now_ms, uint32_t last_interaction_ms,
                         uint32_t idle_timeout_ms)
    {
      if (last_interaction_ms == 0 || now_ms < last_interaction_ms)
      {
        return false;
      }
      return now_ms - last_interaction_ms > idle_timeout_ms;
    }

    bool should_mark_comms_offline(bool comms_ok, uint32_t now_ms,
                                   uint32_t last_ha_update_ms,
                                   uint32_t comms_timeout_ms)
    {
      if (!comms_ok || now_ms < last_ha_update_ms)
      {
        return false;
      }
      return now_ms - last_ha_update_ms > comms_timeout_ms;
    }

    bool should_trigger_redraw(bool needs_redraw, bool has_display,
                               uint32_t last_redraw_ms, uint32_t now_ms,
                               uint16_t redraw_interval_ms)
    {
      if (!needs_redraw || !has_display)
      {
        return false;
      }
      if (last_redraw_ms == 0 || now_ms < last_redraw_ms)
      {
        return true;
      }
      return now_ms - last_redraw_ms >= redraw_interval_ms;
    }

    bool should_tick_no_connection_animation(bool comms_ok, uint32_t now_ms,
                                             uint32_t last_anim_tick_ms,
                                             uint16_t anim_interval_ms)
    {
      if (comms_ok)
      {
        return false;
      }
      if (last_anim_tick_ms == 0 || now_ms < last_anim_tick_ms)
      {
        return true;
      }
      return now_ms - last_anim_tick_ms >= anim_interval_ms;
    }

    bool compute_comms_ok_from_api(bool has_received_ha_state, bool api_connected,
                                   uint32_t now_ms, uint32_t last_api_connected_ms,
                                   uint32_t comms_timeout_ms)
    {
      // Stay disconnected until we have confirmed at least one HA state update.
      if (!has_received_ha_state)
      {
        return false;
      }
      if (api_connected)
      {
        return true;
      }
      if (last_api_connected_ms == 0 || now_ms < last_api_connected_ms)
      {
        return false;
      }
      // Grace period prevents short API blips from flickering reconnect UI.
      return now_ms - last_api_connected_ms <= comms_timeout_ms;
    }

    int next_wrapped_index(int current_index, int count)
    {
      if (count <= 0 || current_index < 0 || current_index >= count)
      {
        return -1;
      }
      return (current_index + 1) % count;
    }

    SetpointAdjustResult adjust_setpoint(float local_setpoint_c,
                                         float target_temp_c,
                                         float min_temp_c,
                                         float max_temp_c,
                                         float temp_step_c, int direction)
    {
      if (temp_step_c <= 0.0f || direction == 0 || std::isnan(target_temp_c))
      {
        return SetpointAdjustResult{.changed = false, .new_setpoint_c = local_setpoint_c};
      }

      float seed_setpoint_c = local_setpoint_c;
      if (std::isnan(seed_setpoint_c))
      {
        seed_setpoint_c = target_temp_c;
      }

      const float delta = direction > 0 ? temp_step_c : -temp_step_c;
      float next_setpoint_c = seed_setpoint_c + delta;
      if (next_setpoint_c < min_temp_c)
      {
        next_setpoint_c = min_temp_c;
      }
      if (next_setpoint_c > max_temp_c)
      {
        next_setpoint_c = max_temp_c;
      }

      const bool changed = std::fabs(next_setpoint_c - seed_setpoint_c) > 1e-6f;
      return SetpointAdjustResult{
          .changed = changed,
          .new_setpoint_c = changed ? next_setpoint_c : local_setpoint_c,
      };
    }

    bool is_setpoint_ack_within_tolerance(float requested_setpoint_c,
                                          float echoed_setpoint_c,
                                          float temp_step_c)
    {
      if (std::isnan(requested_setpoint_c) || std::isnan(echoed_setpoint_c))
      {
        return false;
      }
      const float step = temp_step_c > 0.0f ? temp_step_c : 0.5f;
      const float tolerance = step * 0.5f + 1e-3f;
      return std::fabs(requested_setpoint_c - echoed_setpoint_c) <= tolerance;
    }

    bool should_send_setpoint(bool local_setpoint_dirty, float local_setpoint_c,
                              bool comms_ok)
    {
      if (!local_setpoint_dirty || std::isnan(local_setpoint_c))
      {
        return false;
      }
      return comms_ok;
    }

    bool has_display_temp_changed(float previous_temp_c, float next_temp_c,
                                  bool display_fahrenheit)
    {
      const bool previous_nan = std::isnan(previous_temp_c);
      const bool next_nan = std::isnan(next_temp_c);
      if (previous_nan || next_nan)
      {
        return previous_nan != next_nan;
      }

      const float previous_display =
          to_display_temp(previous_temp_c, display_fahrenheit);
      const float next_display = to_display_temp(next_temp_c, display_fahrenheit);
      return quantize_tenths(previous_display) != quantize_tenths(next_display);
    }

    float get_effective_setpoint_step_c(bool display_fahrenheit,
                                        float climate_step_c,
                                        float fahrenheit_step_f)
    {
      constexpr float kDefaultClimateStepC = 0.5f;
      constexpr float kHalfFStep = 0.5f;
      constexpr float kOneFStep = 1.0f;
      constexpr float kFToCScale = 5.0f / 9.0f;

      const float valid_climate_step =
          climate_step_c > 0.0f ? climate_step_c : kDefaultClimateStepC;
      if (!display_fahrenheit)
      {
        return valid_climate_step;
      }

      const bool one_degree_mode = std::fabs(fahrenheit_step_f - kOneFStep) < 1e-3f;
      const float step_f = one_degree_mode ? kOneFStep : kHalfFStep;
      return step_f * kFToCScale;
    }

    bool use_integer_fahrenheit_display(bool display_fahrenheit,
                                        float fahrenheit_step_f)
    {
      if (!display_fahrenheit)
      {
        return false;
      }
      return std::fabs(fahrenheit_step_f - 1.0f) < 1e-3f;
    }

    uint8_t map_backlight_level(uint8_t level, bool active_low)
    {
      if (!active_low)
      {
        return level;
      }
      return static_cast<uint8_t>(255U - level);
    }

    uint32_t level_to_ledc_duty_10bit(uint8_t level)
    {
      return (static_cast<uint32_t>(level) * 1023U) / 255U;
    }

  } // namespace m5dial_thermostat
} // namespace esphome
