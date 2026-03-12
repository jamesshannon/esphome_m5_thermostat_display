#pragma once

#ifdef ESPHOME_STUBS
#include "tests/esphome_stubs.h"
#else
#include "esphome/components/display/display.h"
#endif

#include <array>
#include <cmath>

namespace esphome {
namespace m5dial_thermostat {

enum class HvacMode : uint8_t {
  kOff,
  kHeat,
  kCool,
  kHeatCool,
  kFanOnly,
  kAuto,
  kDry,
  kUnknown,
};

enum class HvacAction : uint8_t {
  kOff,
  kHeating,
  kCooling,
  kIdle,
  kFan,
  kDrying,
  kUnknown,
};

struct ArcSegment {
  float start_angle;
  float end_angle;
  Color color;
};

struct ThermostatState {
  float current_temp;
  float local_setpoint;
  float min_temp;
  float max_temp;
  HvacMode hvac_mode;
  HvacAction hvac_action;
  bool display_fahrenheit;
  bool comms_ok;
};

struct ThermostatFonts {
  display::BaseFont *mode{nullptr};
  display::BaseFont *setpoint{nullptr};
  display::BaseFont *temp{nullptr};
  display::BaseFont *error{nullptr};
};

constexpr float kArcStartAngleDeg = 120.0f;
constexpr float kArcSpanDeg = 300.0f;
constexpr float kArcEndAngleDeg = kArcStartAngleDeg + kArcSpanDeg;

constexpr int kDefaultCenterX = 120;
constexpr int kDefaultCenterY = 120;
constexpr int kDefaultOuterRadius = 108;
constexpr int kDefaultInnerRadius = 90;

constexpr Color kColorTrack = Color(0xCC, 0xCC, 0xCC);
constexpr Color kColorHeatLight = Color(0xFF, 0xCB, 0xA4);
constexpr Color kColorHeatDark = Color(0xE8, 0x85, 0x5A);
constexpr Color kColorCoolLight = Color(0xAD, 0xD8, 0xF0);
constexpr Color kColorCoolDark = Color(0x1E, 0x90, 0xFF);
constexpr Color kColorFan = Color(0xC8, 0xA8, 0xE9);
constexpr Color kColorText = Color(0xFF, 0xFF, 0xFF);
constexpr Color kColorTextMuted = Color(0xDD, 0xDD, 0xDD);

float temp_to_angle(float temp, float min_temp, float max_temp);

void angle_to_xy(int cx, int cy, float radius, float angle_deg, int *x, int *y);

int compute_heat_segments(
    float current,
    float setpoint,
    float min_t,
    float max_t,
    ArcSegment out[3]);

int compute_cool_segments(
    float current,
    float setpoint,
    float min_t,
    float max_t,
    ArcSegment out[3]);

float celsius_to_fahrenheit(float value);

float clamp_setpoint(float value, float min_temp, float max_temp);

void draw_arc_segment(
    display::Display &d,
    int cx,
    int cy,
    int r_inner,
    int r_outer,
    float angle_start,
    float angle_end,
    Color color);

void render_thermostat(
    display::Display &d,
    const ThermostatState &state,
    const ThermostatFonts &fonts);

void render_no_connection(display::Display &d, const ThermostatFonts &fonts);

}  // namespace m5dial_thermostat
}  // namespace esphome
