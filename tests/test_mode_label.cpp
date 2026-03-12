#include "components/m5dial_thermostat/thermostat_ui.h"

#include <cassert>
#include <cstring>

using namespace esphome::m5dial_thermostat;

// When action is known, the action label takes priority.
static void test_action_labels() {
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kHeat, HvacAction::kHeating),
      "Heating") == 0);
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kCool, HvacAction::kCooling),
      "Cooling") == 0);
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kFanOnly, HvacAction::kFan),
      "Fan") == 0);
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kHeat, HvacAction::kIdle),
      "Idle") == 0);
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kOff, HvacAction::kOff),
      "Off") == 0);
}

// When action is unknown (e.g. right after a local mode change),
// the label should fall through to the mode's own name.
static void test_mode_fallback_labels() {
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kHeat, HvacAction::kUnknown),
      "Heating") == 0);
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kCool, HvacAction::kUnknown),
      "Cooling") == 0);
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kHeatCool, HvacAction::kUnknown),
      "Heat/Cool") == 0);
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kAuto, HvacAction::kUnknown),
      "Auto") == 0);
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kFanOnly, HvacAction::kUnknown),
      "Fan") == 0);
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kDry, HvacAction::kUnknown),
      "Dry") == 0);
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kOff, HvacAction::kUnknown),
      "Off") == 0);
}

// Regression: after cycling to HeatCool or Auto with a stale
// action, the label must NOT show the previous action's text.
// This simulates on_mode_button_ resetting action to kUnknown.
static void test_mode_cycle_no_stale_action() {
  // Simulate: was in Heat mode with kHeating action, cycled to HeatCool.
  // on_mode_button_ now resets action to kUnknown.
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kHeatCool, HvacAction::kUnknown),
      "Heat/Cool") == 0);
  // Simulate: cycled to Auto with stale action reset.
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kAuto, HvacAction::kUnknown),
      "Auto") == 0);

  // Verify that if action somehow remained kHeating (the old bug),
  // HeatCool/Auto would wrongly show "Heating" -- this documents
  // the exact failure mode the fix prevents.
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kHeatCool, HvacAction::kHeating),
      "Heating") == 0);  // action takes priority -- correct per spec
  assert(std::strcmp(
      get_setpoint_label_for_mode(HvacMode::kAuto, HvacAction::kHeating),
      "Heating") == 0);  // action takes priority -- correct per spec
  // The bug was that on_mode_button_ didn't reset action to kUnknown,
  // so these action-priority results would show instead of mode names.
}

int main() {
  test_action_labels();
  test_mode_fallback_labels();
  test_mode_cycle_no_stale_action();
  return 0;
}
