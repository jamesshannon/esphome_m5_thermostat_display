#include "thermostat_ui.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kTwoPi / 360.0f;
constexpr float kRadToDeg = 180.0f / kPi;
}  // namespace

namespace esphome {
namespace m5dial_thermostat {

// Convert from temperature to the expanded angle range [120, 420],
// which avoids wrapping while still covering the clockwise 300° span.
float temp_to_angle(float temp, float min_temp, float max_temp) {
  if (std::isnan(min_temp) || std::isnan(max_temp) || max_temp <= min_temp) {
    return kArcStartAngleDeg;
  }

  const float clamped = clamp_setpoint(temp, min_temp, max_temp);
  const float span = max_temp - min_temp;
  const float ratio = (clamped - min_temp) / span;
  return kArcStartAngleDeg + (ratio * kArcSpanDeg);
}

void angle_to_xy(int cx, int cy, float radius, float angle_deg, int *x, int *y) {
  const float angle_rad = angle_deg * kDegToRad;
  if (x != nullptr) {
    *x = static_cast<int>(std::lround(static_cast<float>(cx) +
                                     (radius * std::cos(angle_rad))));
  }
  if (y != nullptr) {
    *y = static_cast<int>(std::lround(static_cast<float>(cy) +
                                     (radius * std::sin(angle_rad))));
  }
}

float clamp_setpoint(float value, float min_temp, float max_temp) {
  if (std::isnan(value)) {
    return value;
  }
  return std::max(min_temp, std::min(max_temp, value));
}

float celsius_to_fahrenheit(float value) {
  return (value * 9.0f / 5.0f) + 32.0f;
}

int compute_heat_segments(
    float current,
    float setpoint,
    float min_t,
    float max_t,
    ArcSegment out[3]) {
  if (out == nullptr || max_t <= min_t || std::isnan(current) ||
      std::isnan(setpoint) || std::isnan(min_t) || std::isnan(max_t)) {
    return 0;
  }

  const float clamped_current = clamp_setpoint(current, min_t, max_t);
  const float clamped_setpoint = clamp_setpoint(setpoint, min_t, max_t);

  const float low = std::min(clamped_current, clamped_setpoint);
  const float high = std::max(clamped_current, clamped_setpoint);

  const float start_angle = temp_to_angle(min_t, min_t, max_t);
  const float end_angle = temp_to_angle(max_t, min_t, max_t);
  const float low_angle = temp_to_angle(low, min_t, max_t);
  const float high_angle = temp_to_angle(high, min_t, max_t);

  int count = 0;
  if (start_angle < low_angle - 1e-6f) {
    out[count++] = {start_angle, low_angle, kColorHeatLight};
  }
  if (low_angle < high_angle - 1e-6f) {
    out[count++] = {low_angle, high_angle, kColorHeatDark};
  }
  if (high_angle < end_angle - 1e-6f) {
    out[count++] = {high_angle, end_angle, kColorTrack};
  }

  return count;
}

int compute_cool_segments(
    float current,
    float setpoint,
    float min_t,
    float max_t,
    ArcSegment out[3]) {
  if (out == nullptr || max_t <= min_t || std::isnan(current) ||
      std::isnan(setpoint) || std::isnan(min_t) || std::isnan(max_t)) {
    return 0;
  }

  const float clamped_current = clamp_setpoint(current, min_t, max_t);
  const float clamped_setpoint = clamp_setpoint(setpoint, min_t, max_t);

  const float low = std::min(clamped_current, clamped_setpoint);
  const float high = std::max(clamped_current, clamped_setpoint);

  const float start_angle = temp_to_angle(min_t, min_t, max_t);
  const float end_angle = temp_to_angle(max_t, min_t, max_t);
  const float low_angle = temp_to_angle(low, min_t, max_t);
  const float high_angle = temp_to_angle(high, min_t, max_t);

  int count = 0;
  if (high_angle < end_angle - 1e-6f) {
    out[count++] = {high_angle, end_angle, kColorCoolLight};
  }
  if (low_angle < high_angle - 1e-6f) {
    out[count++] = {low_angle, high_angle, kColorCoolDark};
  }
  if (start_angle < low_angle - 1e-6f) {
    out[count++] = {start_angle, low_angle, kColorTrack};
  }

  return count;
}

// Draw a ring segment for [angle_start, angle_end] in extended [120, 420] space,
// with rounded end caps based on the ring width.
void draw_arc_segment(
    display::Display &d,
    int cx,
    int cy,
    int r_inner,
    int r_outer,
    float angle_start,
    float angle_end,
    Color color) {
  if (angle_end < angle_start) {
    return;
  }

  const float center_radius = static_cast<float>(r_inner + r_outer) * 0.5f;
  const float cap_radius = static_cast<float>(r_outer - r_inner) * 0.5f;
  const float r_inner_sq = static_cast<float>(r_inner * r_inner);
  const float r_outer_sq = static_cast<float>(r_outer * r_outer);

  int x_start = 0;
  int y_start = 0;
  int x_end = 0;
  int y_end = 0;

  angle_to_xy(cx, cy, center_radius, angle_start, &x_start, &y_start);
  angle_to_xy(cx, cy, center_radius, angle_end, &x_end, &y_end);

  for (int y = cy - r_outer; y <= cy + r_outer; ++y) {
    for (int x = cx - r_outer; x <= cx + r_outer; ++x) {
      const float dx = static_cast<float>(x - cx);
      const float dy = static_cast<float>(y - cy);
      const float dist_sq = (dx * dx) + (dy * dy);
      if (dist_sq < r_inner_sq || dist_sq > r_outer_sq) {
        continue;
      }

      float angle = std::atan2f(dy, dx) * kRadToDeg;
      if (angle < 0.0f) {
        angle += 360.0f;
      }
      if (angle < kArcStartAngleDeg) {
        angle += 360.0f;
      }

      const bool in_arc = angle >= angle_start && angle <= angle_end;
      const float cap_start =
          (x - x_start) * (x - x_start) + (y - y_start) * (y - y_start);
      const float cap_end =
          (x - x_end) * (x - x_end) + (y - y_end) * (y - y_end);
      const float cap_limit_sq = cap_radius * cap_radius;

      if (in_arc || cap_start <= cap_limit_sq || cap_end <= cap_limit_sq) {
        d.draw_pixel_at(x, y, color);
      }
    }
  }
}

static const char *mode_text_for_action(HvacAction action, HvacMode mode) {
  switch (action) {
    case HvacAction::kHeating:
      return "Heating";
    case HvacAction::kCooling:
      return "Cooling";
    case HvacAction::kFan:
      return "Fan";
    case HvacAction::kIdle:
      return "Idle";
    case HvacAction::kOff:
      return "Off";
    default:
      break;
  }

  switch (mode) {
    case HvacMode::kOff:
      return "Off";
    case HvacMode::kHeat:
      return "Heating";
    case HvacMode::kCool:
      return "Cooling";
    case HvacMode::kHeatCool:
      return "Heat/Cool";
    case HvacMode::kFanOnly:
      return "Fan";
    case HvacMode::kDry:
      return "Dry";
    case HvacMode::kAuto:
      return "Auto";
    case HvacMode::kUnknown:
      return "Unknown";
    default:
      return "Unknown";
  }
}

static const char *get_setpoint_label_for_mode(HvacMode mode, HvacAction action) {
  if (action == HvacAction::kHeating || action == HvacAction::kCooling ||
      action == HvacAction::kFan || action == HvacAction::kIdle ||
      action == HvacAction::kOff) {
    return mode_text_for_action(action, mode);
  }

  return mode_text_for_action(HvacAction::kUnknown, mode);
}

static Color dot_color_for_current_temp(
    float current_temp,
    float setpoint,
    HvacMode mode,
    float min_t,
    float max_t) {
  if (std::isnan(current_temp) || std::isnan(setpoint)) {
    return kColorTrack;
  }

  const float clamped_current = clamp_setpoint(current_temp, min_t, max_t);
  const float clamped_setpoint = clamp_setpoint(setpoint, min_t, max_t);

  if (mode == HvacMode::kHeat) {
    const float low = std::min(clamped_current, clamped_setpoint);
    const float high = std::max(clamped_current, clamped_setpoint);
    if (clamped_current <= low + 1e-6f) {
      return kColorHeatLight;
    }
    if (clamped_current <= high + 1e-6f) {
      return kColorHeatDark;
    }
    return kColorTrack;
  }

  if (mode == HvacMode::kCool) {
    const float low = std::min(clamped_current, clamped_setpoint);
    const float high = std::max(clamped_current, clamped_setpoint);
    if (clamped_current >= high - 1e-6f) {
      return kColorCoolLight;
    }
    if (clamped_current >= low - 1e-6f) {
      return kColorCoolDark;
    }
    return kColorTrack;
  }

  if (mode == HvacMode::kFanOnly) {
    return kColorFan;
  }

  return kColorTrack;
}

static void draw_text(
    display::Display &d,
    int x,
    int y,
    font::Font *font,
    const char *text,
    Color color) {
  if (font == nullptr || text == nullptr) {
    return;
  }

  d.print(x, y, font, color, display::TextAlign::CENTER, text, Color(0, 0, 0));
}

static void format_temperature(
    char *out,
    size_t size,
    float temp,
    bool fahrenheit,
    const char *prefix,
    const char *suffix) {
  float value = temp;
  if (fahrenheit) {
    value = celsius_to_fahrenheit(value);
  }
  std::snprintf(out, size, "%s%.1f %s", prefix, value, suffix);
}

void render_thermostat(
    display::Display &d,
    const ThermostatState &state,
    const ThermostatFonts &fonts) {
  d.fill(Color(0, 0, 0));

  draw_arc_segment(d, kDefaultCenterX, kDefaultCenterY, kDefaultInnerRadius,
                   kDefaultOuterRadius, kArcStartAngleDeg, kArcEndAngleDeg,
                   kColorTrack);

  std::array<ArcSegment, 3> segments{};
  int segment_count = 0;

  if (state.hvac_mode == HvacMode::kHeat) {
    segment_count = compute_heat_segments(
        state.current_temp, state.local_setpoint, state.min_temp, state.max_temp,
        segments.data());
  } else if (state.hvac_mode == HvacMode::kCool) {
    segment_count = compute_cool_segments(
        state.current_temp, state.local_setpoint, state.min_temp, state.max_temp,
        segments.data());
  } else if (state.hvac_mode == HvacMode::kFanOnly) {
    segment_count = 1;
    segments[0] = {kArcStartAngleDeg, kArcEndAngleDeg, kColorFan};
  }

  for (int i = 0; i < segment_count; ++i) {
    const ArcSegment &segment = segments[i];
    draw_arc_segment(d, kDefaultCenterX, kDefaultCenterY, kDefaultInnerRadius,
                     kDefaultOuterRadius, segment.start_angle,
                     segment.end_angle, segment.color);
  }

  if (!std::isnan(state.current_temp)) {
    int x = 0;
    int y = 0;
    const float angle = temp_to_angle(
        state.current_temp, state.min_temp, state.max_temp);
    angle_to_xy(kDefaultCenterX, kDefaultCenterY,
                (static_cast<float>(kDefaultInnerRadius + kDefaultOuterRadius) / 2.0f),
                angle, &x, &y);
    d.filled_circle(x, y, 9,
                    dot_color_for_current_temp(state.current_temp,
                                              state.local_setpoint,
                                              state.hvac_mode,
                                              state.min_temp,
                                              state.max_temp));
  }

  if (!std::isnan(state.local_setpoint) && state.hvac_mode != HvacMode::kOff) {
    int x = 0;
    int y = 0;
    const float angle = temp_to_angle(
        state.local_setpoint, state.min_temp, state.max_temp);
    angle_to_xy(kDefaultCenterX, kDefaultCenterY,
                (static_cast<float>(kDefaultInnerRadius + kDefaultOuterRadius) / 2.0f),
                angle, &x, &y);

    if (state.hvac_mode == HvacMode::kHeat) {
      d.filled_circle(x, y, 18, kColorHeatDark);
    } else if (state.hvac_mode == HvacMode::kCool) {
      d.filled_circle(x, y, 18, kColorCoolDark);
    } else if (state.hvac_mode == HvacMode::kFanOnly) {
      d.filled_circle(x, y, 18, kColorFan);
    } else {
      d.filled_circle(x, y, 18, kColorTrack);
    }
  }

  draw_text(d, kDefaultCenterX, 20, fonts.mode,
            get_setpoint_label_for_mode(state.hvac_mode, state.hvac_action),
            kColorText);

  if (!std::isnan(state.current_temp) && fonts.temp != nullptr) {
    char text[24];
    format_temperature(text, sizeof(text), state.current_temp,
                      state.display_fahrenheit, "", "deg");
    draw_text(d, kDefaultCenterX, 72, fonts.temp, text, kColorText);
  }

  if (!std::isnan(state.current_temp) && !std::isnan(state.local_setpoint) &&
      state.hvac_mode != HvacMode::kOff && fonts.setpoint != nullptr) {
    char text[32];
    format_temperature(text, sizeof(text), state.local_setpoint,
                      state.display_fahrenheit, "Set ", "deg");
    draw_text(d, kDefaultCenterX, 155, fonts.setpoint, text, kColorTextMuted);
  }
}

void render_no_connection(display::Display &d, const ThermostatFonts &fonts) {
  d.fill(Color(0, 0, 0));
  draw_text(d, kDefaultCenterX, 58, fonts.error, "?", kColorText);
  draw_text(d, kDefaultCenterX, 150, fonts.mode, "No connection", kColorTextMuted);
}

}  // namespace m5dial_thermostat
}  // namespace esphome
