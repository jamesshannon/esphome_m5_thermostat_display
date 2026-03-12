#include "components/m5dial_thermostat/thermostat_ui.h"

#include <cassert>
#include <cmath>

using namespace esphome::m5dial_thermostat;

static void test_clamp_and_nan() {
  assert(std::isnan(clamp_setpoint(NAN, 10.0f, 20.0f)));
  assert(clamp_setpoint(5.0f, 10.0f, 20.0f) == 10.0f);
  assert(clamp_setpoint(25.0f, 10.0f, 20.0f) == 20.0f);
  assert(clamp_setpoint(15.0f, 10.0f, 20.0f) == 15.0f);
}

static void test_segments_nan() {
  std::array<ArcSegment, 3> out{};
  assert(compute_heat_segments(NAN, 22.0f, 15.0f, 30.0f, out.data()) == 0);
  assert(compute_heat_segments(20.0f, NAN, 15.0f, 30.0f, out.data()) == 0);
  assert(compute_cool_segments(NAN, 22.0f, 15.0f, 30.0f, out.data()) == 0);
  assert(compute_cool_segments(20.0f, NAN, 15.0f, 30.0f, out.data()) == 0);
}

static void test_colors() {
  assert(kColorHeatLight != kColorHeatDark);
  assert(kColorCoolLight != kColorCoolDark);
  assert(kColorTrack != kColorFan);
}

int main() {
  test_clamp_and_nan();
  test_segments_nan();
  test_colors();
  return 0;
}
