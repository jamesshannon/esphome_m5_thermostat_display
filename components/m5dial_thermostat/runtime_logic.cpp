#include "runtime_logic.h"

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
