# M5Stack Dial Thermostat -- Spec

## Overview

Build an ESPHome external component for the M5Stack Dial that
displays a thermostat UI and controls an existing Home Assistant
`climate` entity. See `AGENTS.md` for file structure, coding
standards, and workflow.

---

## Device Hardware

- **MCU:** ESP32-S3 (M5StampS3)
- **Display:** 1.28" round TFT, 240x240px, driver GC9A01A, SPI
- **Input:** Rotary encoder (GPIO40/41) + push button (GPIO42)
- **Secondary button:** GPIO46 (hold -- reserved, no-op for now)
- **I2C:** SDA GPIO11, SCL GPIO12
- **SPI:** MOSI GPIO5, CLK GPIO6
- **Display pins:** CS GPIO7, RESET GPIO8, DC GPIO4
- **Backlight:** GPIO9 (LEDC PWM)
- **Buzzer:** GPIO3 (LEDC PWM)
- **RTC:** PCF8563, I2C address 0x51
- **Platform:** `esp32-s3`, framework `esp-idf`, flash mode `dio`

---

## User Configuration (thermostat.yaml)

The user configures standard ESPHome boilerplate, M5 Dial hardware
(SPI bus and display), and the component:

```yaml
esphome:
  name: thermostat-dial
  friendly_name: Thermostat

esp32:
  board: esp32-s3-devkitc-1
  variant: esp32s3
  framework:
    type: esp-idf

psram:
  mode: octal
  speed: 80MHz

api:

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

ota:
  - platform: esphome

# --- M5 Dial hardware (fixed pins) ---
spi:
  mosi_pin: GPIO5
  clk_pin: GPIO6

display:
  - platform: ili9xxx
    id: m5dial_display
    model: GC9A01A
    cs_pin: GPIO7
    dc_pin: GPIO4
    reset_pin: GPIO8
    invert_colors: true
    update_interval: never  # component controls redraws

# --- Component ---
external_components:
  - source:
      type: local
      path: components

m5dial_thermostat:
  entity_id: climate.my_thermostat
  display_id: m5dial_display
  # Optional (shown with defaults):
  # active_brightness: 255
  # idle_brightness: 50
  # idle_timeout: 30s
  # enable_sounds: true
  # comms_timeout: 30s
```

No `substitutions`, `globals`, `sensor`, `script`, or
`binary_sensor` blocks are needed. The component's `__init__.py`
auto-creates LEDC outputs (backlight, buzzer), RTTTL, fonts, and
the units select entity. Encoder and button GPIOs are hardcoded
in C++ (fixed M5 Dial pins).

---

## Component Registration (`__init__.py`)

### Metadata

```python
DEPENDENCIES = ["api"]
CODEOWNERS = ["@yourname"]
```

### CONFIG_SCHEMA

```python
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(M5DialThermostat),
    cv.Required(CONF_ENTITY_ID): cv.string,
    cv.Required(CONF_DISPLAY_ID): cv.use_id(
        display.Display
    ),
    cv.Optional(
        CONF_ACTIVE_BRIGHTNESS, default=255
    ): cv.int_range(min=0, max=255),
    cv.Optional(
        CONF_IDLE_BRIGHTNESS, default=50
    ): cv.int_range(min=0, max=255),
    cv.Optional(
        CONF_IDLE_TIMEOUT, default="30s"
    ): cv.positive_time_period_milliseconds,
    cv.Optional(
        CONF_ENABLE_SOUNDS, default=True
    ): cv.boolean,
    cv.Optional(
        CONF_COMMS_TIMEOUT, default="30s"
    ): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA)
```

### `to_code(config)` responsibilities

1. Create `M5DialThermostat` via `cg.new_Pvariable()`
2. `await cg.register_component(var, config)`
3. Look up display: `await cg.get_variable(display_id)`
4. Auto-create LEDC output for backlight (GPIO9)
5. Auto-create LEDC output + RTTTL for buzzer (GPIO3)
6. Auto-create 4 fonts via `gfonts://Roboto` (16, 20, 48, 72px)
   - Use `bpp: 4` for anti-aliased rendering
   - Single shared glyph set (digits, degree sign, mode labels,
     `?`, space) -- ESP32-S3 has ample flash/PSRAM
7. Auto-create `UnitSelect` (options: `"celsius"`, `"fahrenheit"`)
8. Wire all sub-components to main class via `set_*()` calls

---

## Home Assistant Integration

The device is a **remote control** for an existing HA `climate`
entity. It reads state and writes commands; it does not implement
thermostat logic itself.

### State subscribed from HA

All subscriptions use `subscribe_homeassistant_state()` in
`setup()` with `StringRef` callbacks (zero-allocation, current
API). Target the single configured `entity_id`:

| Attribute | Member | Type | Notes |
|---|---|---|---|
| (main state) | `hvac_mode_` | HvacMode enum | Parse from string |
| `current_temperature` | `current_temp_` | float | Reported room temperature. NaN if unavail |
| `temperature` | `target_temp_` | float | Setpoint. May be NaN if climate device is off |
| `hvac_action` | `hvac_action_` | HvacAction enum | Parse string |
| `hvac_modes` | `supported_modes_[]` | HvacMode[] | Parse once |
| `min_temp` | `min_temp_` | float | Default 15 |
| `max_temp` | `max_temp_` | float | Default 30 |
| `target_temp_step` | `temp_step_` | float | Default 0.5 |

Each callback updates the relevant member, records
`this->last_ha_update_ = millis()`, sets `this->comms_ok_ = true`,
and sets `this->needs_redraw_ = true`.

The `target_temp_` callback additionally syncs
`this->local_setpoint_` only if `this->local_setpoint_dirty_` is
false (no pending unconfirmed local change).

### Parsing HVAC strings

Use enums to avoid runtime heap allocation:

```cpp
enum class HvacMode : uint8_t {
  kOff, kHeat, kCool, kHeatCool,
  kFanOnly, kAuto, kDry, kUnknown
};
enum class HvacAction : uint8_t {
  kOff, kHeating, kCooling, kIdle,
  kFan, kDrying, kUnknown
};

// Parse at callback time from StringRef
static HvacMode parse_hvac_mode(const char *s);
static HvacAction parse_hvac_action(const char *s);

// For display text, return const char* literals
static const char *action_to_label(
    HvacAction action, HvacMode mode);
```

For `supported_modes_` (parsed once from `hvac_modes` attribute,
which arrives as a comma-separated string):

```cpp
static constexpr int kMaxSupportedModes = 8;
HvacMode supported_modes_[kMaxSupportedModes];
int supported_modes_count_{0};
```

### Commands sent to HA

- **Setpoint change:** `climate.set_temperature` with
  `temperature: local_setpoint_`. Debounced 500ms after last
  encoder tick using `this->set_timeout()` (see Debounce section).
- **Mode change:** `climate.set_hvac_mode` with
  `hvac_mode: next_mode`. Fired immediately on button press.

---

## Component State

All mutable state lives as `protected` member variables on the
component class (use `this->` prefix for all access):

| Member | Type | Purpose |
|---|---|---|
| `current_temp_` | float | Latest from HA |
| `target_temp_` | float | Confirmed setpoint from HA |
| `local_setpoint_` | float | Local working setpoint |
| `local_setpoint_dirty_` | bool | Awaiting HA confirmation |
| `hvac_mode_` | HvacMode | Latest from HA |
| `hvac_action_` | HvacAction | Latest from HA. Note that off and idle are functionally similar and dependent on the model of climate system |
| `supported_modes_[]` | HvacMode[8] | From `hvac_modes` attr |
| `supported_modes_count_` | int | Number of valid entries |
| `min_temp_` | float | From HA entity |
| `max_temp_` | float | From HA entity |
| `temp_step_` | float | From HA `target_temp_step` |
| `display_fahrenheit_` | bool | From UnitSelect |
| `last_ha_update_` | uint32_t | millis() of last HA update |
| `last_interaction_` | uint32_t | millis() of last input |
| `comms_ok_` | bool | False if HA timeout exceeded |
| `needs_redraw_` | bool | Dirty flag for display |

### Sub-component pointers (set via codegen)

| Member | Type | Source |
|---|---|---|
| `display_` | `display::Display*` | User YAML `display_id` |
| `backlight_` | `output::FloatOutput*` | Auto-created LEDC |
| `rtttl_` | `rtttl::Rtttl*` | Auto-created |
| `font_mode_` | `font::Font*` | Auto-created 16px |
| `font_setpoint_` | `font::Font*` | Auto-created 20px |
| `font_temp_` | `font::Font*` | Auto-created 48px |
| `font_error_` | `font::Font*` | Auto-created 72px |

---

## Display Redraw Strategy

Configure the display with `update_interval: never` so the
component has full control over refresh timing.

Set `this->needs_redraw_ = true` whenever state changes (HA
callbacks, encoder ticks, button presses, comms timeout). In
`loop()`, check the flag and call `this->display_->update()` once
per iteration at most, then clear the flag.

The display's writer callback (set in `setup()` via
`this->display_->set_writer(...)`) calls the rendering functions
from `thermostat_ui.h`.

---

## Debounce (Setpoint Send)

Use ESPHome's built-in `set_timeout()` instead of manual flags.
Each encoder tick restarts the timeout:

```cpp
void on_encoder_tick_(int direction) {
  // ... adjust local_setpoint_ ...
  this->set_timeout("send_setpoint", 500, [this]() {
    this->send_setpoint_to_ha_();
  });
}
```

`set_timeout()` automatically cancels any previous timeout with
the same name. When the timeout fires, send the service call and
set `this->local_setpoint_dirty_ = false`.

---

## Lost Communications

In `loop()`, check
`millis() - this->last_ha_update_ > this->comms_timeout_ms_`. If
true, set `this->comms_ok_ = false` and trigger redraw. When comms
restore (any subscription callback fires), set
`this->comms_ok_ = true`.

**When `comms_ok_ == false`:**
- Display the lost comms screen (large centered `?`, see UI)
- All encoder and button input is ignored

**NaN setpoint vs. lost comms:** When `hvac_mode_` is `kOff` or `kIdle`, HA
may not provide `temperature`. This is normal, not a comms error.
`comms_ok_` may still be true. Handle separately: omit setpoint
dot and text; ignore encoder input while `target_temp_` is NaN.

---

## Display Units (C / F)

HA always sends degrees C. The component exposes a `select` entity
to HA (options: `"celsius"`, `"fahrenheit"`) that controls
`this->display_fahrenheit_`. All internal logic and HA service
calls remain in C; conversion is display-only.

### UnitSelect class

```cpp
class UnitSelect : public select::Select,
                   public Component {
 public:
  void set_parent(M5DialThermostat *p) {
    this->parent_ = p;
  }
 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    this->parent_->set_fahrenheit(
        value == "fahrenheit");
  }
  M5DialThermostat *parent_{nullptr};
};
```

Registered in `__init__.py` via
`await select.new_select(config, options=[...])`.

Conversion: `F = C * 9.0 / 5.0 + 32`

---

## Backlight / Auto-Dimming

- On boot: set backlight to `active_brightness_`
- On any user input: set to `active_brightness_`, record
  `this->last_interaction_ = millis()`
- In `loop()`: if idle time exceeded, set to `idle_brightness_`

The `__init__.py` auto-creates a LEDC output on GPIO9. The
component stores the pointer as `this->backlight_` and calls
`this->backlight_->set_level(brightness / 255.0f)`.

---

## Buzzer / Sounds

The `__init__.py` auto-creates a LEDC output on GPIO3 and an
RTTTL component wired to it. If `this->enable_sounds_` is false,
skip all `play()` calls.

Three tones:
- **Rotary up:** `"up:d=64,o=6,b=255:c"`
- **Rotary down:** `"down:d=64,o=4,b=255:c"`
- **Button click:** `"click:d=64,o=5,b=255:c,p,c"`

---

## Rotary Encoder (Direct GPIO)

GPIO40 (pin A), GPIO41 (pin B). Handled directly in C++ using
GPIO reads in `loop()` or ISR-based quadrature decoding. No
separate ESPHome `rotary_encoder` component needed.

On each tick:
1. If `!this->comms_ok_` or `isnan(this->target_temp_)`: return
2. Adjust `this->local_setpoint_` by `+/- this->temp_step_`,
   clamped to `[this->min_temp_, this->max_temp_]`
3. Set `this->local_setpoint_dirty_ = true`
4. Reset backlight / `this->last_interaction_`
5. Play rotary sound
6. Set `this->needs_redraw_ = true`
7. Call `this->set_timeout("send_setpoint", 500, ...)` to
   debounce the HA service call

---

## Mode Cycling (Button Press)

GPIO42, short press. Handled directly in C++ via GPIO polling
with debounce in `loop()`. No separate ESPHome binary_sensor
needed.

On press:
1. If `!this->comms_ok_`: return
2. Find current `hvac_mode_` in `supported_modes_[]`; advance
   to next (wrapping)
3. Play click sound
4. Reset backlight
5. Call `climate.set_hvac_mode` immediately

---

## Lifecycle Methods

### `setup()`
1. Configure encoder GPIOs (GPIO40/41) as inputs with pullups
2. Configure button GPIO (GPIO42) as input with pullup
3. Subscribe to all HA entity attributes (see table above)
4. Set display writer callback
5. Set initial backlight brightness

### `loop()`
1. Read encoder state, call tick handler on change
2. Read button state, call press handler on change (debounced)
3. Check comms timeout
4. Check idle timeout for backlight dimming
5. If `this->needs_redraw_`: call `this->display_->update()`

### `dump_config()`
Log entity_id, display pointer, brightness settings, sound
enable, comms timeout.

---

## UI Design

### Arc Geometry

The arc spans **300 degrees**, gap at the bottom (6 o'clock).
Temperature maps linearly from `min_temp_` to `max_temp_`.

**Coordinate convention: 0 deg = 3 o'clock, increasing
clockwise.** (Matches ESPHome's `line_at_angle()` and standard
`cos`/`sin` in screen coordinates where Y increases downward.)

- Arc start: **120 deg** (7 o'clock) = `min_temp_`
- Arc end: **420 deg** (= 60 mod 360, 5 o'clock) = `max_temp_`
- Direction: clockwise through 12 o'clock (the long way, 300 deg)
- **Document this convention explicitly in comments -- it is the
  most likely source of bugs**

Work in the `[120, 420]` range internally to avoid wraparound
arithmetic; mod by 360 only when converting to screen
coordinates.

```cpp
// Returns angle in [120, 420]
float temp_to_angle(float temp,
                    float min_temp,
                    float max_temp) {
  float frac = (temp - min_temp)
             / (max_temp - min_temp);
  return 120.0f + frac * 300.0f;
}
```

### Arc Drawing

ESPHome has no `filled_arc()` function. Implement a custom
`draw_arc_segment()` in `thermostat_ui.cpp` using pixel-by-pixel
drawing with polar coordinate rejection:

For each pixel in the bounding box `[cx-r_outer, cy-r_outer]` to
`[cx+r_outer, cy+r_outer]`:
1. Compute `dx = x - cx`, `dy = y - cy`
2. `r_sq = dx*dx + dy*dy`
3. Reject if outside `[r_inner^2, r_outer^2]`
4. `angle = atan2f(dy, dx) * 180/PI`, normalize to `[0, 360)`
5. Convert to extended range `[120, 420]` for comparison
6. Accept if within `[angle_start, angle_end]`

**Rounded end caps:** Each arc segment has semicircular end caps
with diameter equal to the arc width (`r_outer - r_inner`).
Center each cap on the arc centerline radius at the start/end
angle. Integrate into the pixel loop: if a pixel is outside the
angular range but within distance `cap_radius` of either end-cap
center, accept it.

```cpp
void draw_arc_segment(
    display::Display &d,
    int cx, int cy,
    int r_inner, int r_outer,
    float angle_start, float angle_end,
    Color color);
```

### Arc Layers (drawn bottom to top)

Center: (120, 120). Outer radius: ~108px. Ring width: ~18px.
Inner radius: ~90px.

**Layer 1 -- Gray track:** full 300 deg arc, always drawn.

**Layer 2 -- Colored fill:** mode-dependent, see Color Logic.
Omitted in off mode.

**Layer 3 -- Current temp dot:**
- Filled circle at arc centerline for `current_temp_`
- Radius ~9px (half ring width)
- Color: same as arc segment underneath it
- Omitted if `current_temp_` is NaN

**Layer 4 -- Setpoint dot:**
- Filled circle at arc centerline for `local_setpoint_`
- Radius ~18px (full ring width)
- Color: dark mode color
- Omitted if setpoint unavailable (NaN or off mode)

### Arc Color Logic

**Off:** gray track only.

**Heat:**
- Light salmon: `min_temp_` to `min(current, setpoint)`
- Dark salmon: band between `current_temp_` and
  `local_setpoint_` (the active gap)
- Gray: `max(current, setpoint)` to `max_temp_`

**Cool** (mirror of heat, from `max_temp_` inward):
- Light blue: `max_temp_` to `max(current, setpoint)`
- Dark blue: band between `local_setpoint_` and `current_temp_`
- Gray: `min(current, setpoint)` to `min_temp_`

**Fan:** full arc in light lavender, no variation.

```cpp
// Named color constants in thermostat_ui.h
constexpr Color kColorTrack =
    Color(0xCC, 0xCC, 0xCC);
constexpr Color kColorHeatLight =
    Color(0xFF, 0xCB, 0xA4);
constexpr Color kColorHeatDark =
    Color(0xE8, 0x85, 0x5A);
constexpr Color kColorCoolLight =
    Color(0xAD, 0xD8, 0xF0);
constexpr Color kColorCoolDark =
    Color(0x1E, 0x90, 0xFF);
constexpr Color kColorFan =
    Color(0xC8, 0xA8, 0xE9);
```

### Arc Segment Computation

Pure logic for testability. Returns segments to draw:

```cpp
struct ArcSegment {
  float start_angle;
  float end_angle;
  Color color;
};

// Returns segment count (max 3)
int compute_heat_segments(
    float current, float setpoint,
    float min_t, float max_t,
    ArcSegment out[3]);
int compute_cool_segments(
    float current, float setpoint,
    float min_t, float max_t,
    ArcSegment out[3]);
```

### Center Text

Layout, top to bottom, all horizontally centered:

1. **Mode label** (~16px, arc color): from `hvac_action_` if
   actionable, else `hvac_mode_`. Labels: `"Off"`, `"Heating"`,
   `"Cooling"`, `"Fan"`, `"Idle"`
2. **Current temp** (~48px, white): e.g. `"21 deg"` -- display
   unit applied, formatted via `snprintf` into stack buffer
3. **Setpoint** (~20px, subdued gray): e.g. `"Set 22.5 deg"` --
   omitted if unavailable

### Lost Comms Screen

When `comms_ok_ == false`: black fill, large `?` centered
(~72px), small `"No connection"` below.

---

## Rendering Interface (`thermostat_ui.h`)

Decouple rendering from component state using plain structs:

```cpp
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
  font::Font *mode;      // 16px
  font::Font *setpoint;  // 20px
  font::Font *temp;      // 48px
  font::Font *error;     // 72px
};

void render_thermostat(
    display::Display &d,
    const ThermostatState &state,
    const ThermostatFonts &fonts);

void render_no_connection(
    display::Display &d,
    const ThermostatFonts &fonts);
```

The component constructs `ThermostatState` from its members in
the display writer callback and passes it to these functions.

---

## Fonts

Four sizes, Roboto via `gfonts://Roboto`. Auto-created by
`__init__.py`. Use `bpp: 4` for anti-aliased rendering on the
color display.

| Size | ID | Use |
|---|---|---|
| 16px | `font_mode_` | Mode label |
| 20px | `font_setpoint_` | Setpoint value |
| 48px | `font_temp_` | Current temperature |
| 72px | `font_error_` | Lost comms `?` |

Single shared glyph set is simplest given ESP32-S3 resources.
If flash becomes tight, per-size glyph sets are possible (72px
needs only `?` and space).

---

## Out of Scope

- No thermostat scheduling or automation logic
- No RFID
- No haze/radial gradient background
- No multi-zone support

---

## Deliverables

1. `components/m5dial_thermostat/` -- complete external component
2. `thermostat.yaml` -- user config, flashable
3. `tests/` -- unit tests for all pure logic

Heavily comment angle math, arc color logic, and the
`local_setpoint_dirty_` sync pattern.
