#include "components/m5dial_thermostat/thermostat_ui.h"

#include <cassert>

using namespace esphome;
using namespace esphome::m5dial_thermostat;

namespace {
constexpr int kMaxThermostatDrawPixelCalls = 250000;
constexpr int kMaxThermostatFilledCircleCalls = 20;
constexpr int kMaxNoConnectionDrawPixelCalls = 120000;
constexpr int kMaxNoConnectionFilledCircleCalls = 12;
}  // namespace

static ThermostatFonts make_fonts() {
  static display::BaseFont mode_font;
  static display::BaseFont setpoint_font;
  static display::BaseFont temp_font;
  static display::BaseFont error_font;
  return ThermostatFonts{
      .mode = &mode_font,
      .setpoint = &setpoint_font,
      .temp = &temp_font,
      .error = &error_font,
  };
}

static void test_render_thermostat_draw_budget() {
  display::Display d;
  ThermostatState state{
      .current_temp = 25.0f,
      .local_setpoint = 27.0f,
      .min_temp = 15.0f,
      .max_temp = 30.0f,
      .hvac_mode = HvacMode::kHeat,
      .hvac_action = HvacAction::kHeating,
      .display_fahrenheit = false,
      .comms_ok = true,
      .reconnect_spinner_start_deg = 0.0f,
  };
  const ThermostatFonts fonts = make_fonts();

  d.reset_stats();
  render_thermostat(d, state, fonts);
  assert(d.draw_pixel_calls() > 0);
  assert(d.draw_pixel_calls() < kMaxThermostatDrawPixelCalls);
  assert(d.filled_circle_calls() < kMaxThermostatFilledCircleCalls);
}

static void test_render_no_connection_draw_budget() {
  display::Display d;
  ThermostatState state{
      .current_temp = NAN,
      .local_setpoint = NAN,
      .min_temp = 15.0f,
      .max_temp = 30.0f,
      .hvac_mode = HvacMode::kUnknown,
      .hvac_action = HvacAction::kUnknown,
      .display_fahrenheit = false,
      .comms_ok = false,
      .reconnect_spinner_start_deg = 180.0f,
  };
  const ThermostatFonts fonts = make_fonts();

  d.reset_stats();
  render_no_connection(d, state, fonts);
  assert(d.draw_pixel_calls() > 0);
  assert(d.draw_pixel_calls() < kMaxNoConnectionDrawPixelCalls);
  assert(d.filled_circle_calls() < kMaxNoConnectionFilledCircleCalls);
}

int main() {
  test_render_thermostat_draw_budget();
  test_render_no_connection_draw_budget();
  return 0;
}
