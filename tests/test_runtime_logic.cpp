#include "components/m5dial_thermostat/runtime_logic.h"

#include <cassert>

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
  assert(rotate_up.duration_ms == 8U);
  assert(should_retrigger_buzzer(SoundEvent::kRotateUp));

  const ToneSpec rotate_down = get_tone_spec(SoundEvent::kRotateDown);
  assert(rotate_down.frequency_hz == 7000U);
  assert(rotate_down.duration_ms == 8U);
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

int main() {
  test_consume_encoder_counts();
  test_tone_spec_and_retrigger();
  test_should_idle_dim();
  return 0;
}
