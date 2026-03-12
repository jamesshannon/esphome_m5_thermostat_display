#include "components/m5dial_thermostat/thermostat_ui.h"

#include <cassert>
#include <cmath>

using namespace esphome::m5dial_thermostat;

static void test_temp_to_angle() {
  // Endpoints: min→130°, max→410°
  assert(std::fabs(temp_to_angle(15.0f, 15.0f, 30.0f) - 130.0f) < 1e-4f);
  assert(std::fabs(temp_to_angle(30.0f, 15.0f, 30.0f) - 410.0f) < 1e-4f);
  // Midpoint (22.5°C) should still map to exactly 270°: 130 + 0.5*280 = 270
  assert(std::fabs(temp_to_angle(22.5f, 15.0f, 30.0f) - 270.0f) < 1e-3f);
  // Clamping
  assert(std::fabs(temp_to_angle(1000.0f, 15.0f, 30.0f) - 410.0f) < 1e-4f);
  assert(std::fabs(temp_to_angle(-1000.0f, 15.0f, 30.0f) - 130.0f) < 1e-4f);
}

static void test_angle_to_xy() {
  int x = 0;
  int y = 0;
  // These test arbitrary angles, not arc bounds -- function is unchanged.
  angle_to_xy(120, 120, 10.0f, 120.0f, &x, &y);
  assert(x == 115 && y == 129);

  angle_to_xy(120, 120, 10.0f, 270.0f, &x, &y);
  assert(x == 120 && y == 110);

  angle_to_xy(120, 120, 10.0f, 420.0f, &x, &y);
  assert(x == 125 && y == 129);
}

static void test_heat_segments() {
  std::array<ArcSegment, 2> out{};

  // current=20°C, setpoint=25°C → heating demanded (setpoint > current)
  // cur_angle  = 130 + (5/15)*280 ≈ 223.33
  // sp_angle   = 130 + (10/15)*280 ≈ 316.67
  int count = compute_heat_segments(20.0f, 25.0f, 15.0f, 30.0f, out.data());
  assert(count == 2);
  assert(std::fabs(out[0].start_angle - 130.0f) < 1e-3f);
  assert(std::fabs(out[0].end_angle - 223.33f) < 0.1f);
  assert(out[0].color == kColorHeatLight);
  assert(std::fabs(out[1].start_angle - 223.33f) < 0.1f);
  assert(std::fabs(out[1].end_angle - 316.67f) < 0.1f);
  assert(out[1].color == kColorHeatDark);

  // current=25°C, setpoint=20°C → no heating demanded (setpoint < current)
  count = compute_heat_segments(25.0f, 20.0f, 15.0f, 30.0f, out.data());
  assert(count == 1);
  assert(std::fabs(out[0].start_angle - 130.0f) < 1e-3f);
  assert(std::fabs(out[0].end_angle - 316.67f) < 0.1f);
  assert(out[0].color == kColorHeatLight);

  // current=min_temp → zero-width current segment, no current segment emitted
  count = compute_heat_segments(15.0f, 20.0f, 15.0f, 30.0f, out.data());
  assert(count == 1);  // only active segment
  assert(out[0].color == kColorHeatDark);
}

static void test_cool_segments() {
  std::array<ArcSegment, 2> out{};

  // current=20°C, setpoint=15°C → cooling demanded (setpoint < current)
  // cur_angle = 130 + (5/15)*280 ≈ 223.33
  // sp_angle  = 130 + (0/15)*280 = 130
  int count = compute_cool_segments(20.0f, 15.0f, 15.0f, 30.0f, out.data());
  assert(count == 2);
  assert(std::fabs(out[0].start_angle - 223.33f) < 0.1f);
  assert(std::fabs(out[0].end_angle - 410.0f) < 1e-3f);
  assert(out[0].color == kColorCoolLight);
  assert(std::fabs(out[1].start_angle - 130.0f) < 1e-3f);
  assert(std::fabs(out[1].end_angle - 223.33f) < 0.1f);
  assert(out[1].color == kColorCoolDark);

  // current=20°C, setpoint=25°C → no cooling demanded (setpoint > current)
  count = compute_cool_segments(20.0f, 25.0f, 15.0f, 30.0f, out.data());
  assert(count == 1);
  assert(std::fabs(out[0].start_angle - 223.33f) < 0.1f);
  assert(std::fabs(out[0].end_angle - 410.0f) < 1e-3f);
  assert(out[0].color == kColorCoolLight);

  // current=max_temp → zero-width current segment, no current segment emitted
  count = compute_cool_segments(30.0f, 20.0f, 15.0f, 30.0f, out.data());
  assert(count == 1);  // only active segment
  assert(out[0].color == kColorCoolDark);
}

int main() {
  test_temp_to_angle();
  test_angle_to_xy();
  test_heat_segments();
  test_cool_segments();
  return 0;
}
