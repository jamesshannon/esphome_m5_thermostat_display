#define ESPHOME_STUBS
#include "components/m5dial_thermostat/thermostat_ui.h"

#include <cassert>
#include <cmath>

static void test_temp_to_angle() {
  assert(std::fabs(temp_to_angle(15.0f, 15.0f, 30.0f) - 120.0f) < 1e-4f);
  assert(std::fabs(temp_to_angle(30.0f, 15.0f, 30.0f) - 420.0f) < 1e-4f);
  assert(std::fabs(temp_to_angle(22.5f, 15.0f, 30.0f) - 270.0f) < 1e-3f);
  assert(std::fabs(temp_to_angle(1000.0f, 15.0f, 30.0f) - 420.0f) < 1e-4f);
  assert(std::fabs(temp_to_angle(-1000.0f, 15.0f, 30.0f) - 120.0f) < 1e-4f);
}

static void test_angle_to_xy() {
  int x = 0;
  int y = 0;
  angle_to_xy(120, 120, 10.0f, 120.0f, &x, &y);
  assert(x == 115 && y == 129);

  angle_to_xy(120, 120, 10.0f, 270.0f, &x, &y);
  assert(x == 120 && y == 110);

  angle_to_xy(120, 120, 10.0f, 420.0f, &x, &y);
  assert(x == 125 && y == 129);
}

static void test_segments() {
  std::array<ArcSegment, 3> out{};
  int count = compute_heat_segments(20.0f, 25.0f, 15.0f, 30.0f, out.data());
  assert(count == 3);
  assert(std::fabs(out[0].start_angle - 120.0f) < 1e-4f);
  assert(std::fabs(out[0].end_angle - 220.0f) < 1e-4f);
  assert(out[0].color == kColorHeatLight);
  assert(std::fabs(out[1].start_angle - 220.0f) < 1e-4f);
  assert(std::fabs(out[1].end_angle - 320.0f) < 1e-4f);
  assert(out[1].color == kColorHeatDark);
  assert(std::fabs(out[2].start_angle - 320.0f) < 1e-4f);
  assert(std::fabs(out[2].end_angle - 420.0f) < 1e-4f);
  assert(out[2].color == kColorTrack);

  count = compute_cool_segments(20.0f, 25.0f, 15.0f, 30.0f, out.data());
  assert(count == 3);
  assert(std::fabs(out[0].start_angle - 320.0f) < 1e-4f);
  assert(std::fabs(out[0].end_angle - 420.0f) < 1e-4f);
  assert(out[0].color == kColorCoolLight);
  assert(std::fabs(out[1].start_angle - 220.0f) < 1e-4f);
  assert(std::fabs(out[1].end_angle - 320.0f) < 1e-4f);
  assert(out[1].color == kColorCoolDark);
  assert(std::fabs(out[2].start_angle - 120.0f) < 1e-4f);
  assert(std::fabs(out[2].end_angle - 220.0f) < 1e-4f);
  assert(out[2].color == kColorTrack);
}

int main() {
  test_temp_to_angle();
  test_angle_to_xy();
  test_segments();
  return 0;
}
