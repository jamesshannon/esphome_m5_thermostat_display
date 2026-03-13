#include "thermostat_ui.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace
{
  constexpr float kTwoPi = 6.28318530717958647692f;
  constexpr float kDegToRad = kTwoPi / 360.0f;
  constexpr float kArcRasterStepDeg = 0.5f;
  constexpr float kArcAngleEpsilon = 1e-4f;
  constexpr int kTrigLutSize = 720;

  struct TrigLut
  {
    bool initialized{false};
    std::array<float, kTrigLutSize> sin_values{};
    std::array<float, kTrigLutSize> cos_values{};
  };

  TrigLut g_trig_lut;

  void ensure_trig_lut()
  {
    if (g_trig_lut.initialized)
    {
      return;
    }
    for (int i = 0; i < kTrigLutSize; ++i)
    {
      const float angle_deg = static_cast<float>(i) * kArcRasterStepDeg;
      const float angle_rad = angle_deg * kDegToRad;
      g_trig_lut.sin_values[i] = std::sin(angle_rad);
      g_trig_lut.cos_values[i] = std::cos(angle_rad);
    }
    g_trig_lut.initialized = true;
  }

  void sample_trig(float angle_deg, float *sine, float *cosine)
  {
    float normalized = std::fmod(angle_deg, 360.0f);
    if (normalized < 0.0f)
    {
      normalized += 360.0f;
    }

    int index = static_cast<int>(std::lround(normalized / kArcRasterStepDeg));
    if (index >= kTrigLutSize)
    {
      index = 0;
    }

    if (sine != nullptr)
    {
      *sine = g_trig_lut.sin_values[index];
    }
    if (cosine != nullptr)
    {
      *cosine = g_trig_lut.cos_values[index];
    }
  }
} // namespace

namespace esphome
{
  namespace m5dial_thermostat
  {

    // Convert temperature to the extended angle range [130, 410],
    // which avoids wraparound arithmetic for the 280° clockwise arc.
    float temp_to_angle(float temp, float min_temp, float max_temp)
    {
      if (std::isnan(min_temp) || std::isnan(max_temp) || max_temp <= min_temp)
      {
        return kArcStartAngleDeg;
      }

      const float clamped = clamp_setpoint(temp, min_temp, max_temp);
      const float span = max_temp - min_temp;
      const float ratio = (clamped - min_temp) / span;
      return kArcStartAngleDeg + (ratio * kArcSpanDeg);
    }

    void angle_to_xy(int cx, int cy, float radius, float angle_deg, int *x, int *y)
    {
      const float angle_rad = angle_deg * kDegToRad;
      if (x != nullptr)
      {
        *x = static_cast<int>(std::lround(static_cast<float>(cx) +
                                          (radius * std::cos(angle_rad))));
      }
      if (y != nullptr)
      {
        *y = static_cast<int>(std::lround(static_cast<float>(cy) +
                                          (radius * std::sin(angle_rad))));
      }
    }

    float clamp_setpoint(float value, float min_temp, float max_temp)
    {
      if (std::isnan(value))
      {
        return value;
      }
      return std::max(min_temp, std::min(max_temp, value));
    }

    float celsius_to_fahrenheit(float value)
    {
      return (value * 9.0f / 5.0f) + 32.0f;
    }

    // Returns overlay segments for heat mode (drawn on top of the gray track).
    //   out[0]: current segment [arc_start → current_temp] in kColorHeatLight.
    //           Always present when current_temp is valid.
    //   out[1]: active segment [current_temp → setpoint] in kColorHeatDark.
    //           Only when setpoint > current (more heating demanded).
    int compute_heat_segments(
        float current,
        float setpoint,
        float min_t,
        float max_t,
        ArcSegment out[2])
    {
      if (out == nullptr || max_t <= min_t || std::isnan(current) ||
          std::isnan(setpoint) || std::isnan(min_t) || std::isnan(max_t))
      {
        return 0;
      }

      const float cur_angle = temp_to_angle(current, min_t, max_t);

      int count = 0;
      // Current segment: left edge (min) → current_temp
      if (kArcStartAngleDeg < cur_angle - 1e-4f)
      {
        out[count++] = {kArcStartAngleDeg, cur_angle, kColorHeatLight};
      }
      // Active segment: current_temp → setpoint (only if heating demanded)
      if (setpoint > current + 1e-4f)
      {
        const float sp_angle = temp_to_angle(setpoint, min_t, max_t);
        if (cur_angle < sp_angle - 1e-4f)
        {
          out[count++] = {cur_angle, sp_angle, kColorHeatDark};
        }
      }
      return count;
    }

    // Returns overlay segments for cool mode (drawn on top of the gray track).
    //   out[0]: current segment [current_temp → arc_end] in kColorCoolLight.
    //           Always present when current_temp is valid.
    //   out[1]: active segment [setpoint → current_temp] in kColorCoolDark.
    //           Only when setpoint < current (more cooling demanded).
    int compute_cool_segments(
        float current,
        float setpoint,
        float min_t,
        float max_t,
        ArcSegment out[2])
    {
      if (out == nullptr || max_t <= min_t || std::isnan(current) ||
          std::isnan(setpoint) || std::isnan(min_t) || std::isnan(max_t))
      {
        return 0;
      }

      const float cur_angle = temp_to_angle(current, min_t, max_t);

      int count = 0;
      // Current segment: current_temp → right edge (max)
      if (cur_angle < kArcEndAngleDeg - 1e-4f)
      {
        out[count++] = {cur_angle, kArcEndAngleDeg, kColorCoolLight};
      }
      // Active segment: setpoint → current_temp (only if cooling demanded)
      if (setpoint < current - 1e-4f)
      {
        const float sp_angle = temp_to_angle(setpoint, min_t, max_t);
        if (sp_angle < cur_angle - 1e-4f)
        {
          out[count++] = {sp_angle, cur_angle, kColorCoolDark};
        }
      }
      return count;
    }

    // Draw a ring segment for [angle_start, angle_end] in extended [120, 420] space,
    // with rounded end caps based on the ring width. Rasterization uses a
    // precomputed sin/cos LUT to avoid per-pixel atan2f() work.
    void draw_arc_segment(
        display::Display &d,
        int cx,
        int cy,
        int r_inner,
        int r_outer,
        float angle_start,
        float angle_end,
        Color color)
    {
      if (angle_end < angle_start)
      {
        return;
      }
      ensure_trig_lut();

      const float center_radius = static_cast<float>(r_inner + r_outer) * 0.5f;
      const float cap_radius = static_cast<float>(r_outer - r_inner) * 0.5f;

      int x_start = 0;
      int y_start = 0;
      int x_end = 0;
      int y_end = 0;

      angle_to_xy(cx, cy, center_radius, angle_start, &x_start, &y_start);
      angle_to_xy(cx, cy, center_radius, angle_end, &x_end, &y_end);
      d.filled_circle(x_start, y_start, static_cast<int>(std::lround(cap_radius)),
                      color);
      d.filled_circle(x_end, y_end, static_cast<int>(std::lround(cap_radius)),
                      color);

      for (float angle = angle_start; angle <= angle_end + kArcAngleEpsilon;
           angle += kArcRasterStepDeg)
      {
        float sine = 0.0f;
        float cosine = 0.0f;
        sample_trig(angle, &sine, &cosine);
        const int x = static_cast<int>(std::lround(static_cast<float>(cx) +
                                                   center_radius * cosine));
        const int y = static_cast<int>(std::lround(static_cast<float>(cy) +
                                                   center_radius * sine));
        d.filled_circle(x, y, static_cast<int>(std::lround(cap_radius)), color);
      }
    }

    // Returns the active color for the setpoint dot border and arc active segment.
    static Color active_color_for_mode(HvacMode mode)
    {
      if (mode == HvacMode::kHeat)
        return kColorHeatDark;
      if (mode == HvacMode::kCool)
        return kColorCoolDark;
      if (mode == HvacMode::kFanOnly)
        return kColorFan;
      return kColorTrack;
    }

    const char *mode_text_for_action(HvacAction action, HvacMode mode)
    {
      switch (action)
      {
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

      switch (mode)
      {
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

    const char *get_setpoint_label_for_mode(HvacMode mode, HvacAction action)
    {
      if (action == HvacAction::kHeating || action == HvacAction::kCooling ||
          action == HvacAction::kFan || action == HvacAction::kIdle ||
          action == HvacAction::kOff)
      {
        return mode_text_for_action(action, mode);
      }

      return mode_text_for_action(HvacAction::kUnknown, mode);
    }

    static void draw_text(
        display::Display &d,
        int x,
        int y,
        display::BaseFont *font,
        const char *text,
        Color color,
        display::TextAlign align = display::TextAlign::CENTER)
    {
      if (font == nullptr || text == nullptr)
      {
        return;
      }

      d.print(x, y, font, color, align, text, kColorBackground);
    }

    static void format_temperature(
        char *out,
        size_t size,
        float temp,
        bool fahrenheit,
        const char *prefix,
        const char *suffix)
    {
      float value = temp;
      if (fahrenheit)
      {
        value = celsius_to_fahrenheit(value);
      }
      std::snprintf(out, size, "%s%.1f %s", prefix, value, suffix);
    }

    void render_thermostat(
        display::Display &d,
        const ThermostatState &state,
        const ThermostatFonts &fonts)
    {
      // White background
      d.fill(kColorBackground);

      // Layer 1: full track arc
      draw_arc_segment(d, kDefaultCenterX, kDefaultCenterY, kDefaultInnerRadius,
                       kDefaultOuterRadius, kArcStartAngleDeg, kArcEndAngleDeg,
                       kColorTrack);

      // Layer 2: mode-dependent color overlays on top of the track
      std::array<ArcSegment, 2> segments{};
      int segment_count = 0;

      if (state.hvac_mode == HvacMode::kHeat)
      {
        segment_count = compute_heat_segments(
            state.current_temp, state.local_setpoint,
            state.min_temp, state.max_temp, segments.data());
      }
      else if (state.hvac_mode == HvacMode::kCool)
      {
        segment_count = compute_cool_segments(
            state.current_temp, state.local_setpoint,
            state.min_temp, state.max_temp, segments.data());
      }
      else if (state.hvac_mode == HvacMode::kFanOnly)
      {
        segment_count = 1;
        segments[0] = {kArcStartAngleDeg, kArcEndAngleDeg, kColorFan};
      }

      for (int i = 0; i < segment_count; ++i)
      {
        draw_arc_segment(d, kDefaultCenterX, kDefaultCenterY, kDefaultInnerRadius,
                         kDefaultOuterRadius, segments[i].start_angle,
                         segments[i].end_angle, segments[i].color);
      }

      // Layer 3: current temp dot -- dark grey, r=7 (50% of arc width)
      if (!std::isnan(state.current_temp))
      {
        int x = 0;
        int y = 0;
        const float angle = temp_to_angle(
            state.current_temp, state.min_temp, state.max_temp);
        const float centerline =
            static_cast<float>(kDefaultInnerRadius + kDefaultOuterRadius) * 0.5f;
        angle_to_xy(kDefaultCenterX, kDefaultCenterY, centerline, angle, &x, &y);
        d.filled_circle(x, y, kCurrentDotRadius, kColorCurrentDot);
      }

      // Layer 4: setpoint dot -- white fill + 2 px border in active color.
      // Total r=14 equals arc width so the dot exactly fills the ring.
      if (!std::isnan(state.local_setpoint) && state.hvac_mode != HvacMode::kOff)
      {
        int x = 0;
        int y = 0;
        const float angle = temp_to_angle(
            state.local_setpoint, state.min_temp, state.max_temp);
        const float centerline =
            static_cast<float>(kDefaultInnerRadius + kDefaultOuterRadius) * 0.5f;
        angle_to_xy(kDefaultCenterX, kDefaultCenterY, centerline, angle, &x, &y);
        // Border circle in active color, then white fill on top
        d.filled_circle(x, y, kSetpointDotBorderRadius,
                        active_color_for_mode(state.hvac_mode));
        d.filled_circle(x, y, kSetpointDotFillRadius, kColorBackground);
      }

      // Center text layout: current temp vertically centered at y=120.
      // Mode label: bottom 10px above current temp top.
      // Setpoint: top 15px below current temp bottom.
      // Approximate half-heights: 48px font → 24, 16px → 8, 20px → 10.
      constexpr int kTempCenterY = 120;
      constexpr int kTempHalfH = 24; // ~48px font / 2
      // Mode label bottom = temp top - 10px gap
      constexpr int kModeLabelY = kTempCenterY - kTempHalfH - 10;
      // Setpoint top = temp bottom + 15px gap
      constexpr int kSetpointY = kTempCenterY + kTempHalfH + 15;

      const char *unit_suffix =
          state.display_fahrenheit ? "\xC2\xB0"
                                     "F"
                                   : "\xC2\xB0"
                                     "C";

      draw_text(d, kDefaultCenterX, kModeLabelY, fonts.mode,
                get_setpoint_label_for_mode(state.hvac_mode, state.hvac_action),
                kColorText, display::TextAlign::BOTTOM_CENTER);

      if (!std::isnan(state.current_temp) && fonts.temp != nullptr)
      {
        char text[24];
        format_temperature(text, sizeof(text), state.current_temp,
                           state.display_fahrenheit, "", unit_suffix);
        draw_text(d, kDefaultCenterX, kTempCenterY, fonts.temp,
                  text, kColorText);
      }

      if (!std::isnan(state.current_temp) && !std::isnan(state.local_setpoint) &&
          state.hvac_mode != HvacMode::kOff && fonts.setpoint != nullptr)
      {
        char text[32];
        format_temperature(text, sizeof(text), state.local_setpoint,
                           state.display_fahrenheit, "", unit_suffix);
        draw_text(d, kDefaultCenterX, kSetpointY, fonts.setpoint,
                  text, kColorTextMuted,
                  display::TextAlign::TOP_CENTER);
      }
    }

    void render_no_connection(display::Display &d, const ThermostatFonts &fonts)
    {
      d.fill(kColorBackground);
      draw_arc_segment(d, kDefaultCenterX, kDefaultCenterY, kDefaultInnerRadius,
                       kDefaultOuterRadius, kArcStartAngleDeg, kArcEndAngleDeg,
                       kColorTrack);
      draw_text(d, kDefaultCenterX, 58, fonts.error, "?", kColorText);
      draw_text(d, kDefaultCenterX, 150, fonts.mode, "No connection", kColorTextMuted);
    }

  } // namespace m5dial_thermostat
} // namespace esphome
