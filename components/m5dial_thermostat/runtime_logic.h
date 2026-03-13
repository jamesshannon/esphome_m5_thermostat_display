#pragma once

#include <cstdint>

namespace esphome
{
  namespace m5dial_thermostat
  {

    enum class SoundEvent : uint8_t
    {
      kRotateUp,
      kRotateDown,
      kClick,
    };

    struct ToneSpec
    {
      uint32_t frequency_hz;
      uint32_t duration_ms;
    };

    struct EncoderTickResult
    {
      int32_t accumulator;
      int32_t clockwise_ticks;
      int32_t counterclockwise_ticks;
    };

    // Returns frequency/duration for a logical sound event.
    ToneSpec get_tone_spec(SoundEvent event);

    // Returns true when event audio should force an off->on edge retrigger.
    bool should_retrigger_buzzer(SoundEvent event);

    // Converts raw quadrature counts into detent-level ticks.
    EncoderTickResult consume_encoder_counts(int32_t accumulator,
                                             int32_t delta_counts,
                                             int8_t counts_per_tick);

    // Returns true when interaction idle timeout has elapsed.
    bool should_idle_dim(uint32_t now_ms, uint32_t last_interaction_ms,
                         uint32_t idle_timeout_ms);

    // Converts logical brightness [0..255] to hardware level considering polarity.
    uint8_t map_backlight_level(uint8_t level, bool active_low);

    // Converts [0..255] level to 10-bit LEDC duty [0..1023].
    uint32_t level_to_ledc_duty_10bit(uint8_t level);

  } // namespace m5dial_thermostat
} // namespace esphome
