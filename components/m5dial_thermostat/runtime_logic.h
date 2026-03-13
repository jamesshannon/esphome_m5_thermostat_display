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

    struct SetpointAdjustResult
    {
      bool changed;
      float new_setpoint_c;
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

    // Returns true when comms should be marked offline due to timeout.
    bool should_mark_comms_offline(bool comms_ok, uint32_t now_ms,
                                   uint32_t last_ha_update_ms,
                                   uint32_t comms_timeout_ms);

    // Returns true when a pending frame should be rendered this loop.
    bool should_trigger_redraw(bool needs_redraw, bool has_display,
                               uint32_t last_redraw_ms, uint32_t now_ms,
                               uint16_t redraw_interval_ms);

    // Returns true when no-connection animation should advance a frame.
    bool should_tick_no_connection_animation(bool comms_ok, uint32_t now_ms,
                                             uint32_t last_anim_tick_ms,
                                             uint16_t anim_interval_ms);

    // Computes the next wrapped index. Returns -1 on invalid inputs.
    int next_wrapped_index(int current_index, int count);

    // Applies a setpoint step and clamp. Returns changed=false on no-op.
    SetpointAdjustResult adjust_setpoint(float local_setpoint_c,
                                         float target_temp_c,
                                         float min_temp_c,
                                         float max_temp_c,
                                         float temp_step_c, int direction);

    // Returns true when the displayed 0.1-degree value changed (or NaN state
    // changed), for the selected display units.
    bool has_display_temp_changed(float previous_temp_c, float next_temp_c,
                                  bool display_fahrenheit);

    // Converts logical brightness [0..255] to hardware level considering polarity.
    uint8_t map_backlight_level(uint8_t level, bool active_low);

    // Converts [0..255] level to 10-bit LEDC duty [0..1023].
    uint32_t level_to_ledc_duty_10bit(uint8_t level);

  } // namespace m5dial_thermostat
} // namespace esphome
