#include "components/m5dial_thermostat/runtime_logic.h"

#include <cassert>
#include <cmath>

using namespace esphome::m5dial_thermostat;

static void test_consume_encoder_counts() {
  EncoderTickResult result = consume_encoder_counts(0, 5, 2);
  assert(result.clockwise_ticks == 2);
  assert(result.counterclockwise_ticks == 0);
  assert(result.accumulator == 1);

  result = consume_encoder_counts(1, -5, 2);
  assert(result.clockwise_ticks == 0);
  assert(result.counterclockwise_ticks == 2);
  assert(result.accumulator == 0);

  result = consume_encoder_counts(7, 3, 0);
  assert(result.clockwise_ticks == 0);
  assert(result.counterclockwise_ticks == 0);
  assert(result.accumulator == 7);
}

static void test_tone_spec_and_retrigger() {
  const ToneSpec rotate_up = get_tone_spec(SoundEvent::kRotateUp);
  assert(rotate_up.frequency_hz == 6000U);
  assert(rotate_up.duration_ms == 4U);
  assert(should_retrigger_buzzer(SoundEvent::kRotateUp));

  const ToneSpec rotate_down = get_tone_spec(SoundEvent::kRotateDown);
  assert(rotate_down.frequency_hz == 3000U);
  assert(rotate_down.duration_ms == 5U);
  assert(should_retrigger_buzzer(SoundEvent::kRotateDown));

  const ToneSpec click = get_tone_spec(SoundEvent::kClick);
  assert(click.frequency_hz == 2000U);
  assert(click.duration_ms == 20U);
  assert(!should_retrigger_buzzer(SoundEvent::kClick));
}

static void test_should_idle_dim() {
  assert(!should_idle_dim(1000U, 0U, 30000U));
  assert(!should_idle_dim(1000U, 1100U, 30000U));
  assert(!should_idle_dim(31000U, 1000U, 30000U));
  assert(should_idle_dim(31001U, 1000U, 30000U));
}

static void test_should_mark_comms_offline() {
  assert(!should_mark_comms_offline(false, 5000U, 1000U, 30000U));
  assert(!should_mark_comms_offline(true, 5000U, 6000U, 30000U));
  assert(!should_mark_comms_offline(true, 31000U, 1000U, 30000U));
  assert(should_mark_comms_offline(true, 31001U, 1000U, 30000U));
}

static void test_should_trigger_redraw() {
  assert(!should_trigger_redraw(false, true, 1000U, 2000U, 33U));
  assert(!should_trigger_redraw(true, false, 1000U, 2000U, 33U));
  assert(should_trigger_redraw(true, true, 0U, 2000U, 33U));
  assert(!should_trigger_redraw(true, true, 1980U, 2000U, 33U));
  assert(should_trigger_redraw(true, true, 1967U, 2000U, 33U));
}

static void test_should_tick_no_connection_animation() {
  assert(!should_tick_no_connection_animation(true, 1000U, 0U, 100U));
  assert(should_tick_no_connection_animation(false, 1000U, 0U, 100U));
  assert(should_tick_no_connection_animation(false, 1000U, 1100U, 100U));
  assert(!should_tick_no_connection_animation(false, 1099U, 1000U, 100U));
  assert(should_tick_no_connection_animation(false, 1100U, 1000U, 100U));
}

static void test_next_wrapped_index() {
  assert(next_wrapped_index(0, 4) == 1);
  assert(next_wrapped_index(3, 4) == 0);
  assert(next_wrapped_index(-1, 4) == -1);
  assert(next_wrapped_index(4, 4) == -1);
  assert(next_wrapped_index(0, 0) == -1);
}

static void test_adjust_setpoint() {
  SetpointAdjustResult result =
      adjust_setpoint(22.0f, 22.0f, 15.0f, 30.0f, 0.5f, +1);
  assert(result.changed);
  assert(result.new_setpoint_c == 22.5f);

  result = adjust_setpoint(15.0f, 15.0f, 15.0f, 30.0f, 0.5f, -1);
  assert(!result.changed);
  assert(result.new_setpoint_c == 15.0f);

  result = adjust_setpoint(NAN, 21.0f, 15.0f, 30.0f, 0.5f, +1);
  assert(result.changed);
  assert(result.new_setpoint_c == 21.5f);

  result = adjust_setpoint(22.0f, NAN, 15.0f, 30.0f, 0.5f, +1);
  assert(!result.changed);
  assert(result.new_setpoint_c == 22.0f);

  result = adjust_setpoint(22.0f, 22.0f, 15.0f, 30.0f, 0.0f, +1);
  assert(!result.changed);
}

static void test_backlight_mapping() {
  assert(map_backlight_level(0U, false) == 0U);
  assert(map_backlight_level(128U, false) == 128U);
  assert(map_backlight_level(255U, false) == 255U);

  assert(map_backlight_level(0U, true) == 255U);
  assert(map_backlight_level(128U, true) == 127U);
  assert(map_backlight_level(255U, true) == 0U);

  assert(level_to_ledc_duty_10bit(0U) == 0U);
  assert(level_to_ledc_duty_10bit(255U) == 1023U);
}

static void test_has_display_temp_changed() {
  assert(!has_display_temp_changed(21.04f, 21.03f, false));
  assert(has_display_temp_changed(21.04f, 21.15f, false));

  assert(!has_display_temp_changed(21.00f, 21.02f, true));
  assert(has_display_temp_changed(21.00f, 21.06f, true));

  assert(has_display_temp_changed(NAN, 21.0f, false));
  assert(has_display_temp_changed(21.0f, NAN, false));
  assert(!has_display_temp_changed(NAN, NAN, false));
}

static void test_should_send_setpoint() {
  assert(!should_send_setpoint(false, 22.0f, true));
  assert(!should_send_setpoint(true, NAN, true));
  assert(!should_send_setpoint(true, 22.0f, false));
  assert(should_send_setpoint(true, 22.0f, true));
}

int main() {
  test_consume_encoder_counts();
  test_tone_spec_and_retrigger();
  test_should_idle_dim();
  test_should_mark_comms_offline();
  test_should_trigger_redraw();
  test_should_tick_no_connection_animation();
  test_next_wrapped_index();
  test_adjust_setpoint();
  test_backlight_mapping();
  test_has_display_temp_changed();
  test_should_send_setpoint();
  return 0;
}
