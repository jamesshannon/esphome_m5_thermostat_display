#define ESPHOME_STUBS
#include "components/m5dial_thermostat/thermostat_ui.h"

#include <cassert>

int main() {
  assert(celsius_to_fahrenheit(0.0f) == 32.0f);
  assert(celsius_to_fahrenheit(25.0f) == 77.0f);
  assert(celsius_to_fahrenheit(-40.0f) == -40.0f);

  assert(clamp_setpoint(-2.0f, 10.0f, 20.0f) == 10.0f);
  assert(clamp_setpoint(30.0f, 10.0f, 20.0f) == 20.0f);
  assert(clamp_setpoint(15.0f, 10.0f, 20.0f) == 15.0f);

  return 0;
}
