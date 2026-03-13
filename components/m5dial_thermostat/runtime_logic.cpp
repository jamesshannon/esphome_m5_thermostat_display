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
