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

// Arc spans [130, 410] degrees (280° span, gap at bottom).
// 0° = 3 o'clock, clockwise. Y increases downward.
constexpr float kArcStartAngleDeg = 130.0f;
constexpr float kArcSpanDeg = 280.0f;
constexpr float kArcEndAngleDeg = kArcStartAngleDeg + kArcSpanDeg;

constexpr int kDefaultCenterX = 120;
constexpr int kDefaultCenterY = 120;
// Outer radius near screen edge; inner radius gives 28 px width.
constexpr int kDefaultOuterRadius = 118;
constexpr int kDefaultInnerRadius = 90;

// Setpoint dot: white fill (r=10) + 4 px border (total r=14 = arc width).
constexpr int kSetpointDotFillRadius = 10;
constexpr int kSetpointDotBorderRadius = 14;
// Current temp dot: 50% of arc width (14 px diameter = r=7).
constexpr int kCurrentDotRadius = 7;

constexpr Color kColorBackground  = Color(0xFF, 0xFF, 0xFF);
constexpr Color kColorTrack       = Color(0xEC, 0xE7, 0xE4);
constexpr Color kColorHeatLight   = Color(0xFF, 0xB6, 0x91);
constexpr Color kColorHeatDark    = Color(0xFF, 0x70, 0x22);
constexpr Color kColorCoolLight   = Color(0x95, 0xC8, 0xF9);
constexpr Color kColorCoolDark    = Color(0x21, 0x96, 0xF3);
constexpr Color kColorFan         = Color(0xC8, 0xA8, 0xE9);
constexpr Color kColorCurrentDot  = Color(0x7F, 0x7F, 0x7F);
constexpr Color kColorText        = Color(0x00, 0x00, 0x00);
constexpr Color kColorTextMuted   = Color(0x55, 0x55, 0x55);

float temp_to_angle(float temp, float min_temp, float max_temp);

void angle_to_xy(int cx, int cy, float radius, float angle_deg, int *x, int *y);

int compute_heat_segments(
    float current,
    float setpoint,
    float min_t,
    float max_t,
    ArcSegment out[2]);

int compute_cool_segments(
    float current,
    float setpoint,
    float min_t,
    float max_t,
    ArcSegment out[2]);

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
