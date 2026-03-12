# AGENTS.md -- Project Instructions

Read `SPEC.md` fully before writing any code. Deliverables are an
ESPHome external component and a `thermostat.yaml`.

---

## Coding Standards

**C++:** Follow the
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
2-space indent, 80-char line limit. All numeric constants (radii,
colors, angles) must be named constants. Every function needs a brief
comment; non-trivial math (especially arc angles) needs inline
comments. Use `#pragma once`. Prefer `constexpr` over `#define`.
Prefix all member access with `this->` (e.g., `this->value_`).

**Python (`__init__.py`):** Follow PEP 8, snake_case naming.

**YAML:** Section comment headers required (see SPEC.md). The
user-facing `thermostat.yaml` includes standard ESPHome boilerplate
plus M5 Dial hardware config (SPI, display) and the component
declaration.

---

## Architecture

This is an ESPHome **external component** (not a legacy "custom
component"). All HA state subscriptions, input handling, and display
logic live in C++.

### Component class

The main class inherits from both `Component` (ESPHome lifecycle) and
`api::CustomAPIDevice` (HA state subscriptions and service calls):

```cpp
class M5DialThermostat : public Component,
                         public api::CustomAPIDevice {
```

### Key C++ APIs (from `CustomAPIDevice`)

- `subscribe_homeassistant_state(callback, entity_id, attribute)`
  -- subscribe to HA entity attributes. Callbacks use
  `void on_*(StringRef state)` (zero-allocation, current API).
- `call_homeassistant_service(name, data_map)` -- send service
  calls to HA.

### Key Python codegen APIs (in `__init__.py`)

- `cg.register_component(var, config)` -- register the C++ object
  with ESPHome's lifecycle (setup/loop/dump_config).
- `cg.new_Pvariable(config[CONF_ID])` -- allocate C++ variable.
- `cg.add(var.set_foo(...))` -- call setter on C++ variable.
- `await cg.get_variable(config[CONF_DISPLAY_ID])` -- look up
  existing component by ID.

### Component metadata (`__init__.py`)

```python
DEPENDENCIES = ["api"]  # requires HA API connection
```

### Setup priority

HA-dependent components must initialize after the API connection:

```cpp
float get_setup_priority() const override {
  return setup_priority::AFTER_CONNECTION;
}
```

### User YAML

```yaml
m5dial_thermostat:
  entity_id: climate.my_thermostat
  display_id: m5dial_display
```

The user also configures `spi:` and `display:` sections for the M5
Dial hardware (fixed pins, copy-paste once). See SPEC.md for the
complete `thermostat.yaml`.

---

## File Structure

```
SPEC.md
AGENTS.md
components/
  m5dial_thermostat/
    __init__.py            # Config schema + codegen
                           #   (auto-creates LEDC, RTTTL, fonts,
                           #    select entity)
    m5dial_thermostat.h    # Component class + UnitSelect class
    m5dial_thermostat.cpp  # setup(), loop(), HA callbacks,
                           #   direct GPIO encoder/button
    thermostat_ui.h        # Pure math + drawing helpers +
                           #   ThermostatState struct
    thermostat_ui.cpp      # Drawing implementations
thermostat.yaml            # User config (hardware + component)
tests/
  esphome_stubs.h          # Stubs for Color, Display, Font
  test_arc_math.cpp
  test_color_logic.cpp
  test_unit_conversion.cpp
  Makefile                 # `make test`: g++ -std=c++17 ...
```

---

## Tests

### What to test

Unit tests cover all pure logic in `thermostat_ui.h`:
`temp_to_angle()`, `angle_to_xy()`, arc segment computation
(`compute_heat_segments`, `compute_cool_segments`),
`celsius_to_fahrenheit()`, `clamp_setpoint()`.

### Isolation strategy

- **Pure math functions** (angle calculations, temp conversions,
  segment computation) have zero ESPHome dependencies and are
  directly testable.
- **Drawing functions** (`draw_arc_segment`, `render_thermostat`)
  depend on `Display` and `Color`. Test with minimal stubs in
  `esphome_stubs.h`.
- The `ThermostatState` plain-data struct decouples drawing from
  component state, so rendering can be tested independently.

Tests must not depend on ESPHome headers -- stub required types in
`esphome_stubs.h`. A plain `assert()`-based runner is fine. All
tests must pass before committing.

---

## Embedded Best Practices

- **No heap allocation after `setup()`**: Use enums (not
  `std::string`) for HVAC mode/action. Use fixed-size arrays (not
  `std::vector`) for supported modes. Format display strings with
  `snprintf` into stack buffers.
- **Field visibility**: Use `protected` (not `private`) for class
  members, per ESPHome convention.
- **Member access**: Always use `this->member_` prefix.
- **Naming**: `lower_snake_case` for functions/variables,
  `UpperCamelCase` for classes, `UPPER_SNAKE_CASE` for
  global/namespace-scope constants, trailing underscore for
  protected/private fields.

---

## Workflow

- **Commit after every logical chunk** (component scaffolding, HA
  subscriptions, encoder logic, arc drawing, color logic, etc.).
  Short imperative commit messages.
- Before each commit: run `make test`; run
  `yamllint thermostat.yaml` if available.
- If ESPHome-specific behavior can't be verified locally, mark it
  `// UNVERIFIED:` with an explanation rather than silently
  guessing.
- If the spec is ambiguous, make a reasonable decision and note it
  with `// NOTE:`.
