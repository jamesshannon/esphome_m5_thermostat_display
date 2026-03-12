# M5 Dial Thermostat External Component

This project is an ESPHome external component for the M5 Dial that renders a
thermostat-style UI on the built-in round display and controls an existing
Home Assistant climate entity.

The component reads rotary encoder and button GPIO state directly, mirrors HA
state (current temp, target temp, mode, action, supported modes), and sends
Home Assistant service calls when users adjust temperature or cycle modes.

## What this component does

- Renders a custom thermostat face on `GC9A01A` (`ILI9XXX`) display.
- Subscribes to Home Assistant state updates for a selected climate entity.
- Lets users change setpoint with the rotary encoder.
- Lets users cycle HVAC modes with the center button.
- Shows connection status and falls back to offline/idle UI when HA is not
  responding.
- Supports Celsius/Fahrenheit unit switching through a generated
  `select` entity.
- Creates all required companion objects automatically in codegen:
  - Backlight LEDC output
  - Buzzer LEDC output + RTTTL player
  - Four Roboto gfonts (`16/20/48/72`, `bpp: 4`)
  - Unit select entity (`celsius`, `fahrenheit`)

## Project structure

- `components/m5dial_thermostat/__init__.py` - ESPHome codegen and component
  registration.
- `components/m5dial_thermostat/m5dial_thermostat.h` - component class,
  unit select class, and control/state members.
- `components/m5dial_thermostat/m5dial_thermostat.cpp` - HA subscriptions,
  GPIO polling, send/receive flow, redraw and timeout logic.
- `components/m5dial_thermostat/thermostat_ui.h` - plain-data UI types,
  color constants, and pure math/render declarations.
- `components/m5dial_thermostat/thermostat_ui.cpp` - arc math and rendering
  helpers.
- `tests/` - isolated unit tests with ESPHome stubs for pure logic validation.
- `thermostat.yaml` - example full user-facing ESPHome configuration.

## File roles

- `thermostat.yaml` is the user entrypoint for flashing the device.
- `components/...` is the actual runtime component implementation.
- `tests/` should stay clean, fast, and independent of ESPHome headers.
- `thermostat-debug.yaml` is a local debug-friendly config with `DEBUG_TEST`.

## Usage

Copy or mount this repo in your ESPHome config path and point
`external_components` to the `components` directory. Then use `thermostat.yaml`
(or adapt it) as your primary config.

```yaml
---
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

# M5 Dial hardware (fixed pins)
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
    update_interval: never

external_components:
  - source:
      type: local
      path: components

m5dial_thermostat:
  entity_id: climate.my_thermostat
  display_id: m5dial_display
  # Optional settings
  # active_brightness: 255
  # idle_brightness: 50
  # idle_timeout: 30s
  # enable_sounds: true
  # comms_timeout: 30s
```

If you hit an error like:

```
Error reading file secrets.yaml: [Errno 2] No such file or directory
```

use the debug config:

```bash
esphome run thermostat-debug.yaml --device /dev/<your-usb-serial-port>
```

`thermostat-debug.yaml`:
- hardcodes Wi-Fi credentials directly (no `!secret` block)
- enables `DEBUG_TEST` at build time so it works without Home Assistant
  subscriptions
- keeps the same hardware/component wiring

```yaml
---
esphome:
  name: thermostat-dial-debug
  friendly_name: Thermostat Debug

esp32:
  board: esp32-s3-devkitc-1
  variant: esp32s3
  framework:
    type: esp-idf
  platformio_options:
    build_flags:
      - -DDEBUG_TEST

psram:
  mode: octal
  speed: 80MHz

api:

wifi:
  ssid: "YOUR_WIFI_SSID"
  password: "YOUR_WIFI_PASSWORD"

ota:
  - platform: esphome

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
    update_interval: never

external_components:
  - source:
      type: local
      path: components

m5dial_thermostat:
  entity_id: climate.my_thermostat
  display_id: m5dial_display
  active_brightness: 255
  idle_brightness: 50
  idle_timeout: 30s
  enable_sounds: true
  comms_timeout: 30s
```

## Configuration options

- `entity_id` (required): Home Assistant climate entity ID, e.g.
  `climate.my_thermostat`.
- `display_id` (required): Display component ID, e.g. `m5dial_display`.
- `active_brightness` (optional, default `255`): LEDC backlight level while
  active.
- `idle_brightness` (optional, default `50`): LEDC backlight level when idle.
- `idle_timeout` (optional, default `30s`): Time before dimming the backlight
  after inactivity.
- `comms_timeout` (optional, default `30s`): Timeout before entering no-connection
  UI.
- `enable_sounds` (optional, default `true`): Enable beep feedback for UI
  interactions.

## Hardware assumptions

The component currently expects fixed GPIO wiring matching M5 Dial for:
- Rotary encoder channels: `GPIO40`, `GPIO41`
- Rotary button: `GPIO42`
- Hold/button support pin: `GPIO46`
- Backlight output: `GPIO9`
- Buzzer output: `GPIO3`
- Display SPI pins: `GPIO5`, `GPIO6`, `GPIO7`, `GPIO4`, `GPIO8`

## Development and testing

- Run tests:
  - `make test`
- Run lint:
  - `./.venv/bin/yamllint thermostat.yaml`

The unit tests exercise pure math/render helpers in `thermostat_ui.*` and are
standalone via `tests/esphome_stubs.h`.

## Notes

- The component depends on the ESPHome API integration (`api`) and Home
  Assistant for thermostat state control.
- The setup priority is `AFTER_CONNECTION` so HA subscriptions are initialized
  after API readiness.
